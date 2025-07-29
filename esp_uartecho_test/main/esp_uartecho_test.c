#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include <string.h>

#define UART_PORT       UART_NUM_0
#define TX_PIN GPIO_NUM_1   // TXD0 (ESP32 to PC)
#define RX_PIN GPIO_NUM_3  // RXD0 (PC to ESP32)
#define BUF_SIZE        1024

static const char *TAG = "UART_ECHO";


void uart_echo_task(void *arg)
{
    uint8_t data[BUF_SIZE];
    while (1) {
        int len = uart_read_bytes(UART_PORT, data, BUF_SIZE - 1, pdMS_TO_TICKS(1000));
        if (len > 0) {
            data[len] = '\0';
            ESP_LOGI(TAG, "Received: %s", data);

                uart_write_bytes(UART_PORT, (const char *)data, len);
            
        }
    }
}

void app_main(void)
{
    const uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE
    };

    uart_driver_install(UART_PORT, BUF_SIZE * 2, 0, 0, NULL, 0);
    uart_param_config(UART_PORT, &uart_config);
    uart_set_pin(UART_PORT, TX_PIN, RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);

    xTaskCreate(uart_echo_task, "uart_echo_task", 4096, NULL, 10, NULL);
}