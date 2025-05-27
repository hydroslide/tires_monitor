# ESP32 Tire Temperature Monitor

This directory contains the main implementation of the Tire Temperature Monitor system using an ESP32 microcontroller. This version features a touch interface, WiFi connectivity, and an advanced menu system.

## Features

- ESP32-based implementation with full touch interface
- ST7789 LCD display (240x280 pixels) 
- CST816 touch controller for gesture support
- WiFi connectivity for remote monitoring
- Menu system for configuration
- EEPROM storage for persistent settings
- Night mode with configurable brightness
- Support for both Fahrenheit and Celsius temperature scales
- Two modes (Street and Track) with different temperature thresholds

## Hardware Requirements

- Waveshare ESP32-S3 Touch LCD 1.69" development board
  - ESP32-S3 dual-core microcontroller
  - 1.69" round LCD display (240×280 resolution) with ST7789 driver
  - Built-in CST816S capacitive touch controller with gesture support
  - 8MB PSRAM and 16MB Flash memory
  - USB Type-C interface for programming and power
  - WiFi and Bluetooth connectivity
  - I2C, SPI, and GPIO interfaces
  - Compact round form factor that perfectly fits the project needs
- MLX90614 infrared temperature sensors (4x, one for each tire)
- TCA9548A I2C multiplexer (to connect multiple sensors with the same I2C address)
- 3D printed enclosure (see the `/3d_parts` directory)

## Pin Configuration

The pin configuration is defined in the `pin_config.h` file (not shown in the repository listing). Refer to the code for the specific pin assignments.

## Libraries

The following libraries are required:

- Adafruit_GFX
- Adafruit_ST7789
- SPI
- Wire
- EEPROM
- Arduino_GFX_Library
- CST816Touch_SWMode

## Core Files

- `tires_esp32.ino` - Main Arduino sketch file
- `Tire.h/cpp` - Implements individual tire display and behavior
- `Wheels.h/cpp` - Manages the layout of four tires on the display
- `TempReader.h/cpp` - Handles reading temperatures from MLX90614 sensors
- `NBPProtocol.h/cpp` - Implements the Networked Binary Protocol for remote communication
- `WifiSerial.h/cpp` - Provides WiFi communication capabilities
- `TireMenu.h/cpp` - Defines menu structure and configuration options
- `MenuSystem.h/cpp` - Implements the menu navigation system
- `MenuRenderer.h/cpp` - Handles drawing the menu on the display
- `TouchMenuHandler.h/cpp` - Processes touch events for menu navigation

## Menu System

The menu system allows configuration of:

1. Mode (Street or Track)
2. Temperature scale (Fahrenheit or Celsius)
3. Temperature thresholds for Street mode
4. Temperature thresholds for Track mode
5. Night mode brightness
6. Temperature sensor mapping for each tire

## WiFi Setup

The system creates its own WiFi access point:
### Incorrect info
- SSID: TireTempMonitor
- Password: esp32
- Port: 8080

## Usage

1. Power on the device.
2. Temperatures will be displayed in color-coded format.
3. Swipe left to enter the menu system.
4. Swipe right on the main screen to toggle night mode.
5. Connect to the WiFi access point to access remote monitoring.

## Troubleshooting

If the system is not detecting temperature sensors:
1. Use the `i2c_scanner` sketch to verify sensor connections.
2. Check the I2C multiplexer wiring.
3. Verify the correct sensor indices are configured in the menu.

If the touch interface is not responding:
1. Check the I2C connection to the CST816 controller.
2. Verify the `TP_INT` and `TP_RST` pins are correctly wired.