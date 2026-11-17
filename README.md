# PocketBeep: ESP32-C3 Morse Code Transceiver
![Platform](https://img.shields.io/badge/Platform-ESP32--C3-blue)
![License](https://img.shields.io/badge/License-MIT-green)
![Hardware](https://img.shields.io/badge/Hardware-Open_Source-orange)
A compact, retro-styled Morse code communicator built around the Seeed Studio XIAO ESP32-C3. This project uses ESP-NOW to send authentic Morse timing pulses over the air, avoiding the "cheat" of sending raw text strings. It features a live text-to-Morse translator, an inbox for missed messages, customizable UI themes, and a few hidden arcade games.
![box-image](https://github.com/Kiteretsu1/PocketBeep-Morse-Transreciver/blob/main/Assets/IMG_20261117_125700.jpg?raw=true)


(https://github.com/Kiteretsu1/PocketBeep-Morse-Transreciver/blob/main/Assets/IMG_20261117_125700.jpg)

![Alt text](Kiteretsu1/PocketBeep-Morse-Transreciver/Assets/GIF_20261117_185044_496.gif)
![Alt text](Kiteretsu1/PocketBeep-Morse-Transreciver/Assets/MorseCircuit.pdf)

## Features
* **True Pulse Transmission:** Calculates and transmits raw timing data (dots, dashes, gaps) over ESP-NOW, keeping units perfectly synchronized regardless of the sender's tapping speed.
* **Live Translator:** A built-in 40-key digital keyboard allows users who don't know Morse code to type messages that are automatically pulsed over the air.
* **The Inbox:** Stores missed incoming pulses and decodes them into text so you never miss a message.
* **Customizable UI:** Five distinct retro themes (Phosphor NES, Cutesy BMO, Gameboy, Amber CRT, Famicom) and adjustable timing/volume settings persisted in NVS flash.
* **Zero-Flicker Graphics:** Uses a 40KB hardware SPI framebuffer canvas on a 1.8" ST7735 TFT display for perfectly smooth UI rendering at ~45 FPS.
* **Hidden Arcade:** Includes fully playable, miniature versions of Flappy Bird and the Chrome Dino game.

## Hardware Requirements
* **Microcontroller:** Seeed Studio XIAO ESP32-C3
* **Display:** 1.8" SPI TFT Display (128x160, ST7735 controller)
* **Power:** 1500mAh LiPo Battery + TP4056 Charging Module
* **Audio:** Passive Piezo Buzzer + BC547 NPN Transistor + 330Ω Resistor
* **Inputs:** 3x 12x12mm Tactile Push Buttons + 3x 10kΩ Pull-down Resistors
* **Battery Monitor:** Voltage divider using 2x 10kΩ Resistors + 22nF Capacitor

## Pin Mapping (XIAO ESP32-C3)
* `D0` - Battery Voltage Divider Input (ADC)
* `D1` - Piezo Buzzer (Driven via BC547 base)
* `D2` - Red Action / Morse Key Button
* `D3` - White Button Left (Previous)
* `D4` - White Button Right (Next)
* `D6` - TFT CS
* `D7` - TFT DC / A0
* `D8` - TFT SCK
* `D10` - TFT SDA / MOSI
* `3V3` - TFT RST, VCC, and LED

## Library Dependencies
Install the following libraries via the Arduino IDE Library Manager:
1. **Adafruit GFX Library** (by Adafruit)
2. **Adafruit ST7735 and ST7789 Library** (by Adafruit)

## Installation & Flashing
1. Add the ESP32 board manager URL to your Arduino IDE preferences.
2. Install the `esp32` board package (supports both Core 2.x and 3.x).
3. Select **XIAO_ESP32C3** from the boards menu.
4. Connect the board via USB-C, select the correct COM port, and upload the sketch. 
*(Note: You will need to build and flash two identical units for them to communicate!)*
