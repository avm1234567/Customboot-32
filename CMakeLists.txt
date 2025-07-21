cmake_minimum_required(VERSION 3.5)

# Initialize the ESP-IDF build system
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

project(OTA_ESP)
spiffs_create_partition_image(storage spiffs FLASH_IN_PROJECT)
