# Customboot-32
**STM32+ESP32 Development Board with Custom Bootloader and OTA Support**

### Project Description
- Pair an STM32F103 “Blue Pill” with an ESP32-WROOM-32D (16 MB) on a custom PCB.
- ESP32 runs a web-server, saves uploaded binaries to SPIFFS, and streams them over chunked UART with CRC checks.
- STM32 hosts a dual-image bootloader: it verifies each image, rolls back on failure, and lets a 2-position switch pick which firmware to boot.
- Result: a plug-and-play dev board that can flash, select, and update its own firmware wirelessly—no USB cables or ST-Link required.

### ESP OTA server testing
- Initially started with running the file serving example from the Espressif example directory. This is the path to the folder: examples/protocols/http_server/file_serving
- After examples, we tried to make our own server which accepts two binary files and they are flashed to STM Referred the code file serving and made changes in it.
  
### ESP tests
- We ran test for uart loopback which displays the message that is hardcoded in the code by connecting the TX n RX pin of UART 1.
- After that we ran UART echo test that displays what we type on the `screen`.


