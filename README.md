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

- `/tires_esp32` - **Main implementation** using ESP32 with touch interface and WiFi
- `/docs` - Dev environment setup and build/flash guides
- `/scripts` - `tm.sh` / `tm.ps1` build helpers (compile, flash, monitor, diagnose)
- `/3d_parts` - 3D printable files for enclosures and mounting hardware
- `/circuits` - Circuit diagrams and PCB designs
- `/esp32_i2c_scanner` - I2C scanner for this board; use it to debug the sensor bus
- `/i2c_scanner` - Generic Arduino I2C scanner
- `/menu_test` - Test sketch for menu system
- `/tires_modular` - Older/simplified implementation for Arduino with Bluetooth
- `/wifi_test` - Test sketch for WiFi functionality

## Setup and Installation

### Hardware Setup

1. Assemble the hardware components according to the circuit diagrams in the `/circuits` folder.
2. Connect the MLX90614 sensors to the TCA9548A I2C multiplexer.
3. Connect the ST7789 display via SPI.
4. For ESP32 version, connect the CST816 touch controller via I2C.
5. Print and assemble the enclosures from the 3D models in the `/3d_parts/final parts` folder.

### Software Setup

#### ESP32 Version (main firmware)

Development happens in **VS Code** using `arduino-cli`. The Arduino IDE is not required.

| Guide | For |
|---|---|
| **[docs/DEV-SETUP-MACOS.md](docs/DEV-SETUP-MACOS.md)** | First-time setup on a Mac |
| **[docs/DEV-SETUP-WINDOWS.md](docs/DEV-SETUP-WINDOWS.md)** | First-time setup on Windows |
| **[docs/BUILD-AND-FLASH.md](docs/BUILD-AND-FLASH.md)** | The daily build/flash/monitor loop |

Short version, once set up:

```bash
./scripts/tm.sh flash      # compile + upload   (~1 min)
./scripts/tm.sh monitor    # watch serial output
```

Windows: `.\scripts\tm.ps1 <same>`. Or use the status-bar buttons / `Cmd+Shift+B` in
VS Code.

Core and library versions are pinned in
[`tires_esp32/sketch.yaml`](tires_esp32/sketch.yaml) and installed automatically on the
first build — there is nothing to click through in a Library Manager. For reference, the
firmware depends on:

- ESP32 Arduino core 3.0.7 (bundles `Wire`, `SPI`, `EEPROM`, `WiFi`)
- Adafruit GFX Library, Adafruit ST7735 and ST7789 Library, Adafruit BusIO
- Adafruit MLX90614 Library (spot IR sensors), Adafruit MLX90640 (thermal cameras)
- GFX Library for Arduino
- CST816_TouchLib — supplies `CST816Touch_SWMode`, which is a class in that library, not
  a library of its own

WiFi credentials are `#define`d at the top of `tires_esp32.ino`.

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