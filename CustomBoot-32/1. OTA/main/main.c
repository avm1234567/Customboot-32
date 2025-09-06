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

#define TAG "OTA_SERVER"
#define WIFI_SSID "Amita"
#define WIFI_PASS "05072006"
#define UPLOAD_PATH_A "/spiffs/firmware_a.bin"
#define UPLOAD_PATH_B "/spiffs/firmware_b.bin"

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
    if (!f) {
        ESP_LOGE(TAG, "Failed to open file for writing");
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    char buf[1024];
    int received;
    bool header_skipped = false;
    int total_written = 0;
    int trailing_cut = 46;

    // Temporary buffer to store tail for stripping
    char tail_buffer[64] = {0};  // 46 < 64
    int tail_len = 0;

    while ((received = httpd_req_recv(req, buf, sizeof(buf))) > 0) {
        char *data = buf;
        int len = received;

        // Skip multipart headers
        if (!header_skipped) {
            char *start = strstr(buf, "\r\n\r\n");
            if (!start) continue;
            data = start + 4;
            len = received - (data - buf);
            header_skipped = true;
        }

        // Handle tail buffer
        if (tail_len + len <= trailing_cut) {
            memcpy(tail_buffer + tail_len, data, len);
            tail_len += len;
            continue;  // still haven't reached cut limit
        }

        // First, flush previous tail (except 46 final bytes)
        int flush_len = (tail_len > 0) ? tail_len : 0;
        int keep_tail = trailing_cut;
        if (flush_len > 0) {
            int to_write = flush_len - keep_tail;
            if (to_write > 0) {
                fwrite(tail_buffer, 1, to_write, f);
                total_written += to_write;
            }
        }

        // Append current chunk to tail
        int copy_len = len + tail_len - trailing_cut;
        if (copy_len > 0) {
            fwrite(data, 1, copy_len, f);
            total_written += copy_len;
        }

        // Save last 46 bytes as tail for next round
        int tail_start = len - trailing_cut;
        if (tail_start >= 0) {
            memcpy(tail_buffer, data + tail_start, trailing_cut);
            tail_len = trailing_cut;
        }
    }

    fclose(f);

    list_spiffs_files();
    // httpd_resp_sendstr(req, "Upload complete");
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

void app_main(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(spiffs_init());
    wifi_init_sta();
}
