# Customboot-32
**STM32+ESP32 Development Board with Custom Bootloader and OTA Support**

### Project Description
- Pair an STM32F103 “Blue Pill” with an ESP32-WROOM-32D (16 MB) on a custom PCB.
- ESP32 runs a web-server, saves uploaded binaries to SPIFFS, and streams them over chunked UART with CRC checks.
- STM32 hosts a dual-image bootloader: it verifies each image, rolls back on failure, and lets a 2-position switch pick which firmware to boot.
- Result: a plug-and-play dev board that can flash, select, and update its own firmware wirelessly—no USB cables or ST-Link required.

### ESP OTA server testing 
- Initially started with running the file serving example from the Espressif example directory.
This is the path to the folder: examples/protocols/http_server/file_serving
- After examples, we tried to make our own server which accepts two binary files and they are flashed to STM  Referred the code file serving and made changes in it.

### ESP tests
- We ran test for uart loopback which displays the message that is hardcoded in the code by connecting the TX n RX pin of UART 1.
- After that we ran UART echo test that displays what we type on the `screen`.

### Simple Bootloader
- Made a bootloader which switches to the main application that is contains.
- In the bootloader the light blinks 2 times with a delay of 100ms.
- After it switches to application it blinks continuously with a delay of 1sec.
- To flash the program use STM32CubeProgrammer and put the start address of bootloader as 0x08000000 and application as 0x08004400.

### Implementing File protocol
- When we transfer the binary files from ESP to STM, we cannot send the whole file at once.
- It should be sent in chunks and for that we have to use file transfer protocol.

### Exploring libopencm3 to implement 
- The current size of the bootloader is big and we have to short it. For that we will we using libopencm3.
- libopencm3 is a low level, bare metal library for ARM Cortex-M microcontroller.







