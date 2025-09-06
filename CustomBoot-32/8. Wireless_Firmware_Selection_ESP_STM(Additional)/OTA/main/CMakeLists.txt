idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES
        esp_http_server
        nvs_flash
        spiffs
        driver
        freertos
        esp_wifi
)

