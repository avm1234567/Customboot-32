This is Varun Adinath Patil from SY EXTC.
This branch contains my work and will update my progress throughout my Eklavya. 
# Customboot-32

**Custom PCB integrating ESP32-WROOM-32D & STM32F103C8T6 with OTA Firmware Update & Custom Bootloader**

---

## 🚀 Project Overview

This project aims to design a **custom PCB** integrating the powerful **ESP32-WROOM-32D** and **STM32F103C8T6** microcontrollers. The ESP32 acts as the primary firmware manager, receiving updates via **OTA (Over-The-Air)**, storing them securely in SPIFFS, and transmitting them to the STM32 MCU over UART.

A **custom bootloader** on the STM32 coordinates firmware requests from the ESP32 based on user inputs, ensuring robust and reliable firmware flashing with CRC validation for data integrity.

---

## 🛠️ Features & Highlights

- **ESP32 OTA Firmware Reception**  
  - Initializes SPIFFS for file storage.  
  - Manages custom memory partitions for firmware.

- **Robust UART Communication**  
  - Supports one-way and two-way UART communication between ESP32 and STM32.  
  - Ready-to-use protocol formats for seamless data transfer.

- **Custom STM32 Bootloader**  
  - Requests firmware chunks from ESP32 dynamically.  
  - Implements CRC32 checks for firmware validation.  
  - Supports user-driven firmware update control.

- **Extensive Testing & Validation**  
  - UART communication thoroughly tested with real firmware images.  
  - CRC verification ensures firmware reliability.

---

## 📂 Repository Structure (varun branch)


