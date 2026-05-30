
#include "driver/uart.h"
#include "esp_log.h"
#include "uart.h"

#define UART_PORT UART_NUM_1
#define UART_TX_PIN 4
#define UART_RX_PIN 5
#define BUF_SIZE 1024

static const char *TAG = "uart";

void uart_init(void)
{
    const uart_config_t uart_config = {
        .baud_rate = 115200, // match your device
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT};

    uart_driver_install(UART_PORT, BUF_SIZE * 2, 0, 0, NULL, 0);
    uart_param_config(UART_PORT, &uart_config);
    uart_set_pin(UART_PORT, UART_TX_PIN, UART_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);

    ESP_LOGI(TAG, "UART init done (115200 8N1)");
}

void uart_echo_task(void *arg)
{
    uint8_t buf[BUF_SIZE];

    while (1)
    {
        int len = uart_read_bytes(UART_PORT, buf, BUF_SIZE, pdMS_TO_TICKS(20));
        if (len > 0)
        {
            // echo back
            uart_write_bytes(UART_PORT, (const char *)buf, len);

            // optional: log count
            ESP_LOGI(TAG, "echoed %d bytes", len);
        }
    }
}