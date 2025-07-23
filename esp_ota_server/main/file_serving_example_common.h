#pragma once

#include "sdkconfig.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t example_mount_storage(const char *base_path);

esp_err_t example_start_file_server(const char *base_path);

#ifdef __cplusplus
}
#endif