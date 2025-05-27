# Circuit Diagrams and PCB Designs

This directory contains circuit diagrams and PCB images for the tire temperature monitoring system.

## Contents

- `i2c bottom.JPG` - Bottom layer of the I2C multiplexer PCB
- `i2c top.JPG` - Top layer of the I2C multiplexer PCB

## I2C Multiplexer Board

The I2C multiplexer board is a critical component of the tire temperature monitoring system, allowing multiple MLX90614 temperature sensors (which share the same I2C address) to be connected to a single microcontroller.

### Features

- Based on the TCA9548A I2C multiplexer chip
- Supports up to 8 I2C devices with the same address
- Breakout headers for each multiplexed channel
- Address selection jumpers
- Power status LED

### Connection Guide

1. **Main I2C Bus** - Connect to the microcontroller's SDA/SCL pins
2. **Channels 0-7** - Connect to the individual MLX90614 temperature sensors
3. **Address Jumpers** - Configure to change the multiplexer's I2C address if needed
4. **Power** - Connect to 3.3V power source

### Implementation Notes

- In the tire temperature monitoring system, typically 4 channels are used (one for each tire)
- Channel mapping can be configured through the menu system in the ESP32 version
- Default sensor mapping in the code:
  - Front Left: Channel 0
  - Front Right: Channel 7
  - Rear Left: Channel 3
  - Rear Right: Channel 4

## PCB Manufacturing

If you need to reproduce the PCB:

1. Use the JPG images as reference
2. Standard 2-layer PCB with FR4 substrate
3. Minimum trace width: 8mil
4. Minimum drill size: 0.3mm

## General Wiring Guidelines

For the complete tire temperature monitoring system:

1. **Power Supply**:
   - ESP32/Arduino: 5V input
   - TCA9548A: 3.3V (use voltage regulator if needed)
   - MLX90614 sensors: 3.3V
   - ST7789 display: 3.3V logic, 5V backlight

2. **I2C Connections**:
   - Connect microcontroller I2C pins to TCA9548A input
   - Connect MLX90614 sensors to TCA9548A outputs
   - Use 4.7kΩ pull-up resistors on SDA/SCL lines

3. **Display Connections**:
   - Connect display via SPI
   - Configure CS, DC, RST pins as defined in the code

4. **Touch Controller** (ESP32 version only):
   - Connect CST816 touch controller via I2C
   - Connect interrupt and reset pins as defined in the code

## Related Files

For more details on the hardware implementation, refer to:
- `/tires_esp32/TempReader.cpp` - Shows how the I2C multiplexer is addressed in code
- `/3d_parts/final parts/I2C Hub Box.stl` - 3D printable enclosure for this PCB