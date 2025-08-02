# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "C:/Espressif/frameworks/esp-idf-v5.2.3/components/bootloader/subproject"
  "C:/Users/archi/Customboot-32/esp_loopback_test/build/bootloader"
  "C:/Users/archi/Customboot-32/esp_loopback_test/build/bootloader-prefix"
  "C:/Users/archi/Customboot-32/esp_loopback_test/build/bootloader-prefix/tmp"
  "C:/Users/archi/Customboot-32/esp_loopback_test/build/bootloader-prefix/src/bootloader-stamp"
  "C:/Users/archi/Customboot-32/esp_loopback_test/build/bootloader-prefix/src"
  "C:/Users/archi/Customboot-32/esp_loopback_test/build/bootloader-prefix/src/bootloader-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "C:/Users/archi/Customboot-32/esp_loopback_test/build/bootloader-prefix/src/bootloader-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "C:/Users/archi/Customboot-32/esp_loopback_test/build/bootloader-prefix/src/bootloader-stamp${cfgdir}") # cfgdir has leading slash
endif()
