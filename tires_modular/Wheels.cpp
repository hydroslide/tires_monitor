#include "Wheels.h"
#include <Adafruit_ST7789.h>
#include <Adafruit_GFX.h>

extern Adafruit_ST7789 tft;

Wheels::Wheels() {}

Wheels::Wheels(int bufferPix, uint16_t outlineColor, uint16_t _textColor, float _minTemp, float _maxTemp, char tempUnit,
               uint16_t _lowTempColor = ST77XX_BLUE, uint16_t _normalTempColor = ST77XX_GREEN, uint16_t _highTempColor = ST77XX_RED)
    : textColor(_textColor),
      minTemp(_minTemp), maxTemp(_maxTemp),
      lowTempColor(_lowTempColor), normalTempColor(_normalTempColor), highTempColor(_highTempColor) {
    int tireWidth = (tft.width() - (bufferPix * 3)) / 2;
    int tireHeight = (tft.height() - (bufferPix * 3)) / 2;
    int tire_0_x = bufferPix;
    int tire_1_x = tire_0_x + tireWidth + bufferPix;
    int tire_0_y = bufferPix;
    int tire_1_y = tire_0_y + tireHeight + bufferPix;

    frontLeft = Tire(tire_0_x, tire_0_y, tireWidth, tireHeight, outlineColor, _textColor, tempUnit);
    frontRight = Tire(tire_1_x, tire_0_y, tireWidth, tireHeight, outlineColor, _textColor, tempUnit);
    rearLeft = Tire(tire_0_x, tire_1_y, tireWidth, tireHeight, outlineColor, _textColor, tempUnit);
    rearRight = Tire(tire_1_x, tire_1_y, tireWidth, tireHeight, outlineColor, _textColor, tempUnit);
}

void Wheels::draw(bool force=false) {
    frontLeft.draw(force);
    frontRight.draw(force);
    rearLeft.draw(force);
    rearRight.draw(force);
}

void Wheels::setTireTemps(float frontLeftTemp, float frontRightTemp, float rearLeftTemp, float rearRightTemp) {
    frontLeft.setTemp(frontLeftTemp, minTemp, maxTemp, lowTempColor, normalTempColor, highTempColor);
    frontRight.setTemp(frontRightTemp, minTemp, maxTemp, lowTempColor, normalTempColor, highTempColor);
    rearLeft.setTemp(rearLeftTemp, minTemp, maxTemp, lowTempColor, normalTempColor, highTempColor);
    rearRight.setTemp(rearRightTemp, minTemp, maxTemp, lowTempColor, normalTempColor, highTempColor);
}

void Wheels::setup(){
    
}
