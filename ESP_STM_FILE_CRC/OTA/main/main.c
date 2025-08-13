#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include "esp_log.h"
#include "esp_spiffs.h"
#include "esp_http_server.h"
#include "esp_vfs.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "driver/gpio.h"

#define TAG "OTA_SERVER"
#define WIFI_SSID "1234"
#define WIFI_PASS "123456789"
#define UPLOAD_PATH_A "/spiffs/firmware_a.bin"
#define UPLOAD_PATH_B "/spiffs/firmware_b.bin"
#define BUF_SIZE 1024
#define DATA_SIZE 512
#define UART_NUM UART_NUM_1
#define TXD_PIN GPIO_NUM_17
#define RXD_PIN GPIO_NUM_16
#define CRC_POLY 0x04C11DB7U
#define CRC_INIT 0xFFFFFFFFU
TaskHandle_t wifiTaskHandle;
TaskHandle_t uartTaskHandle;
uint8_t Packet[DATA_SIZE + 8];
uint8_t rx_buffer[BUF_SIZE];
void receiveAck(void);

struct Chunk_file
{
    uint8_t start;
    uint16_t length;
    uint8_t data[DATA_SIZE];
    uint8_t crc32[4];
    uint8_t end;
};

static esp_err_t spiffs_init(void)
{
    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/spiffs",
        .partition_label = "storage",
        .max_files = 5,
        .format_if_mount_failed = true};

    esp_err_t ret = esp_vfs_spiffs_register(&conf);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to mount SPIFFS (%s)", esp_err_to_name(ret));
        return ret;
    }

    size_t total = 0, used = 0;
    ret = esp_spiffs_info("storage", &total, &used);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to get SPIFFS info (%s)", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "SPIFFS mounted. Total: %d bytes, Used: %d bytes", total, used);
    return ESP_OK;
}

void list_spiffs_files()
{
    DIR *dir = opendir("/spiffs");
    struct dirent *entry;

    if (!dir)
    {
        ESP_LOGE(TAG, "Failed to open /spiffs");
        return;
    }

    while ((entry = readdir(dir)) != NULL)
    {
        char filepath[512];
        snprintf(filepath, sizeof(filepath), "/spiffs/%s", entry->d_name);
        FILE *f = fopen(filepath, "r");
        if (!f)
            continue;
        fseek(f, 0, SEEK_END);
        long size = ftell(f);
        fclose(f);
        ESP_LOGI(TAG, "  %s (%ld bytes)", entry->d_name, size);
    }

    closedir(dir);
}

static esp_err_t index_get_handler(httpd_req_t *req)
{
    list_spiffs_files();
    FILE *file = fopen("/spiffs/index.html", "r");
    if (!file)
    {
        ESP_LOGE(TAG, "index.html not found on SPIFFS");
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "index.html missing");
        return ESP_FAIL;
    }

    char line[256];
    httpd_resp_set_type(req, "text/html");
    while (fgets(line, sizeof(line), file))
    {
        httpd_resp_sendstr_chunk(req, line);
    }
    fclose(file);
    httpd_resp_sendstr_chunk(req, NULL);
    return ESP_OK;
}

static esp_err_t favicon_handler(httpd_req_t *req)
{
    httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "favicon not found");
    return ESP_FAIL;
}

