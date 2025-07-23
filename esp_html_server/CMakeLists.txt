cmake_minimum_required(VERSION 3.16)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
include($ENV{IDF_PATH}/tools/cmake/spiffs.cmake)

project(esp_html_server)

spiffs_create_partition_image(spiffs spiffs_image FLASH_IN_PROJECT)
