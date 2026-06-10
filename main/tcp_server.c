#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"

#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "nvs_flash.h"

#include "driver/gpio.h"
#include "driver/uart.h"

#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_adc/adc_oneshot.h"

#define PORT CONFIG_EXAMPLE_PORT
#define KEEPALIVE_IDLE CONFIG_EXAMPLE_KEEPALIVE_IDLE
#define KEEPALIVE_INTERVAL CONFIG_EXAMPLE_KEEPALIVE_INTERVAL
#define KEEPALIVE_COUNT CONFIG_EXAMPLE_KEEPALIVE_COUNT

#define EXAMPLE_ESP_WIFI_SSID CONFIG_ESP_WIFI_SSID
#define EXAMPLE_ESP_WIFI_PASS CONFIG_ESP_WIFI_PASSWORD
#define ESP_WIFI_SAE_MODE WPA3_SAE_PWE_BOTH
#define EXAMPLE_H2E_IDENTIFIER CONFIG_ESP_WIFI_PW_ID
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA2_PSK

#define WIFI_CONNECTED_BIT BIT0

#define UART_PORT UART_NUM_1
#define UART_TX_PIN GPIO_NUM_4
#define UART_RX_PIN GPIO_NUM_5
#define UART_BAUD_RATE 57600
#define UART_BUF_SIZE 1024

#define BATTERY_ADC_UNIT ADC_UNIT_1
#define BATTERY_ADC_CHANNEL ADC_CHANNEL_1 // GPIO1

#define R_TOP_OHMS 100000.0f
#define R_BOTTOM_OHMS 47000.0f
#define DIVIDER_RATIO ((R_TOP_OHMS + R_BOTTOM_OHMS) / R_BOTTOM_OHMS)
#define ADC_SAMPLES 32

#define LED1_GPIO GPIO_NUM_6 // leftmost
#define LED2_GPIO GPIO_NUM_7
#define LED3_GPIO GPIO_NUM_8
#define LED4_GPIO GPIO_NUM_2  // moved away from GPIO9
#define LED5_GPIO GPIO_NUM_10 // rightmost

static const char *TAG = "wifi station";

static EventGroupHandle_t s_wifi_event_group;

static adc_oneshot_unit_handle_t adc_handle;
static adc_cali_handle_t adc_cali_handle;
static bool adc_calibrated = false;

static const gpio_num_t battery_leds[5] = {
    LED1_GPIO,
    LED2_GPIO,
    LED3_GPIO,
    LED4_GPIO,
    LED5_GPIO};

static void battery_leds_init(void)
{
    for (int i = 0; i < 5; i++)
    {
        gpio_reset_pin(battery_leds[i]);
        gpio_set_direction(battery_leds[i], GPIO_MODE_OUTPUT);
        gpio_set_level(battery_leds[i], 0);
    }
}

static void battery_led_set_binary(uint8_t value)
{
    gpio_set_level(LED1_GPIO, (value >> 4) & 1);
    gpio_set_level(LED2_GPIO, (value >> 3) & 1);
    gpio_set_level(LED3_GPIO, (value >> 2) & 1);
    gpio_set_level(LED4_GPIO, (value >> 1) & 1);
    gpio_set_level(LED5_GPIO, (value >> 0) & 1);
}

static void battery_led_set_level(uint8_t level)
{
    if (level > 5)
    {
        level = 5;
    }

    for (int i = 0; i < 5; i++)
    {
        gpio_set_level(battery_leds[i], i < level ? 1 : 0);
    }
}

static void battery_led_startup_test(void)
{
    const uint8_t pattern[] = {

        0b10000,  // Low level
        0b11000,
        0b11100,
        0b11110,
        0b11111, // High level
        0b10001  // Done with startup
    };

    for (int i = 0; i < sizeof(pattern); i++)
    {
        battery_led_set_binary(pattern[i]);
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

static uint8_t battery_voltage_to_level(float v)
{
    if (v >= 8.20f)
        return 5;
    if (v >= 7.90f)
        return 4;
    if (v >= 7.65f)
        return 3;
    if (v >= 7.40f)
        return 2;
    if (v >= 6.80f)
        return 1;

    return 0; // Future: critical low-battery flashing
}

static bool adc_calibration_init(void)
{
    adc_cali_curve_fitting_config_t cali_config = {
        .unit_id = BATTERY_ADC_UNIT,
        .chan = BATTERY_ADC_CHANNEL,
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };

    esp_err_t ret = adc_cali_create_scheme_curve_fitting(&cali_config, &adc_cali_handle);

    if (ret == ESP_OK)
    {
        ESP_LOGI(TAG, "ADC calibration enabled");
        return true;
    }

    ESP_LOGW(TAG, "ADC calibration not available");
    return false;
}

static void battery_adc_init(void)
{
    adc_oneshot_unit_init_cfg_t init_config = {
        .unit_id = BATTERY_ADC_UNIT,
    };

    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config, &adc_handle));

    adc_oneshot_chan_cfg_t chan_config = {
        .bitwidth = ADC_BITWIDTH_DEFAULT,
        .atten = ADC_ATTEN_DB_12,
    };

    ESP_ERROR_CHECK(adc_oneshot_config_channel(
        adc_handle,
        BATTERY_ADC_CHANNEL,
        &chan_config));

    adc_calibrated = adc_calibration_init();
}

