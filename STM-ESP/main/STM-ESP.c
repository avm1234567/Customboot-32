#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "driver/gpio.h"

#define UART_PORT UART_NUM_1
#define TXD_PIN GPIO_NUM_17
#define RXD_PIN GPIO_NUM_16
#define BUF_SIZE 1024

static const char *TAG = "UART_LOOPBACK";

void app_main(void)
{
    // UART config
    uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE
    };
    
    // Install and configure UART driver
    uart_driver_install(UART_PORT, BUF_SIZE * 2, 0, 0, NULL, 0);
    uart_param_config(UART_PORT, &uart_config);
    uart_set_pin(UART_PORT, TXD_PIN, RXD_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);

    const char *tx_data = "Hello STM32!\n";

    while (1) {
        // 1. Send to STM32
        uart_write_bytes(UART_PORT, tx_data, strlen(tx_data));
        ESP_LOGI(TAG, "Sent: %s", tx_data);

        // 2. Wait for echo from STM32
        uint8_t rx_buf[BUF_SIZE];
        int len = uart_read_bytes(UART_PORT, rx_buf, BUF_SIZE - 1, pdMS_TO_TICKS(2000));

        if (len > 0) {
            rx_buf[len] = '\0'; // Null-terminate received string
            ESP_LOGI(TAG, "Received: %s", (char *)rx_buf);
        } else {
            ESP_LOGW(TAG, "No data received");
        }

        vTaskDelay(pdMS_TO_TICKS(3000)); // Wait 3 seconds before next send
    }
}



