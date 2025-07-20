#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "esp_http_server.h"
#include "lwip/sockets.h"
#include "esp_spiffs.h"  // SPIFFS support

#define UART_PORT UART_NUM_1
#define UART_TX 17
#define UART_RX 16
#define UART_RTS 14
#define UART_CTS 15
#define BUF_SIZE 512
#define MAX_ATTEMPTS 3

static const char *TAG = "ESP_IDF_OTA";
static httpd_handle_t server = NULL;

uint32_t crc32_table[256];

void crc32_init() {
    const uint32_t poly = 0xEDB88320;
    for (uint32_t i = 0; i < 256; i++) {
        uint32_t crc = i;
        for (uint8_t j = 0; j < 8; j++) {
            crc = (crc & 1) ? (crc >> 1) ^ poly : crc >> 1;
        }
        crc32_table[i] = crc;
    }
}

uint32_t crc32(const uint8_t *data, size_t len) {
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < len; i++) {
        uint8_t byte = data[i];
        crc = crc32_table[(crc ^ byte) & 0xFF] ^ (crc >> 8);
    }
    return ~crc;
}

uint32_t crc32_continue(uint32_t prev_crc, uint8_t *data, uint32_t len) {
    uint32_t crc = ~prev_crc;
    for (uint32_t i = 0; i < len; i++) {
        crc = crc32_table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
    }
    return ~crc;
}

esp_err_t send_firmware(const char *path) {
    FILE *fw = fopen(path, "rb");
    if (!fw) {
        ESP_LOGE(TAG, "Failed to open firmware file");
        return ESP_FAIL;
    }

    uint8_t buffer[BUF_SIZE];
    uint32_t total_crc = ~0L;

    while (!feof(fw)) {
        size_t len = fread(buffer, 1, BUF_SIZE, fw);
        uint32_t chunk_crc = crc32(buffer, len);

        uart_write_bytes(UART_PORT, (const char *)&len, 4);
        uart_write_bytes(UART_PORT, (const char *)&chunk_crc, 4);

        for (int attempt = 0; attempt < MAX_ATTEMPTS; attempt++) {
            uart_write_bytes(UART_PORT, (const char *)buffer, len);

            char ack[64] = {0};
            int len_ack = uart_read_bytes(UART_PORT, (uint8_t *)ack, sizeof(ack) - 1, 1000 / portTICK_PERIOD_MS);
            if (len_ack > 0 && strstr(ack, "[STM] CRC matched")) {
                break;
            } else if (attempt == MAX_ATTEMPTS - 1) {
                ESP_LOGE(TAG, "Chunk failed after 3 attempts");
                fclose(fw);
                return ESP_FAIL;
            }
        }

        total_crc = crc32_continue(total_crc, buffer, len);
    }

    uint32_t eof = 0xFFFFFFFF;
    uart_write_bytes(UART_PORT, (const char *)&eof, 4);
    uart_write_bytes(UART_PORT, (const char *)&eof, 4);
    uart_write_bytes(UART_PORT, (const char *)&total_crc, 4);

    fclose(fw);
    ESP_LOGI(TAG, "Firmware transfer complete");
    return ESP_OK;
}

void uart_task(void *arg) {
    char line[128];
    while (1) {
        int len = uart_read_bytes(UART_PORT, (uint8_t *)line, sizeof(line) - 1, 100 / portTICK_PERIOD_MS);
        if (len > 0) {
            line[len] = '\0';
            if (strstr(line, "REQ_A")) {
                ESP_LOGI(TAG, "STM requested firmware A");
                send_firmware("/spiffs/firmware_a.bin");
            } else if (strstr(line, "REQ_B")) {
                ESP_LOGI(TAG, "STM requested firmware B");
                send_firmware("/spiffs/firmware_b.bin");
            } else if (strstr(line, "[STM]")) {
                ESP_LOGI(TAG, "%s", line);
            }
        }
    }
}

esp_err_t upload_handler(httpd_req_t *req, const char *filename) {
    char filepath[64];
    snprintf(filepath, sizeof(filepath), "/spiffs/%s", filename);

    FILE *f = fopen(filepath, "w");
    if (!f) return ESP_FAIL;

    char buf[512];
    int received;
    while ((received = httpd_req_recv(req, buf, sizeof(buf))) > 0) {
        fwrite(buf, 1, received, f);
    }

    fclose(f);
    httpd_resp_send(req, "Upload done", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

esp_err_t upload_a_post_handler(httpd_req_t *req) {
    return upload_handler(req, "firmware_a.bin");
}

esp_err_t upload_b_post_handler(httpd_req_t *req) {
    return upload_handler(req, "firmware_b.bin");
}

esp_err_t root_get_handler(httpd_req_t *req) {
    FILE *file = fopen("/spiffs/index.html", "r");
    if (!file) {
        httpd_resp_send_404(req);
        return ESP_FAIL;
    }

    char line[256];
    httpd_resp_set_type(req, "text/html");

    while (fgets(line, sizeof(line), file)) {
        httpd_resp_sendstr_chunk(req, line);
    }

    fclose(file);
    httpd_resp_sendstr_chunk(req, NULL);  // End response
    return ESP_OK;
}

void start_server() {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    httpd_start(&server, &config);

    httpd_uri_t root = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = root_get_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &root);

    httpd_uri_t uri_a = {
        .uri = "/uploadA",
        .method = HTTP_POST,
        .handler = upload_a_post_handler,
        .user_ctx = NULL
    };
    httpd_uri_t uri_b = {
        .uri = "/uploadB",
        .method = HTTP_POST,
        .handler = upload_b_post_handler,
        .user_ctx = NULL
    };

    httpd_register_uri_handler(server, &uri_a);
    httpd_register_uri_handler(server, &uri_b);
}

void app_main() {
    nvs_flash_init();

    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/spiffs",
        .partition_label = "storage",
        .max_files = 5,
        .format_if_mount_failed = true,
    };

    esp_err_t err = esp_vfs_spiffs_register(&conf);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to mount SPIFFS: %s", esp_err_to_name(err));
        return;
    }
    ESP_LOGI(TAG, "SPIFFS mounted successfully.");

    uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_CTS_RTS,
        .rx_flow_ctrl_thresh = 122,
    };

    uart_driver_install(UART_PORT, BUF_SIZE * 2, 0, 0, NULL, 0);
    uart_param_config(UART_PORT, &uart_config);
    uart_set_pin(UART_PORT, UART_TX, UART_RX, UART_RTS, UART_CTS);

    crc32_init();
    xTaskCreate(uart_task, "uart_task", 4096, NULL, 10, NULL);
    start_server();
}

