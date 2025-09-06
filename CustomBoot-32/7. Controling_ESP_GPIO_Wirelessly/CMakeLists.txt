cmake_minimum_required(VERSION 3.5)
set(SPIFFS_IMG_DIR "${CMAKE_SOURCE_DIR}/spiffs")

# Initialize the ESP-IDF build system
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

project(OTA_ESP)

