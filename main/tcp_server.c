#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include <stdint.h>

#define LED1_GPIO GPIO_NUM_6 // leftmost / MSB
#define LED2_GPIO GPIO_NUM_7
#define LED3_GPIO GPIO_NUM_8
#define LED4_GPIO GPIO_NUM_9
#define LED5_GPIO GPIO_NUM_10 // rightmost / LSB

static const gpio_num_t led_pins[5] = {
    LED1_GPIO,
    LED2_GPIO,
    LED3_GPIO,
    LED4_GPIO,
    LED5_GPIO};

static void set_led_binary(uint8_t value)
{
    // Display as normal binary: MSB on left, LSB on right
    gpio_set_level(LED1_GPIO, (value >> 4) & 0x01);
    gpio_set_level(LED2_GPIO, (value >> 3) & 0x01);
    gpio_set_level(LED3_GPIO, (value >> 2) & 0x01);
    gpio_set_level(LED4_GPIO, (value >> 1) & 0x01);
    gpio_set_level(LED5_GPIO, (value >> 0) & 0x01);
}

static void led_test_task(void *arg)
{
    uint8_t count = 0;

    while (1)
    {
        set_led_binary(count);

        count = (count + 1) & 0x1F; // 0..31

        vTaskDelay(pdMS_TO_TICKS(250));
    }
}

