# Tire Temperature Monitoring System

A real-time tire temperature monitoring system for performance vehicles that displays the temperature of each tire on an LCD screen using infrared temperature sensors.

## Overview

This project provides a complete solution for monitoring tire temperatures in performance driving or racing applications. It uses non-contact infrared temperature sensors (MLX90614) to measure tire temperatures and displays them on an LCD screen with color-coding based on configurable temperature thresholds.

## Features

- Real-time monitoring of all four tire temperatures
- Color-coded temperature display:
  - Blue: Cold tires (below minimum temperature)
  - Green: Normal operating temperature
  - Purple: Ideal temperature range
  - Red: Overheated tires
- Two modes: Street and Track, with different temperature thresholds
- Support for both Fahrenheit and Celsius temperature scales
- Night mode with configurable brightness (activated by right swipe gesture)
- Touch interface for menu navigation
- Configuration menu for temperature ranges, display settings, and sensor mapping
- Settings stored in EEPROM for persistence between power cycles
- Wireless communication via WiFi (ESP32 version) or Bluetooth (modular version)
- Test mode for display demonstration
- 3D printable enclosures and mounting options

## Hardware Components

- Microcontroller:
  - Waveshare ESP32-S3 Touch LCD 1.69" development board (tires_esp32 version)
    - ESP32-S3 dual-core microcontroller
    - 1.69" round LCD display (240×280 resolution) with ST7789 driver
    - Built-in CST816S capacitive touch controller with gesture support
    - 8MB PSRAM and 16MB Flash memory
    - USB Type-C interface for programming and power
    - WiFi and Bluetooth connectivity
    - I2C, SPI, and GPIO interfaces
    - Compact round form factor ideal for the project
  - Arduino/ATmega (tires_modular version)
- MLX90614 Infrared Temperature Sensors (one per tire)
- TCA9548A I2C Multiplexer (for managing multiple sensors)
- 3D printed enclosures (designs included in the 3d_parts folder)

## Directory Structure

- `/3d_parts` - 3D printable files for enclosures and mounting hardware
- `/circuits` - Circuit diagrams and PCB designs
- `/i2c_scanner` - Utility sketch for detecting I2C devices
- `/menu_test` - Test sketch for menu system
- `/tires_esp32` - Main implementation using ESP32 with touch interface and WiFi
- `/tires_modular` - Simplified implementation for Arduino with Bluetooth
- `/wifi_test` - Test sketch for WiFi functionality

## Setup and Installation

### Hardware Setup

1. Assemble the hardware components according to the circuit diagrams in the `/circuits` folder.
2. Connect the MLX90614 sensors to the TCA9548A I2C multiplexer.
3. Connect the ST7789 display via SPI.
4. For ESP32 version, connect the CST816 touch controller via I2C.
5. Print and assemble the enclosures from the 3D models in the `/3d_parts/final parts` folder.

### Software Setup

#### ESP32 Version

1. Install the Arduino IDE and set it up for ESP32 development.
2. Install the following libraries:
   - Adafruit_GFX
   - Adafruit_ST7789
   - Adafruit_MLX90614
   - Wire
   - SPI
   - EEPROM
   - Arduino_GFX_Library
   - CST816Touch_SWMode
3. Open the `tires_esp32.ino` sketch in the Arduino IDE.
4. Configure WiFi credentials in the sketch (if needed).
5. Upload the sketch to your ESP32 board.

#### Modular Version

1. Install the Arduino IDE.
2. Install the following libraries:
   - Adafruit_GFX
   - Adafruit_ST7789
   - Adafruit_MLX90614
   - Wire
   - SPI
   - SoftwareSerial (for Bluetooth communication)
3. Open the `tires_modular.ino` sketch in the Arduino IDE.
4. Upload the sketch to your Arduino board.

## Configuration

The system can be configured through the touch interface menu. Swipe left to access the menu and use swipe up/down gestures to navigate. Swipe left to activate a setting. Swipe up/down to increase/decrease values. Swipe right to exit the setting:

1. **Current Mode**: Choose between Street and Track modes
2. **Temperature Scale**: Select Fahrenheit or Celsius
3. **Street Settings**: Configure temperature thresholds for street driving
4. **Track Settings**: Configure temperature thresholds for track driving
5. **Night Brightness**: Adjust the display brightness for night mode
6. **Hardware Settings**: Configure sensor mapping for each tire position
7. **Save Config**: Save current settings to EEPROM

To activate night mode, swipe right on the main display.

## Debugging Tools

- `i2c_scanner`: Utility to scan and identify I2C devices on the bus
- `wifi_test`: Test the WiFi connection capabilities
- `menu_test`: Test the menu system functionality

## Communication Protocol

The system uses a custom NBP (Networked Binary Protocol) for wireless communication with external devices. This allows for remote monitoring of tire temperatures via WiFi or Bluetooth.

## License

This project is open source. Please respect all third-party library licenses.

## Contributing

Contributions to improve the tire temperature monitoring system are welcome. Please ensure your code follows the existing style and includes appropriate documentation.

## Credits

This project uses several open-source libraries:
- Adafruit_GFX and Adafruit_ST7789 for display functionality
- Adafruit_MLX90614 for temperature sensing
- CST816Touch_SWMode for touch interface