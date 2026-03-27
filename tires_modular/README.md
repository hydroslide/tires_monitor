# Modular Tire Temperature Monitor

This directory contains a simplified implementation of the Tire Temperature Monitor system designed for Arduino-based microcontrollers. This version uses Bluetooth for communication and does not include the touch interface or menu system of the ESP32 version.

## Features

- Arduino-based implementation with ST7789 display
- Bluetooth communication via HC-05 module
- MLX90614 infrared temperature sensors
- Fixed temperature thresholds (not configurable without code changes)
- Color-coded temperature display

## Hardware Requirements

- Arduino board (e.g., Uno, Nano, Mega)
- ST7789 LCD display (240x280 pixels)
- HC-05 Bluetooth module
- MLX90614 infrared temperature sensors (4x)
- TCA9548A I2C multiplexer (for connecting multiple sensors)
- 3D printed enclosure (see the `/3d_parts` directory)

## Pin Configuration

- TFT_CS: 10
- TFT_RST: 8
- TFT_DC: 7
- Bluetooth TX: 2
- Bluetooth RX: 3
- I2C: Default pins (SDA/SCL)

## Libraries

The following libraries are required:

- Adafruit_GFX
- Adafruit_ST7789
- SPI
- Wire
- SoftwareSerial (for Bluetooth communication)

## Core Files

- `tires_modular.ino` - Main Arduino sketch file
- `Tire.h/cpp` - Implements individual tire display and behavior
- `Wheels.h/cpp` - Manages the layout of four tires on the display
- `TempReader.h/cpp` - Handles reading temperatures from MLX90614 sensors
- `NBPProtocol.h/cpp` - Implements the Networked Binary Protocol for remote communication

## Default Configuration

This version uses fixed temperature thresholds:
- Minimum temperature: 75°F
- Ideal temperature: 85°F
- Maximum temperature: 100°F
- Temperature scale: Fahrenheit

To modify these values, edit lines 63-64 in `tires_modular.ino`.

## Bluetooth Communication

The HC-05 Bluetooth module should be configured to operate at 9600 baud rate. The system will send temperature data using the NBP protocol, which can be received by compatible applications.

## Usage

1. Power on the device.
2. Temperatures will be displayed in color-coded format.
3. Connect to the device via Bluetooth to receive temperature data.

## Differences from ESP32 Version

- No touch interface
- No configuration menu
- Fixed temperature thresholds
- No WiFi (Bluetooth only)
- No night mode
- Simplified display layout
- No EEPROM configuration storage

## Troubleshooting

If the system is not detecting temperature sensors:
1. Use the `i2c_scanner` sketch to verify sensor connections.
2. Check the I2C multiplexer wiring.

If Bluetooth communication is not working:
1. Verify the HC-05 module is properly configured (AT commands).
2. Check that the SoftwareSerial pins are correctly connected.
3. Test with a simple Bluetooth terminal app to verify connectivity.