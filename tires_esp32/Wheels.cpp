#include "Wheels.h"
#include <Adafruit_ST7789.h>
#include <Adafruit_GFX.h>

extern Adafruit_ST7789 tft;


Wheels::Wheels() {}

Wheels::Wheels(int bufferPix, uint16_t outlineColor, uint16_t _textColor, float _minTemp, float _idealTemp, float _maxTemp, char tempUnit,
               uint16_t _lowTempColor , uint16_t _normalTempColor , uint16_t _idealTempColor , uint16_t _highTempColor )
    : textColor(_textColor),
      minTemp(_minTemp), idealTemp(_idealTemp), maxTemp(_maxTemp),
      lowTempColor(_lowTempColor), normalTempColor(_normalTempColor), idealTempColor(_idealTempColor), highTempColor(_highTempColor) {
    int tireWidth = (tft.width() - (bufferPix * 3)) / 2;
    int tireHeight = (tft.height() - (bufferPix * 3)) / 2;
    int tire_0_x = bufferPix;
    int tire_1_x = tire_0_x + tireWidth + bufferPix;
    int tire_0_y = bufferPix;
    int tire_1_y = tire_0_y + tireHeight + bufferPix;

    _tempUnit = tempUnit;
    frontLeft = Tire(tire_0_x, tire_0_y, tireWidth, tireHeight, outlineColor, _textColor, tempUnit);
    frontRight = Tire(tire_1_x, tire_0_y, tireWidth, tireHeight, outlineColor, _textColor, tempUnit);
    rearLeft = Tire(tire_0_x, tire_1_y, tireWidth, tireHeight, outlineColor, _textColor, tempUnit);
    rearRight = Tire(tire_1_x, tire_1_y, tireWidth, tireHeight, outlineColor, _textColor, tempUnit);
}

void Wheels::draw(bool force) {
    frontLeft.draw(force);
    frontRight.draw(force);
    rearLeft.draw(force);
    rearRight.draw(force);
}

void Wheels::setTireTemps(float frontLeftTemp, float frontRightTemp, float rearLeftTemp, float rearRightTemp) {
    frontLeft.setTemp(frontLeftTemp, minTemp, idealTemp, maxTemp, lowTempColor, normalTempColor, idealTempColor, highTempColor);
    frontRight.setTemp(frontRightTemp, minTemp, idealTemp, maxTemp, lowTempColor, normalTempColor, idealTempColor, highTempColor);
    rearLeft.setTemp(rearLeftTemp, minTemp, idealTemp, maxTemp, lowTempColor, normalTempColor, idealTempColor, highTempColor);
    rearRight.setTemp(rearRightTemp, minTemp, idealTemp, maxTemp, lowTempColor, normalTempColor, idealTempColor, highTempColor);
}

    char Wheels::getTempUnit(){ return _tempUnit;}
    void Wheels::setTempUnit(char tempUnit){
        _tempUnit=tempUnit;
        frontLeft.tempUnit=tempUnit;
        frontRight.tempUnit=tempUnit;
        rearLeft.tempUnit=tempUnit;
        rearRight.tempUnit=tempUnit;
    }

void Wheels::setup(){
    
}
