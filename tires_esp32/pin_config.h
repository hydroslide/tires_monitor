#pragma once

// Pin map for the Waveshare ESP32-S3-Touch-LCD-1.69 development board.
//
// This file originally shipped inside Waveshare's ESP32-S3-Touch-LCD-1.69_Demo
// bundle, which .gitignore excludes -- so it was never committed and went missing.
// It is now tracked here because tires_esp32.ino includes it with quotes
// (#include "pin_config.h"), which resolves against the sketch folder, not a library.
//
// Values cross-checked against two independent sources that agree exactly:
//   1. https://docs.waveshare.com/ESP32-S3-Touch-LCD-1.69  (wiki pin table)
//   2. arduino-esp32 variants/waveshare_esp32_s3_touch_lcd_169/pins_arduino.h (WS_* defines)
//
// Keep esp32_i2c_scanner/pin_config.h in sync with this file.

#define XPOWERS_CHIP_AXP2101

// LCD -- ST7789V2 over SPI
#define LCD_DC 4
#define LCD_CS 5
#define LCD_SCK 6
#define LCD_MOSI 7
#define LCD_RST 8
#define LCD_BL 15
#define LCD_WIDTH 240
#define LCD_HEIGHT 280

// Shared I2C bus: CST816T touch (0x15), PCF85063 RTC (0x51), QMI8658 IMU (0x6B),
// plus the external TCA9548A mux (0x70) and MLX90614/MLX90640 sensors behind it.
#define IIC_SDA 11
#define IIC_SCL 10

// CST816T capacitive touch controller
#define TP_RST 13
#define TP_INT 14
