#pragma once

// Pin map for the Waveshare ESP32-S3-Touch-LCD-1.69 development board.
//
// Duplicated from tires_esp32/pin_config.h. Arduino sketches can only include
// headers from their own folder, so each sketch keeps its own copy.
// If you change one, change the other.
//
// Values cross-checked against two independent sources that agree exactly:
//   1. https://docs.waveshare.com/ESP32-S3-Touch-LCD-1.69  (wiki pin table)
//   2. arduino-esp32 variants/waveshare_esp32_s3_touch_lcd_169/pins_arduino.h (WS_* defines)

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
