# Hardware Configuration Guide

## Main Board: Waveshare ESP32-S3 Touch LCD 1.69"

The tire temperature monitoring system uses the Waveshare ESP32-S3 Touch LCD 1.69" as the main controller board. This compact, all-in-one development board is ideal for this project due to its integrated display, touch controller, and powerful microcontroller.

### Specifications

- **Microcontroller**: ESP32-S3 dual-core processor
- **Display**: 1.69" round LCD (240×280 resolution) with ST7789 driver
- **Touch Controller**: Built-in CST816S capacitive touch controller with gesture support
- **Memory**: 8MB PSRAM and 16MB Flash
- **Connectivity**: WiFi 802.11 b/g/n and Bluetooth 5 (LE)
- **Interface**:
  - USB Type-C for programming and power
  - I2C, SPI, UART interfaces
  - Multiple GPIO pins
- **Power**: 5V via USB-C
- **Form Factor**: Round PCB that perfectly fits the project's 3D printed case

## Temperature Sensors

The system uses MLX90614 non-contact infrared temperature sensors to measure tire temperatures.

### Specifications

- **Type**: MLX90614 Infrared Thermometer
- **Interface**: I2C
- **Temperature Range**: -70°C to 380°C object temperature
- **Accuracy**: ±0.5°C
- **Resolution**: 0.02°C
- **Field of View**: 90° (standard version)
- **Default I2C Address**: 0x5A (all sensors share the same address)
- **Supply Voltage**: 3.3V

### Connection Setup

Since all MLX90614 sensors share the same I2C address (0x5A), a TCA9548A I2C multiplexer is used to connect multiple sensors:

1. All sensors connect to the TCA9548A multiplexer
2. The multiplexer connects to the ESP32-S3 board via I2C
3. Temperature readings are taken sequentially by selecting different channels on the multiplexer

## I2C Multiplexer

### Specifications

- **Type**: TCA9548A 8-channel I2C multiplexer
- **Interface**: I2C
- **Default Address**: 0x70 (configurable via jumpers)
- **Channels**: 8 independent I2C buses
- **Supply Voltage**: 3.3V

### Connection Diagram

```
ESP32-S3 Board (I2C) ---> TCA9548A Multiplexer --+---> MLX90614 (Front Left)
                                                 |
                                                 +---> MLX90614 (Front Right)
                                                 |
                                                 +---> MLX90614 (Rear Left)
                                                 |
                                                 +---> MLX90614 (Rear Right)
```

## Power Requirements

- **ESP32-S3 Board**: 5V via USB-C connector
- **I2C Devices**: 3.3V (provided by the ESP32-S3 board)
- **Total Power Consumption**: ~150mA during normal operation

## Assembly Notes

1. **Display Orientation**: The LCD is oriented in landscape mode (rotation 3 in the code)
2. **Sensor Placement**: Each MLX90614 sensor should be mounted in a position where it has a clear line of sight to the tire sidewall
3. **Wiring**: Use shielded cables for I2C connections to minimize interference
4. **Enclosure**: The 3D printed parts in the `/3d_parts/final parts/` directory are designed specifically for this hardware configuration

## USB Connection

The ESP32-S3 board connects to a computer via USB-C for:
- Programming (uploading code)
- Serial debugging (9600 baud)
- Power supply

## Troubleshooting Hardware Issues

### Common Problems

1. **No Temperature Readings**:
   - Check I2C connections
   - Verify multiplexer address (should be 0x70)
   - Use the `i2c_scanner.ino` utility to detect connected devices

2. **Touch Not Responding**:
   - Check CST816S initialization in the code
   - Verify the touch settings match the display orientation

3. **Display Issues**:
   - Verify SPI connections
   - Check display initialization parameters in the code
   - Ensure proper power supply