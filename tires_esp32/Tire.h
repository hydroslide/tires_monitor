#ifndef TIRE_H
#define TIRE_H

#include <Adafruit_ST7789.h>
#include <Adafruit_GFX.h>

class Tire {
private:
    uint16_t fillColor;

public:
    int x, y, width, height;
    uint16_t outlineColor, textColor;
    char tempUnit;
    float temperature;
    float lastTemp;
    bool initialized = false;
    bool crossedThreshold = true;

    Tire();
    Tire(int _x, int _y, int _width, int _height, uint16_t _outlineColor, uint16_t _textColor, char _tempUnit);
    void draw(bool force=false);
    void printTemp();
    void setTemp(float temp, float minTemp, float idealTemp, float maxTemp, uint16_t lowColor, uint16_t normalColor, uint16_t idealColor, uint16_t highColor, uint16_t lowTextColor, uint16_t normalTextColor, uint16_t idealTextColor, uint16_t highTextColor);
};

#endif
