<h1 align="center">🚀 CustomBoot-32</h1>


<h2 align="center">✨ SRA Eklavya 2025</h2>
---

## 📑 Table of Contents
* [About Me](#-about-me)  
* [Tech-Stack](#tech-stack)  

---

# About Me

My name is Varun Adinath Patil, a Second Year student in VJTI, Mumbai, pursuing Electrinics and Telecommunication Engineering. I am a member of SRA (Society of Robotics and Autonation), under which we built this project.

My contribution in this project was all the software part, along with the PCB schematic of the actual development board. I refered to datasheets of various components and created entire circuit from scratch, where we connected **ESP32 WROOM-32E and STM32 F103C8T6** via UART, integrated **USB-to-TTL** so that we can flash the board with the same power USB. I used **KiCad** to create the schematic.

Furthermore, one of the most important part of this project was to implement a **Dual-Image Bootloader** in STM32 F103C8T6 that receives the firmware, stores it in the flash of STM32 at a particular starting address and then execute it. Initial bootloader was written using HAL libraries, but to optimize it in terms of size, we used a bare-metal librery **LibOpenCM3**.

I also established **UART** communication between ESP32 and STM32. For this I implemented a custom **File Protocol** where I made data structure of a single chunk to have a better control over the data sent. I also implemented **CRC32** to validate the data received.



## Tech-Stack
### Coding Language
<div style="display:flex; align-items:center; margin-bottom:8px;">
  <img src="assets/EmbedC.png" width="50" style="margin-right:8px;">
  <span style="background:#ff6f3c; color:white; padding:4px 10px; border-radius:4px; font-weight:bold;">
    Embedded C
  </span>
</div>


### Libraries
<div style="display:flex; align-items:center; margin-bottom:8px;">
  <img src="assets/HAL.png" width="35" style="margin-right:8px;">
  <span style="background:#ff6f3c; color:white; padding:4px 10px; border-radius:4px; font-weight:bold;">
    HAL
  </span>
</div>

<div style="display:flex; align-items:center; margin-bottom:8px;">
  <img src="assets/LibOpenCM3.png" width="35" style="margin-right:8px;">
  <span style="background:#ff6f3c; color:white; padding:4px 10px; border-radius:4px; font-weight:bold;">
    LibOpenCM3
  </span>
</div>


### File Protocol
<div style="display:flex; align-items:center; margin-bottom:8px;">
  <img src="assets/SPIFFS.png" width="50" style="margin-right:8px;">
  <span style="background:#ff6f3c; color:white; padding:4px 10px; border-radius:4px; font-weight:bold;">
    SPIFFS
  </span>
</div>

### Communication Protocols
<div style="display:flex; align-items:center; margin-bottom:8px;">
  <img src="assets/UART.png" width="60" style="margin-right:8px;">
  <span style="background:#ff6f3c; color:white; padding:4px 10px; border-radius:4px; font-weight:bold;">
    UART
  </span>
</div>
<div style="display:flex; align-items:center; margin-bottom:8px;">
  <img src="assets/OTA.png" width="30" style="margin-right:8px;">
  <span style="background:#ff6f3c; color:white; padding:4px 10px; border-radius:4px; font-weight:bold;">
    OTA
  </span>
</div>