static float battery_read_voltage(void)
{
    int raw_sum = 0;
    int mv_sum = 0;

    for (int i = 0; i < ADC_SAMPLES; i++)
    {
        int raw = 0;
        ESP_ERROR_CHECK(adc_oneshot_read(adc_handle, BATTERY_ADC_CHANNEL, &raw));
        raw_sum += raw;

        if (adc_calibrated)
        {
            int mv = 0;
            ESP_ERROR_CHECK(adc_cali_raw_to_voltage(adc_cali_handle, raw, &mv));
            mv_sum += mv;
        }

        vTaskDelay(pdMS_TO_TICKS(2));
    }

    float raw_avg = (float)raw_sum / ADC_SAMPLES;
    float adc_voltage = 0.0f;

    if (adc_calibrated)
    {
        float mv_avg = (float)mv_sum / ADC_SAMPLES;
        adc_voltage = mv_avg / 1000.0f;
    }
    else
    {
        adc_voltage = (raw_avg / 4095.0f) * 3.3f;
    }

    float battery_voltage = adc_voltage * DIVIDER_RATIO;

    ESP_LOGI(TAG,
             "raw=%.1f adc=%.3fV battery=%.3fV",
             raw_avg,
             adc_voltage,
             battery_voltage);

    return battery_voltage;
}

