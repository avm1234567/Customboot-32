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
#define WIFI_SSID "Delta_Virus_2.4G"
#define WIFI_PASS "66380115"
#define UPLOAD_PATH_A "/spiffs/firmware_a.bin"
#define UPLOAD_PATH_B "/spiffs/firmware_b.bin"

static esp_err_t spiffs_init(void)
{
    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/spiffs",
        .partition_label = "storage",
        .max_files = 5,
        .format_if_mount_failed = true
    };

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
        if (!f) continue;
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


static esp_err_t upload_post_handler(httpd_req_t *req, const char *path)
{
    FILE *f = fopen(path, "w");
    if (!f)
    {
        ESP_LOGE(TAG, "Failed to open %s for writing", path);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "File open failed");
        return ESP_FAIL;
    }

    char buf[1024];
    int total = 0;
    int received;
    while ((received = httpd_req_recv(req, buf, sizeof(buf))) > 0)
    {
        fwrite(buf, 1, received, f);
        total += received;
    }

    fclose(f);

    if (received < 0)
    {
        ESP_LOGE(TAG, "File receive failed");
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Receive failed");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Received %d bytes to %s", total, path);
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

void app_main(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(spiffs_init());
    wifi_init_sta();
    
}

