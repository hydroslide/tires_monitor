#ifndef WHEELS_H
#define WHEELS_H

#include <Adafruit_ST7789.h>
#include <Adafruit_GFX.h>
#include "Tire.h"

class Wheels {
public:
    Tire frontLeft, frontRight, rearLeft, rearRight;
    float minTemp, maxTemp;
    uint16_t lowTempColor, normalTempColor, highTempColor, textColor;

    Wheels();
    Wheels(int bufferPix, uint16_t outlineColor, uint16_t _textColor, float _minTemp, float _maxTemp, char tempUnit,
           uint16_t _lowTempColor = ST77XX_BLUE, uint16_t _normalTempColor = ST77XX_GREEN, uint16_t _highTempColor = ST77XX_RED);
    void draw(bool force=false);
    void setTireTemps(float frontLeftTemp, float frontRightTemp, float rearLeftTemp, float rearRightTemp);
    void setup();
};

#endif
