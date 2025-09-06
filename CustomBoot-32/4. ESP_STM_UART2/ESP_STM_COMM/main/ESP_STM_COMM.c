#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include <string.h>

#define UART_NUM UART_NUM_1
#define TXD_PIN GPIO_NUM_17
#define RXD_PIN GPIO_NUM_16
#define BUF_SIZE 128
static const char *TAG = "UART_TEST";

void app_main(void)
{
    uart_config_t config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE
    };

    // Install driver and configure UART
    uart_driver_install(UART_NUM, BUF_SIZE * 2, BUF_SIZE * 2, 0, NULL, 0);
    uart_param_config(UART_NUM, &config);
    uart_set_pin(UART_NUM, TXD_PIN, RXD_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
	const char *msg = "Hello STM from ESP\r\n";
    uint8_t data[BUF_SIZE];
    uint8_t last_byte = '\0';
    while (1) {
  uart_write_bytes(UART_NUM, msg, strlen(msg));
//vTaskDelay(pdMS_TO_TICKS(1000));
//uart_wait_tx_done(UART_NUM, portMAX_DELAY);


        int rx_len = uart_read_bytes(UART_NUM, data, BUF_SIZE - 1, pdMS_TO_TICKS(100));
        
        if (rx_len > 0) {
       //if(strcmp(last_byte, (uint8_t*)data[rx_len])!=0){
            data[rx_len] = 0;
            ESP_LOGI(TAG, "Received %d bytes: %s", rx_len, data);
        }
        //}

//vTaskDelay(pdMS_TO_TICKS(300));
    }
}

