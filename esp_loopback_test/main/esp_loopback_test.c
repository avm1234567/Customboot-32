#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_log.h"

#define UART_NUM UART_NUM_1           // Using UART1
#define TXD_PIN GPIO_NUM_17           // Connect TXD to RXD
#define RXD_PIN GPIO_NUM_16
#define BUF_SIZE 1024

static const char *TAG = "UART_LOOPBACK";

void app_main(void)
{
    // UART configuration
    const uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE
    };

    // Install and configure UART driver
    uart_driver_install(UART_NUM, BUF_SIZE * 2, 0, 0, NULL, 0);
    uart_param_config(UART_NUM, &uart_config);
    uart_set_pin(UART_NUM, TXD_PIN, RXD_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);

    const char *test_str = "Hello Loopback ESP32\r\n";
    uint8_t data[BUF_SIZE];

    while (1) {
        // Send string over UART
        uart_write_bytes(UART_NUM, test_str, strlen(test_str));
        ESP_LOGI(TAG, "Sent: %s", test_str);

        // Read echoed data
        int len = uart_read_bytes(UART_NUM, data, BUF_SIZE - 1, pdMS_TO_TICKS(1000));
        if (len > 0) {
            data[len] = '\0';  // Null-terminate for printing
            ESP_LOGI(TAG, "Received (%d bytes): '%s'", len, (char *)data);
        } else {
            ESP_LOGW(TAG, "No data received");
        }

        vTaskDelay(pdMS_TO_TICKS(2000)); // Wait 2 seconds
    }
}
