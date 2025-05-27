# I2C Scanner Utility

This directory contains a simple utility sketch for scanning and identifying I2C devices connected to your microcontroller. This tool is essential for troubleshooting the tire temperature monitoring system's sensor connections.

## Purpose

The I2C scanner helps with:

1. Verifying that all MLX90614 temperature sensors are properly connected
2. Checking that the TCA9548A I2C multiplexer is working correctly
3. Identifying the addresses of connected I2C devices
4. Troubleshooting connection issues with the CST816 touch controller

## Usage

1. Upload the `i2c_scanner.ino` sketch to your Arduino or ESP32 board.
2. Open the Serial Monitor at 9600 baud.
3. The sketch will scan all possible I2C addresses (1-127) and report any devices found.
4. Found devices will be displayed with their hexadecimal addresses.

## Expected Output for the Tire Temperature System

When connected to the tire temperature monitoring system hardware, you should see:

- The TCA9548A I2C multiplexer (typically at address 0x70)
- If the CST816 touch controller is connected, it should appear (typically at address 0x15)
- When testing with the multiplexer, the MLX90614 temperature sensors may appear at address 0x5A

## Testing with the TCA9548A Multiplexer

To test sensors connected through the multiplexer, you'll need to:

1. Note the addresses found with this scanner
2. Modify the `select_I2C_bus()` function in the `TempReader.cpp` file if necessary
3. Test each multiplexer channel to ensure all sensors are detected

## Troubleshooting

If no devices are detected:
1. Check your wiring connections (SDA/SCL pins)
2. Verify that pull-up resistors are present (typically 4.7kΩ)
3. Ensure devices are properly powered
4. Try a different I2C speed if available

If some devices are missing:
1. Check individual connections
2. Verify that the TCA9548A multiplexer is working
3. Ensure each sensor has a unique address or is on a separate multiplexer channel

## Code Attribution

This I2C scanner is based on commonly shared code with improvements by various contributors. The version history is documented in the comments within the sketch.