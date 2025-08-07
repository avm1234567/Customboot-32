# Customboot-32
**STM32+ESP32 Development Board with Custom Bootloader and OTA Support**

### Project Description
- Pair an STM32F103 “Blue Pill” with an ESP32-WROOM-32D (16 MB) on a custom PCB.
- ESP32 runs a web-server, saves uploaded binaries to LittleFS, and streams them over chunked UART with CRC checks.
- STM32 hosts a dual-image bootloader: it verifies each image, rolls back on failure, and lets a 2-position switch pick which firmware to boot.
- Result: a plug-and-play dev board that can flash, select, and update its own firmware wirelessly—no USB cables or ST-Link required.


