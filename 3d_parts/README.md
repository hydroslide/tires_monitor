# 3D Printable Parts

This directory contains STL files for 3D printing enclosures and mounting components for the tire temperature monitoring system.

## Directory Structure

- `final parts/` - Latest production-ready designs
  - `display/` - Enclosures and mounting for the Waveshare ESP32-S3 Touch LCD 1.69"
- `old/` - Previous iterations and obsolete designs
  - `Display/` - Older display module designs
- `src/` - Source files for component design
- `tests/` - Test prints and prototypes

## Components

### Display Module

The display module components are specifically designed for the Waveshare ESP32-S3 Touch LCD 1.69" round development board. These parts create a protective housing and mounting system for the display unit:
- `Tire Temp Display Module 4 back.stl` - Back cover that protects the electronics
- `Tire Temp Display Module 4 front.stl` - Front bezel that secures the 1.69" round LCD
- `Tire Temp Display Module 4 middle.stl` - Middle housing that contains the ESP32-S3 board
- `Tire Temp Display Module high cup thick.stl` - Mounting cup for vehicle installation
- `Tire Temp Display Module nut.stl` - Threaded nut for secure mounting

### Sensor Boxes

These enclosures protect the MLX90614 infrared temperature sensors:
- `Tire Temp Sensor Box protection mk2.stl` - Protective case for the sensor
- `Tire Temp Sensor Lid.stl` - Lid that secures the sensor while allowing temperature readings

### I2C Hub Box

This enclosure houses the TCA9548A I2C multiplexer board:
- `I2C Hub Box.stl` - Main housing for the multiplexer PCB
- `I2C Hub Box lid.stl` - Lid that protects the multiplexer connections

### Power Input Box

Power supply enclosure for 5V regulation:
- `Power Input 5v Box.stl` - Housing for power regulation components
- `Power Input 5v Lid.stl` - Protective lid for the power supply

## Assembly Instructions

1. Insert the Waveshare ESP32-S3 Touch LCD 1.69" board into the `Tire Temp Display Module 4 front.stl` with the screen facing outward.
2. Secure the board in the `Tire Temp Display Module 4 middle.stl` housing.
3. Route the I2C and power wires through the appropriate channels and secure the back panel.
4. For each sensor, place the MLX90614 infrared sensor inside the `Tire Temp Sensor Box protection mk2.stl` and secure with the lid.
5. Mount the TCA9548A I2C multiplexer inside the I2C Hub Box and secure the lid.
6. Assemble the power input box with the appropriate power regulation components if not powering directly via the ESP32-S3's USB-C port.

## Wiring Instructions

The system uses standardized 4-pin connectors for all connections:

1. **ESP32 to Power Supply Box**:
   - Connect a 4-pin wire (5V, GND, SDA, SCL) from the ESP32-S3 board to the power supply box.
   
2. **Power Supply**:
   - The power supply box accepts power via a USB-Mini connector.
   
3. **Power Supply to I2C Multiplexer**:
   - Connect another 4-pin wire from the output of the power supply box to the input of the I2C multiplexer box.
   
4. **I2C Multiplexer to Sensors**:
   - Connect four separate 4-pin wires from the multiplexer box outputs.
   - Each wire routes to one of the four tire temperature sensors.
   - Each wire carries 5V, GND, SDA, and SCL to power and communicate with the MLX90614 sensors.
   
5. **Wire Routing**:
   - Route wires along the vehicle's existing wiring harnesses when possible.
   - Use cable ties or clips to secure wires away from heat sources and moving parts.

## Notes

- The 3D printed parts were designed specifically for the Waveshare ESP32-S3 Touch LCD 1.69" board's round form factor
- Older designs in the `old/` directory are kept for reference but are optimized for different hardware
- The `tests/` directory contains prototype designs that were used during development
- The source files in `src/` can be modified if customization is required