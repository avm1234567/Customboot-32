#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "nvs_flash.h"
#include "protocol_examples_common.h"
#include "esp_err.h"

extern esp_err_t start_web_server(void);
extern esp_err_t example_mount_storage(const char *base_path);

void app_main(void) {
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(example_connect());

    // Mount SPIFFS to path /data
    ESP_ERROR_CHECK(example_mount_storage("/data"));

    // Start web server
    ESP_ERROR_CHECK(start_web_server());
}

