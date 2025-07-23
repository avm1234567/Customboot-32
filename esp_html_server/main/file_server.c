#include "esp_http_server.h"
#include "esp_log.h"
#include <stdio.h>
#include "esp_spiffs.h"

esp_err_t example_mount_storage(const char *base_path) {
    esp_vfs_spiffs_conf_t conf = {
        .base_path = base_path,
        .partition_label = NULL,
        .max_files = 5,
        .format_if_mount_failed = true
    };

    esp_err_t ret = esp_vfs_spiffs_register(&conf);
    if (ret != ESP_OK) {
        ESP_LOGE("SPIFFS", "Failed to mount or format filesystem");
        return ret;
    }

    size_t total = 0, used = 0;
    esp_spiffs_info(NULL, &total, &used);
    ESP_LOGI("SPIFFS", "SPIFFS mounted. Total: %d bytes, Used: %d bytes", total, used);

    return ESP_OK;
}


static const char *TAG = "web_server";

esp_err_t root_get_handler(httpd_req_t *req) {
    FILE *file = fopen("/data/index.html", "r");
    if (!file) {
        ESP_LOGE(TAG, "index.html not found");
        httpd_resp_send_404(req);
        return ESP_FAIL;
    }

    char buf[1024];
    size_t read_bytes;

    httpd_resp_set_type(req, "text/html");

    while ((read_bytes = fread(buf, 1, sizeof(buf), file)) > 0) {
        httpd_resp_send_chunk(req, buf, read_bytes);
    }

    fclose(file);
    httpd_resp_send_chunk(req, NULL, 0);  // End chunked response
    return ESP_OK;
}

esp_err_t start_web_server(void) {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    httpd_handle_t server = NULL;

    if (httpd_start(&server, &config) == ESP_OK) {
        httpd_uri_t root = {
            .uri      = "/",
            .method   = HTTP_GET,
            .handler  = root_get_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &root);
        ESP_LOGI(TAG, "Server started");
        return ESP_OK;
    }

    return ESP_FAIL;
}