static void battery_monitor_task(void *arg)
{
    while (1)
    {
        float v = battery_read_voltage();
        uint8_t level = battery_voltage_to_level(v);

        battery_led_set_level(level);

        ESP_LOGI(TAG, "battery=%.3fV level=%u", v, level);

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

static void uart_init(void)
{
    const uart_config_t uart_config = {
        .baud_rate = UART_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    ESP_ERROR_CHECK(uart_driver_install(UART_PORT, UART_BUF_SIZE * 2, 0, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(UART_PORT, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(
        UART_PORT,
        UART_TX_PIN,
        UART_RX_PIN,
        UART_PIN_NO_CHANGE,
        UART_PIN_NO_CHANGE));

    ESP_LOGI(TAG, "UART init done (%d 8N1)", UART_BAUD_RATE);
}

static void do_retransmit(const int sock)
{
    int len;
    uint8_t tcp_buffer[128];
    uint8_t uart_buffer[128];

    while (1)
    {
        len = recv(sock, tcp_buffer, sizeof(tcp_buffer), MSG_DONTWAIT);

        if (len < 0)
        {
            if (errno != EWOULDBLOCK && errno != EAGAIN)
            {
                ESP_LOGE(TAG, "recv failed: errno %d", errno);
                return;
            }
        }
        else if (len == 0)
        {
            ESP_LOGW(TAG, "Connection closed");
            return;
        }
        else
        {
            ESP_LOGI(TAG, "TCP->UART %d bytes", len);
            uart_write_bytes(UART_PORT, (const char *)tcp_buffer, len);
        }

        int uart_len = uart_read_bytes(UART_PORT, uart_buffer, sizeof(uart_buffer), 0);

        if (uart_len > 0)
        {
            ESP_LOGI(TAG, "UART->TCP %d bytes", uart_len);

            int to_write = uart_len;
            while (to_write > 0)
            {
                int written = send(sock,
                                   uart_buffer + (uart_len - to_write),
                                   to_write,
                                   0);
                if (written < 0)
                {
                    ESP_LOGE(TAG, "send failed: errno %d", errno);
                    return;
                }

                to_write -= written;
            }
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

static void tcp_server_task(void *pvParameters)
{
    char addr_str[128];
    int addr_family = (int)pvParameters;
    int ip_protocol = 0;
    int keepAlive = 1;
    int keepIdle = KEEPALIVE_IDLE;
    int keepInterval = KEEPALIVE_INTERVAL;
    int keepCount = KEEPALIVE_COUNT;
    struct sockaddr_storage dest_addr;

    if (addr_family == AF_INET)
    {
        struct sockaddr_in *dest_addr_ip4 = (struct sockaddr_in *)&dest_addr;
        dest_addr_ip4->sin_addr.s_addr = htonl(INADDR_ANY);
        dest_addr_ip4->sin_family = AF_INET;
        dest_addr_ip4->sin_port = htons(PORT);
        ip_protocol = IPPROTO_IP;
    }

    int listen_sock = socket(addr_family, SOCK_STREAM, ip_protocol);

    if (listen_sock < 0)
    {
        ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
        vTaskDelete(NULL);
        return;
    }

    int opt = 1;
    setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    ESP_LOGI(TAG, "Socket created");

    int err = bind(listen_sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr));

    if (err != 0)
    {
        ESP_LOGE(TAG, "Socket unable to bind: errno %d", errno);
        goto CLEAN_UP;
    }

    ESP_LOGI(TAG, "Socket bound, port %d", PORT);

    err = listen(listen_sock, 1);

    if (err != 0)
    {
        ESP_LOGE(TAG, "Error occurred during listen: errno %d", errno);
        goto CLEAN_UP;
    }

    while (1)
    {
        ESP_LOGI(TAG, "Socket listening");

        struct sockaddr_storage source_addr;
        socklen_t addr_len = sizeof(source_addr);

        int sock = accept(listen_sock, (struct sockaddr *)&source_addr, &addr_len);

        if (sock < 0)
        {
            ESP_LOGE(TAG, "Unable to accept connection: errno %d", errno);
            break;
        }

        setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, &keepAlive, sizeof(int));
        setsockopt(sock, IPPROTO_TCP, TCP_KEEPIDLE, &keepIdle, sizeof(int));
        setsockopt(sock, IPPROTO_TCP, TCP_KEEPINTVL, &keepInterval, sizeof(int));
        setsockopt(sock, IPPROTO_TCP, TCP_KEEPCNT, &keepCount, sizeof(int));

        if (source_addr.ss_family == PF_INET)
        {
            inet_ntoa_r(((struct sockaddr_in *)&source_addr)->sin_addr,
                        addr_str,
                        sizeof(addr_str) - 1);
        }

        ESP_LOGI(TAG, "Socket accepted ip address: %s", addr_str);

        do_retransmit(sock);

        shutdown(sock, 0);
        close(sock);
    }

CLEAN_UP:
    close(listen_sock);
    vTaskDelete(NULL);
}

static void event_handler(void *arg,
                          esp_event_base_t event_base,
                          int32_t event_id,
                          void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
    {
        esp_wifi_connect();
    }
    else if (event_base == WIFI_EVENT &&
             event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        if (event_data != NULL)
        {
            wifi_event_sta_disconnected_t *disconn =
                (wifi_event_sta_disconnected_t *)event_data;

            ESP_LOGI(TAG, "Disconnected, reason=%d. Retrying...", disconn->reason);
        }
        else
        {
            ESP_LOGI(TAG, "Disconnected, retrying...");
        }

        esp_wifi_connect();
    }
    else if (event_base == IP_EVENT &&
             event_id == IP_EVENT_STA_GOT_IP)
    {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;

        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));

        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

static void wifi_init_sta(void)
{
    s_wifi_event_group = xEventGroupCreate();

    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();

    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;

    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT,
        ESP_EVENT_ANY_ID,
        &event_handler,
        NULL,
        &instance_any_id));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        IP_EVENT,
        IP_EVENT_STA_GOT_IP,
        &event_handler,
        NULL,
        &instance_got_ip));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = EXAMPLE_ESP_WIFI_SSID,
            .password = EXAMPLE_ESP_WIFI_PASS,
            .threshold.authmode = ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD,
            .sae_pwe_h2e = ESP_WIFI_SAE_MODE,
            .sae_h2e_identifier = EXAMPLE_H2E_IDENTIFIER,
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "wifi_init_sta finished");

    EventBits_t bits = xEventGroupWaitBits(
        s_wifi_event_group,
        WIFI_CONNECTED_BIT,
        pdFALSE,
        pdFALSE,
        portMAX_DELAY);

    if (bits & WIFI_CONNECTED_BIT)
    {
        ESP_LOGI(TAG, "connected to ap SSID:%s", EXAMPLE_ESP_WIFI_SSID);
    }
    else
    {
        ESP_LOGE(TAG, "UNEXPECTED EVENT");
    }
}

void app_main(void)
{
    esp_err_t ret = nvs_flash_init();

    if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
        ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }
    else
    {
        ESP_ERROR_CHECK(ret);
    }

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    ESP_LOGI(TAG, "ESP_WIFI_MODE_STA");

    battery_leds_init();
    battery_adc_init();

    battery_led_startup_test();

    uart_init();
    wifi_init_sta();

    xTaskCreate(battery_monitor_task, "battery_monitor", 4096, NULL, 5, NULL);
    xTaskCreate(tcp_server_task, "tcp_server", 4096, (void *)AF_INET, 5, NULL);
}