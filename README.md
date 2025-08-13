# Hello!!!
# This is **Varun Adinath Patil** from **SY EXTC.**
### This branch contains my work and will update my progress throughout my Eklavya. 
# Customboot-32

**Custom PCB integrating ESP32-WROOM-32D & STM32F103C8T6 with OTA Firmware Update & Custom Bootloader**

---

## 🚀 Project Overview

This project aims to design a **custom PCB** integrating the powerful **ESP32-WROOM-32D** and **STM32F103C8T6** microcontrollers. The ESP32 acts as the primary firmware manager, receiving updates via **OTA (Over-The-Air)**, storing them securely in SPIFFS, and transmitting them to the STM32 MCU over UART.

A **Custom Dual Image bootloader** on the STM32 coordinates firmware requests from the ESP32 based on user inputs, ensuring robust and reliable firmware flashing with CRC validation for data integrity.

---

## 🛠️ Features & Highlights

- **ESP32 OTA Firmware Reception**  
  - Initializes SPIFFS for file storage.  
  - Manages custom memory partitions for firmware.

- **UART Communication**  
  - Supports one-way and two-way UART communication between ESP32 and STM32.  
  
- **Custom STM32 Bootloader**  
  - Requests firmware chunks from ESP32.  
  - Implements CRC32 checks for firmware validation.  

- **Extensive Testing & Validation**  
  - UART communication thoroughly tested with real firmware images.  
  - CRC verification ensures firmware reliability.

---

# 📂 Repository Structure (varun branch)

## Dual_Image_Bootloader 
- STM32 dual image custom bootloader code
- Also contains 2 applications written according to the firmware locations.
- [Link to old bootloader notes](https://www.notion.so/BootLoader-2298c356155f80d4acd2e90b8f80611c)

## ESP_STM_FILE_CRC 
- Firmware CRC logic updates and tools
- [LibOpenCM3 bootloader notion](https://www.notion.so/LibOpenCM3-2478c356155f80a6afcfc1611a41d326)

## ESP_STM_UART1 
- One-way UART communication (ESP → STM)

## ESP_STM_UART2 
- Two-way UART communication implementation

## ESP_TO_STM_FIRMWARE_VIA_UART 
- UART protocol for sending firmware to STM
- Implemented File protocols for more controllability
- [Notion notes of File protocol and my implementation](https://www.notion.so/Esp-to-Stm-File-transfer-File-Protocol-2438c356155f8017bc71f075e02f5cac)

## OTA 
- ESP32 OTA code with SPIFFS & partition setup



