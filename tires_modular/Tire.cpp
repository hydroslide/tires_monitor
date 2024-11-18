#include "Tire.h"

extern Adafruit_ST7789 tft;

Tire::Tire() {}

Tire::Tire(int _x, int _y, int _width, int _height, uint16_t _outlineColor, uint16_t _textColor, char _tempUnit)
    : x(_x), y(_y), width(_width), height(_height), outlineColor(_outlineColor), textColor(_textColor), temperature(0), tempUnit(_tempUnit) {}

void Tire::draw(bool force=false) {
    if ((int)temperature != (int)lastTemp) {
        int radius = 20;
        if (force || crossedThreshold) tft.fillRoundRect(x, y, width, height, radius, fillColor);
        printTemp();
        if (force || crossedThreshold) tft.drawRoundRect(x, y, width, height, radius, outlineColor);
    }
}

void Tire::printTemp() {
    int tempInt = (int)temperature;
    String tempString = String(tempInt) + (char)0xF7 + tempUnit;

    int16_t textWidth, textHeight;
    tft.setTextSize(4);
    tft.getTextBounds(tempString, 0, 0, NULL, NULL, &textWidth, &textHeight);

    int startX = x + (width - textWidth) / 2;
    tft.setCursor(startX, y + (height / 2) - (textHeight / 2));
    tft.setTextColor(textColor, fillColor);
    tft.println(tempString);

    lastTemp = temperature;
}

void Tire::setTemp(float temp, float minTemp, float idealTemp, float maxTemp, uint16_t lowColor, uint16_t normalColor, uint16_t idealColor, uint16_t highColor) {
    if (isnan(temp)) temp = 0.0f;
    temperature = temp;

    uint16_t newColor;
    if (temperature < minTemp) {
        newColor = lowColor;
    } else if (temperature >= idealTemp && temperature<= maxTemp) {
        newColor = idealColor;
    } else if (temperature > maxTemp) {
        newColor = highColor;
    } else {
        newColor = normalColor;
    }
    if (newColor != fillColor) {
        fillColor = newColor;
        crossedThreshold = true;
    } else {
        if (initialized) crossedThreshold = false;
        else initialized = true;
    }
}