esp_err_t upload_post_handler(httpd_req_t *req, const char *path)
{
    FILE *f = fopen(path, "w");
    if (!f)
    {
        ESP_LOGE(TAG, "Failed to open file for writing");
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    char buf[1024];
    int received;
    bool header_skipped = false;

    // Temp buffer to store all data until end
    uint8_t *file_buffer = NULL;
    size_t file_size = 0;

    while ((received = httpd_req_recv(req, buf, sizeof(buf))) > 0)
    {
        char *data = buf;
        int len = received;

        // Skip HTTP multipart headers once
        if (!header_skipped)
        {
            char *start = strstr(buf, "\r\n\r\n");
            if (!start)
                continue; // still reading headers
            data = start + 4;
            len = received - (data - buf);
            header_skipped = true;
        }

        // Append to buffer
        uint8_t *new_buffer = realloc(file_buffer, file_size + len);
        if (!new_buffer)
        {
            free(file_buffer);
            fclose(f);
            ESP_LOGE(TAG, "Out of memory");
            return ESP_FAIL;
        }
        file_buffer = new_buffer;
        memcpy(file_buffer + file_size, data, len);
        file_size += len;
    }

    // Remove last 46 bytes (boundary)
    if (file_size > 46)
    {
        file_size -= 46;
    }

    // Write cleaned firmware to SPIFFS
    fwrite(file_buffer, 1, file_size, f);
    fclose(f);
    free(file_buffer);

    ESP_LOGI(TAG, "Firmware uploaded. Size after strip: %d bytes", file_size);
    list_spiffs_files();
    return ESP_OK;
}

static esp_err_t upload_a_post(httpd_req_t *req)
{
    return upload_post_handler(req, UPLOAD_PATH_A);
}

static esp_err_t upload_b_post(httpd_req_t *req)
{
    return upload_post_handler(req, UPLOAD_PATH_B);
}

static httpd_handle_t start_webserver(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    httpd_handle_t server = NULL;

    if (httpd_start(&server, &config) == ESP_OK)
    {
        httpd_register_uri_handler(server, &(httpd_uri_t){
                                               .uri = "/", .method = HTTP_GET, .handler = index_get_handler});

        httpd_register_uri_handler(server, &(httpd_uri_t){
                                               .uri = "/upload_a", .method = HTTP_POST, .handler = upload_a_post});

        httpd_register_uri_handler(server, &(httpd_uri_t){
                                               .uri = "/upload_b", .method = HTTP_POST, .handler = upload_b_post});

        httpd_register_uri_handler(server, &(httpd_uri_t){
                                               .uri = "/favicon.ico", .method = HTTP_GET, .handler = favicon_handler});
    }

    return server;
}

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
    {
        esp_wifi_connect();
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        ESP_LOGW(TAG, "Wi-Fi disconnected, reconnecting...");
        esp_wifi_connect();
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
        start_webserver();
    }
}

static void wifi_init_sta(void)
{
    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);

    esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL);
    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL);

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
        },
    };

    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    esp_wifi_start();

    ESP_LOGI(TAG, "Wi-Fi connecting to SSID: %s", WIFI_SSID);
}

uint32_t crc32_libopencm3_style(const uint8_t *data, size_t length)
{
    uint32_t crc = CRC_INIT;

    for (size_t i = 0; i < length; i++)
    {
        crc ^= ((uint32_t)data[i]) << 24; // align byte to MSB
        for (uint8_t bit = 0; bit < 8; bit++)
        {
            if (crc & 0x80000000U)
            {
                crc = (crc << 1) ^ CRC_POLY;
            }
            else
            {
                crc <<= 1;
            }
        }
    }
    return crc; // no final XOR, same as libopencm3
}

