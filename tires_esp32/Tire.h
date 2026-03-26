#ifndef TIRE_H
#define TIRE_H

#include <Adafruit_ST7789.h>
#include <Adafruit_GFX.h>

class Tire {


public:
    int x, y, width, height;
    int bufferPix;
    uint16_t outlineColor, textColor;
    char tempUnit;
    float temperature;
    float lastTemp;
    bool initialized = false;
    bool crossedThreshold = true;
    byte tireIndex;

    virtual ~Tire();
    Tire();
    Tire(int _x, int _y, int _width, int _height, int _bufferPix, uint16_t _outlineColor, uint16_t _textColor, char _tempUnit);
    virtual void draw(bool force=false, bool textOnly = false);
    
    void printTemp();
    virtual void setTemp(float temp, float minTemp, float idealTemp, float maxTemp, uint16_t lowColor, uint16_t normalColor, uint16_t idealColor, uint16_t highColor, uint16_t lowTextColor, uint16_t normalTextColor, uint16_t idealTextColor, uint16_t highTextColor);
    virtual void setTemps(const float *temps, size_t count, bool isFahrenheit, float minTemp, float idealTemp, float maxTemp, uint16_t lowColor,  uint16_t normalColor,uint16_t idealColor,  uint16_t highColor,uint16_t lowTextColor, uint16_t normalTextColor, uint16_t idealTextColor,  uint16_t highTextColor);

protected:
    uint16_t fillColor;
    virtual void initialize(); 
};

#endif