void app_main(void)
{
    for (int i = 0; i < 5; i++)
    {
        gpio_reset_pin(led_pins[i]);
        gpio_set_direction(led_pins[i], GPIO_MODE_OUTPUT);
        gpio_set_level(led_pins[i], 0);
    }

    xTaskCreate(led_test_task, "led_test", 2048, NULL, 5, NULL);
}
///* BSD Socket API Example
//
//   This example code is in the Public Domain (or CC0 licensed, at your option.)
//
//   Unless required by applicable law or agreed to in writing, this
//   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
//   CONDITIONS OF ANY KIND, either express or implied.
//*/
//#include "esp_event.h"
//#include "esp_log.h"
//#include "esp_netif.h"
//#include "esp_system.h"
//#include "esp_wifi.h"
//#include "driver/uart.h"
//#include "freertos/FreeRTOS.h"
//#include "freertos/task.h"
//#include "nvs_flash.h"
//#include "protocol_examples_common.h"
//#include <string.h>
//#include <sys/param.h>
//
//#include "lwip/err.h"
//#include "lwip/sockets.h"
//#include "lwip/sys.h"
//#include <lwip/netdb.h>
//
//#include "uart.h"
//
//#define PORT CONFIG_EXAMPLE_PORT
//#define KEEPALIVE_IDLE CONFIG_EXAMPLE_KEEPALIVE_IDLE
//#define KEEPALIVE_INTERVAL CONFIG_EXAMPLE_KEEPALIVE_INTERVAL
//#define KEEPALIVE_COUNT CONFIG_EXAMPLE_KEEPALIVE_COUNT
//
//#define EXAMPLE_ESP_WIFI_SSID CONFIG_ESP_WIFI_SSID
//#define EXAMPLE_ESP_WIFI_PASS CONFIG_ESP_WIFI_PASSWORD
//#define EXAMPLE_ESP_MAXIMUM_RETRY CONFIG_ESP_MAXIMUM_RETRY
//#define ESP_WIFI_SAE_MODE WPA3_SAE_PWE_BOTH
//#define EXAMPLE_H2E_IDENTIFIER CONFIG_ESP_WIFI_PW_ID
//#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA2_PSK
//
//#define WIFI_CONNECTED_BIT BIT0
//
//#define UART_PORT UART_NUM_1
//#define UART_TX_PIN 4
//#define UART_RX_PIN 5
//#define BUF_SIZE 1024
//
///* FreeRTOS event group to signal when we are connected*/
//static EventGroupHandle_t s_wifi_event_group;
//
///* The event group allows multiple bits for each event, but we only care about two events:
// * - we are connected to the AP with an IP
// * - we failed to connect after the maximum amount of retries */
//#define WIFI_CONNECTED_BIT BIT0
//
//static const char *TAG = "wifi station";
//
////static void do_retransmit(const int sock)
////{
////    int len;
////    char rx_buffer[128];
////
////    do
////    {
////        len = recv(sock, rx_buffer, sizeof(rx_buffer) - 1, 0);
////        if (len < 0)
////        {
////            ESP_LOGE(TAG, "Error occurred during receiving: errno %d", errno);
////        }
////        else if (len == 0)
////        {
////            ESP_LOGW(TAG, "Connection closed");
////        }
////        else
////        {
////            rx_buffer[len] = 0; // Null-terminate whatever is received and treat it like a string
////            ESP_LOGI(TAG, "Received %d bytes: %s", len, rx_buffer);
////
////            // send() can return less bytes than supplied length.
////            // Walk-around for robust implementation.
////            int to_write = len;
////            while (to_write > 0)
////            {
////                int written = send(sock, rx_buffer + (len - to_write), to_write, 0);
////                if (written < 0)
////                {
////                    ESP_LOGE(TAG, "Error occurred during sending: errno %d", errno);
////                    // Failed to retransmit, giving up
////                    return;
////                }
////                to_write -= written;
////            }
////        }
////    } while (len > 0);
////}
//
//static void do_retransmit(const int sock)
//{
//    int len;
//    uint8_t tcp_buffer[128];
//    uint8_t uart_buffer[128];
//
//    while (1)
//    {
//        // --- TCP ? UART ---
//        len = recv(sock, tcp_buffer, sizeof(tcp_buffer), MSG_DONTWAIT);
//        if (len < 0)
//        {
//            if (errno != EWOULDBLOCK && errno != EAGAIN)
//            {
//                ESP_LOGE(TAG, "recv failed: errno %d", errno);
//                return;
//            }
//        }
//        else if (len == 0)
//        {
//            ESP_LOGW(TAG, "Connection closed");
//            return;
//        }
//        else
//        {
//            ESP_LOGI(TAG, "TCP->UART %d bytes", len);
//            uart_write_bytes(UART_PORT, (const char *)tcp_buffer, len);
//        }
//
//        // --- UART ? TCP ---
//        int uart_len = uart_read_bytes(UART_PORT, uart_buffer, sizeof(uart_buffer), 0);
//        if (uart_len > 0)
//        {
//            ESP_LOGI(TAG, "UART->TCP %d bytes", uart_len);
//
//            int to_write = uart_len;
//            while (to_write > 0)
//            {
//                int written = send(sock,
//                                   uart_buffer + (uart_len - to_write),
//                                   to_write,
//                                   0);
//                if (written < 0)
//                {
//                    ESP_LOGE(TAG, "send failed: errno %d", errno);
//                    return;
//                }
//                to_write -= written;
//            }
//        }
//
//        // prevent CPU spin
//        vTaskDelay(pdMS_TO_TICKS(10));
//    }
//}
//
//static void tcp_server_task(void *pvParameters)
//{
//    char addr_str[128];
//    int addr_family = (int)pvParameters;
//    int ip_protocol = 0;
//    int keepAlive = 1;
//    int keepIdle = KEEPALIVE_IDLE;
//    int keepInterval = KEEPALIVE_INTERVAL;
//    int keepCount = KEEPALIVE_COUNT;
//    struct sockaddr_storage dest_addr;
//
//#ifdef CONFIG_EXAMPLE_IPV4
//    if (addr_family == AF_INET)
//    {
//        struct sockaddr_in *dest_addr_ip4 = (struct sockaddr_in *)&dest_addr;
//        dest_addr_ip4->sin_addr.s_addr = htonl(INADDR_ANY);
//        dest_addr_ip4->sin_family = AF_INET;
//        dest_addr_ip4->sin_port = htons(PORT);
//        ip_protocol = IPPROTO_IP;
//    }
//#endif
//
//    int listen_sock = socket(addr_family, SOCK_STREAM, ip_protocol);
//    if (listen_sock < 0)
//    {
//        ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
//        vTaskDelete(NULL);
//        return;
//    }
//    int opt = 1;
//    setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
//#if defined(CONFIG_EXAMPLE_IPV4) && defined(CONFIG_EXAMPLE_IPV6)
//    // Note that by default IPV6 binds to both protocols, it is must be disabled
//    // if both protocols used at the same time (used in CI)
//    setsockopt(listen_sock, IPPROTO_IPV6, IPV6_V6ONLY, &opt, sizeof(opt));
//#endif
//
//    ESP_LOGI(TAG, "Socket created");
//
//    int err = bind(listen_sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
//    if (err != 0)
//    {
//        ESP_LOGE(TAG, "Socket unable to bind: errno %d", errno);
//        ESP_LOGE(TAG, "IPPROTO: %d", addr_family);
//        goto CLEAN_UP;
//    }
//    ESP_LOGI(TAG, "Socket bound, port %d", PORT);
//
//    err = listen(listen_sock, 1);
//    if (err != 0)
//    {
//        ESP_LOGE(TAG, "Error occurred during listen: errno %d", errno);
//        goto CLEAN_UP;
//    }
//
//    while (1)
//    {
//
//        ESP_LOGI(TAG, "Socket listening");
//
//        struct sockaddr_storage source_addr; // Large enough for both IPv4 or IPv6
//        socklen_t addr_len = sizeof(source_addr);
//        int sock = accept(listen_sock, (struct sockaddr *)&source_addr, &addr_len);
//        if (sock < 0)
//        {
//            ESP_LOGE(TAG, "Unable to accept connection: errno %d", errno);
//            break;
//        }
//
//        // Set tcp keepalive option
//        setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, &keepAlive, sizeof(int));
//        setsockopt(sock, IPPROTO_TCP, TCP_KEEPIDLE, &keepIdle, sizeof(int));
//        setsockopt(sock, IPPROTO_TCP, TCP_KEEPINTVL, &keepInterval, sizeof(int));
//        setsockopt(sock, IPPROTO_TCP, TCP_KEEPCNT, &keepCount, sizeof(int));
//        // Convert ip address to string
//
//        if (source_addr.ss_family == PF_INET)
//        {
//            inet_ntoa_r(((struct sockaddr_in *)&source_addr)->sin_addr, addr_str, sizeof(addr_str) - 1);
//        }
//
//        ESP_LOGI(TAG, "Socket accepted ip address: %s", addr_str);
//
//        do_retransmit(sock);
//
//        shutdown(sock, 0);
//        close(sock);
//    }
//
//CLEAN_UP:
//    close(listen_sock);
//    vTaskDelete(NULL);
//}
//
//static void event_handler(void *arg, esp_event_base_t event_base,
//                          int32_t event_id, void *event_data)
//{
//    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
//    {
//        esp_wifi_connect();
//    }
//    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
//    {
//        if (event_data != NULL)
//        {
//            wifi_event_sta_disconnected_t *disconn = (wifi_event_sta_disconnected_t *)event_data;
//            ESP_LOGI(TAG, "Disconnected, reason=%d. Retrying...", disconn->reason);
//        }
//        else
//        {
//            ESP_LOGI(TAG, "Disconnected, retrying...");
//        }
//
//        esp_wifi_connect();
//    }
//    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
//    {
//        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
//        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
//        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
//    }
//}
//
//void wifi_init_sta(void)
//{
//    s_wifi_event_group = xEventGroupCreate();
//
//    ESP_ERROR_CHECK(esp_netif_init());
//
//    //    ESP_ERROR_CHECK(esp_event_loop_create_default());
//    esp_netif_create_default_wifi_sta();
//
//    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
//    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
//
//    esp_event_handler_instance_t instance_any_id;
//    esp_event_handler_instance_t instance_got_ip;
//    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
//                                                        ESP_EVENT_ANY_ID,
//                                                        &event_handler,
//                                                        NULL,
//                                                        &instance_any_id));
//    
//    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
//                                                        IP_EVENT_STA_GOT_IP,
//                                                        &event_handler,
//                                                        NULL,
//                                                        &instance_got_ip));
//
//    wifi_config_t wifi_config = {
//        .sta = {
//            .ssid = EXAMPLE_ESP_WIFI_SSID,
//            .password = EXAMPLE_ESP_WIFI_PASS,
//            /* Authmode threshold resets to WPA2 as default if password matches WPA2 standards (password len => 8).
//             * If you want to connect the device to deprecated WEP/WPA networks, Please set the threshold value
//             * to WIFI_AUTH_WEP/WIFI_AUTH_WPA_PSK and set the password with length and format matching to
//             * WIFI_AUTH_WEP/WIFI_AUTH_WPA_PSK standards.
//             */
//            .threshold.authmode = ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD,
//            .sae_pwe_h2e = ESP_WIFI_SAE_MODE,
//            .sae_h2e_identifier = EXAMPLE_H2E_IDENTIFIER,
//        },
//    };
//    
//    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
//    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
//    ESP_ERROR_CHECK(esp_wifi_start());
//
//    ESP_LOGI(TAG, "wifi_init_sta finished.");
//
// 
//    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
//                                           WIFI_CONNECTED_BIT,
//                                           pdFALSE,
//                                           pdFALSE,
//                                           portMAX_DELAY);
//
//    /* xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually
//     * happened. */
//    if (bits & WIFI_CONNECTED_BIT)
//    {
//        ESP_LOGI(TAG, "connected to ap SSID:%s password:%s",
//                 EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS);
//    }
//    else
//    {
//        ESP_LOGE(TAG, "UNEXPECTED EVENT");
//    }
//}
//
//void uart_init(void)
//{
//    const uart_config_t uart_config = {
//        .baud_rate = 57600, // match your device
//        .data_bits = UART_DATA_8_BITS,
//        .parity = UART_PARITY_DISABLE,
//        .stop_bits = UART_STOP_BITS_1,
//        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
//        .source_clk = UART_SCLK_DEFAULT};
//
//    uart_driver_install(UART_PORT, BUF_SIZE * 2, 0, 0, NULL, 0);
//    uart_param_config(UART_PORT, &uart_config);
//    uart_set_pin(UART_PORT, UART_TX_PIN, UART_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
//
//    ESP_LOGI(TAG, "UART init done (57600 8N1)");
//}
//
//void app_main(void)
//{
//    ESP_ERROR_CHECK(nvs_flash_init());
//    ESP_ERROR_CHECK(esp_netif_init());
//    ESP_ERROR_CHECK(esp_event_loop_create_default());
//
//    ESP_LOGI(TAG, "ESP_WIFI_MODE_STA");
//    
//    uart_init();
//    wifi_init_sta();
//
//    
//    //xTaskCreate(uart_echo_task, "uart_echo", 4096, NULL, 5, NULL);
//    xTaskCreate(tcp_server_task, "tcp_server", 4096, (void *)AF_INET, 5, NULL);
//}