void Send_firmware_protocol(const char *path)
{
    struct Chunk_file Firmware;
    Firmware.start = 0x00;
    // memset(Firmware.crc32, 0x00, sizeof(Firmware.crc32));

    // memcpy(&Packet[3 + DATA_SIZE], Firmware.crc32, 4);

    FILE *f = fopen(path, "rb");
    if (!f)
    {
        ESP_LOGE(TAG, "Failed to open file for reading");
        memset(Packet, (uint8_t)0xAA, 520);
        uart_write_bytes(UART_NUM, (const char *)Packet, 520);
        receiveAck();

        // uart_write_bytes(UART_NUM, "No firmware\r", strlen("No firmware\r"));
    }
    Packet[0] = Firmware.start;

    while (1)
    {
        size_t data_read = fread(Firmware.data, 1, DATA_SIZE, f);

        if (data_read == 0) // EOF
            break;

        // Check if last chunk
        if (data_read < DATA_SIZE)
        {
            memset(&Firmware.data[data_read], 0xFF, DATA_SIZE - data_read);
            Firmware.end = 0xAF; // Final chunk
        }
        else
        {
            // Peek ahead: if no more data, this is final chunk
            int c = fgetc(f);
            if (c == EOF)
                Firmware.end = 0xAF;
            else
            {
                ungetc(c, f);
                Firmware.end = 0x00;
            }
        }
        uint32_t crc_ESP = crc32_libopencm3_style(Firmware.data, 512);

        // Fill packet
        Packet[1] = (data_read >> 8) & 0xFF;
        Packet[2] = data_read & 0xFF;
        memcpy(&Packet[3], Firmware.data, DATA_SIZE);
        Packet[515 + 0] = (crc_ESP >> 24) & 0xFF; // MSB
        Packet[515 + 1] = (crc_ESP >> 16) & 0xFF;
        Packet[515 + 2] = (crc_ESP >> 8) & 0xFF;
        Packet[515 + 3] = (crc_ESP >> 0) & 0xFF; // LSB
        Packet[7 + DATA_SIZE] = Firmware.end;

        int sent = uart_write_bytes(UART_NUM, (const char *)Packet, 8 + DATA_SIZE);
        if (sent == 8 + DATA_SIZE)
        {

            // Wait for STM32 "Ready" before next chunk
            while (1)
            {
                memset(rx_buffer, 0, BUF_SIZE);
                uart_read_bytes(UART_NUM, rx_buffer, BUF_SIZE - 1, pdMS_TO_TICKS(1000));
                // if(strncmp((char *)rx_buffer, "crc", 3) == 0){
                //   ESP_LOGI(TAG, "%s", "Ready\r");
                //}
                if (strcmp((char *)rx_buffer, "Send_\r") == 0)
                {
                }
                if (strncmp((char *)rx_buffer, "crc", 3) == 0)
                {
                    ESP_LOGI(TAG, "CRC of ESP: %lx\r", crc_ESP);
                    ESP_LOGI(TAG, "%s", rx_buffer);
                    break;
                }
                if (Firmware.end == 0xAF)
                {
                    ESP_LOGI(TAG, "Final Chunk Sent.");
                    receiveAck();
                }
            }
        }
    }

    fclose(f);
}

int Ack(void)
{

    uart_read_bytes(UART_NUM, rx_buffer, BUF_SIZE - 1, pdMS_TO_TICKS(100));
    // ESP_LOGI(TAG, "Received String From STM: %s", rx_buffer);
    // ESP_LOGI(TAG, "Ack received");

    if (strcmp((char *)rx_buffer, "Send_A\r") == 0)
    {
        ESP_LOGI(TAG, "Acknowledgement for A verified correct");

        memset(rx_buffer, 0, BUF_SIZE);
        return 1;
    }
    else if (strcmp((char *)rx_buffer, "Send_B\r") == 0)
    {

        ESP_LOGI(TAG, "Acknowledgement for B verified correct");
        memset(rx_buffer, 0, BUF_SIZE);
        return 2;
    }
    else
    {
        ESP_LOGI(TAG, "%s", rx_buffer);
        memset(rx_buffer, 0, BUF_SIZE);
        return 0;
    }
}
void wifi_spiffs_task(void *pvParameters)
{
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(spiffs_init());
    wifi_init_sta(); // Starts WiFi and HTTP server in event handler
    while (1)
    {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
void receiveAck(void)
{

    while (1)
    {
        int ack = Ack();
        if (ack == 1)
        {
            vTaskDelay(pdMS_TO_TICKS(1000));
            Send_firmware_protocol(UPLOAD_PATH_A);
            // If you want to keep sending repeatedly, remove the break
            break;
        }
        if (ack == 2)
        {
            vTaskDelay(pdMS_TO_TICKS(1000));
            Send_firmware_protocol(UPLOAD_PATH_B);
            // If you want to keep sending repeatedly, remove the break
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void uart_firmware_task(void *pvParameters)
{
    uart_config_t config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE};

    uart_driver_install(UART_NUM, BUF_SIZE * 2, BUF_SIZE * 2, 0, NULL, 0);
    uart_param_config(UART_NUM, &config);
    uart_set_pin(UART_NUM, TXD_PIN, RXD_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);

    ESP_LOGI(TAG, "UART initialized");

    receiveAck();
}

void app_main(void)
{
    xTaskCreatePinnedToCore(
        wifi_spiffs_task,
        "WiFi_SPIFFS_Task",
        4096,
        NULL,
        5,
        &wifiTaskHandle,
        0);

    xTaskCreatePinnedToCore(
        uart_firmware_task,
        "UART_Firmware_Task",
        4096,
        NULL,
        6,
        &uartTaskHandle,
        1);
}
