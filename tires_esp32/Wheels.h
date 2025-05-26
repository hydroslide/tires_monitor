#ifndef WHEELS_H
#define WHEELS_H

#include <Adafruit_ST7789.h>
#include <Adafruit_GFX.h>
#include "Tire.h"
#define PURPLE 0xE01F
#define DARK_GREEN 	0x06A0//0x06E0//0x0680 //0x05E0

class Wheels {
public:
    Tire frontLeft, frontRight, rearLeft, rearRight;
    float minTemp, idealTemp, maxTemp;
    uint16_t lowTempColor, normalTempColor, idealTempColor, highTempColor, textColor;
    uint16_t lowTempTextColor, normalTempTextColor, idealTempTextColor, highTempTextColor;

    Wheels();
    Wheels(int bufferPix, uint16_t outlineColor, uint16_t _textColor, float _minTemp, float _idealTemp, float _maxTemp, char tempUnit,
               uint16_t _lowTempColor = ST77XX_BLUE, uint16_t _normalTempColor = DARK_GREEN, uint16_t _idealTempColor = PURPLE, uint16_t _highTempColor = ST77XX_RED,
               uint16_t _lowTempTextColor = ST77XX_WHITE, uint16_t _normalTempTextColor = ST77XX_WHITE, uint16_t _idealTempTextColor = ST77XX_YELLOW, uint16_t _highTempTextColor = ST77XX_YELLOW);
    void draw(bool force=false);
    void setTireTemps(float frontLeftTemp, float frontRightTemp, float rearLeftTemp, float rearRightTemp);
    void setup();
    char getTempUnit();
    void setTempUnit(char tempUnit);

private:
    char _tempUnit;
};

#endif
