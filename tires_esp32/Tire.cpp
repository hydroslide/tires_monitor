#include "Tire.h"
#include <Fonts/FreeMonoBold24pt7b.h>

extern Adafruit_ST7789 tft;
extern HWCDC USBSerial;

Tire::Tire() {}

Tire::~Tire() {}

Tire::Tire(int _x, int _y, int _width, int _height, int _bufferPix, uint16_t _outlineColor, uint16_t _textColor, char _tempUnit)
    : x(_x), y(_y), width(_width), height(_height), bufferPix(_bufferPix), outlineColor(_outlineColor), textColor(_textColor), temperature(0), tempUnit(_tempUnit) {initialize();}

void Tire::draw(bool force, bool textOnly) {
    if ((int)temperature != (int)lastTemp || force || temperature==0.0) {
        if ((lastTemp>=100 && temperature<100) || (lastTemp<100 && temperature>=100) || (lastTemp<0 && temperature>=0) || (lastTemp>=0 && temperature<0))
            force=true;

        int radius = 20;
        //USBSerial.println("Before fillRect");        
        if ((!textOnly) && (force || crossedThreshold)) tft.fillRoundRect(x, y, width, height, radius, fillColor);
        //USBSerial.println("Before printTemp");
        printTemp();
        //USBSerial.println("Before drawRoundRect");
        if ((!textOnly) && (force || crossedThreshold)) tft.drawRoundRect(x, y, width, height, radius, outlineColor);
    }
}

void Tire::initialize(){}

void Tire::printTemp() {
    int tempInt = (int)temperature;
    String tempString = String(tempInt) + (char)0xF7 + tempUnit;

    uint16_t textWidth, textHeight;
    int16_t c_x, c_y;
    
    tft.setFont(nullptr);
    tft.setTextSize(4);
    tft.getTextBounds(tempString, 0, 0, &c_x, &c_y, &textWidth, &textHeight);

    int startX = x + (width - textWidth) / 2;
    int startY = y + (height / 2) - (textHeight / 2);
    tft.setCursor(startX, startY);
    tft.setTextColor(textColor, fillColor);

    // tft.setFont(&FreeMonoBold24pt7b);
    // tft.setTextSize(1);
    // tft.fillRect(startX, startY, textWidth, textHeight, fillColor);
    // tft.println(tempString);

    tft.println(tempString);

    lastTemp = temperature;
}

void Tire::setTemp(float temp, float minTemp, float idealTemp, float maxTemp, uint16_t lowColor, uint16_t normalColor, uint16_t idealColor, uint16_t highColor, uint16_t lowTextColor, uint16_t normalTextColor, uint16_t idealTextColor, uint16_t highTextColor) {
    
    if (isnan(temp)) temp = 0.0f;
    temperature = temp;

    uint16_t newColor;
    if (temperature < minTemp) {
        newColor = lowColor;
        textColor = lowTextColor;
    } else if (temperature >= idealTemp && temperature<= maxTemp) {
        newColor = idealColor;
        textColor = idealTextColor;
    } else if (temperature > maxTemp) {
        newColor = highColor;
        textColor = highTextColor;
    } else {
        newColor = normalColor;
        textColor = normalTextColor;
    }
    if (newColor != fillColor) {
        fillColor = newColor;
        crossedThreshold = true;
    } else {
        if (initialized) crossedThreshold = false;
        else initialized = true;
    }

    outlineColor=textColor;
}

void Tire::setTemps(const float *temps, size_t count, bool isFahrenheit,
                    float minTemp, float idealTemp, float maxTemp,
                    uint16_t lowColor,    uint16_t normalColor,
                    uint16_t idealColor,  uint16_t highColor,
                    uint16_t lowTextColor,    uint16_t normalTextColor,
                    uint16_t idealTextColor,  uint16_t highTextColor)
{
    // old Tire only handles single‐value:
    if (count >= 1) {
        setTemp( temps[0], minTemp, idealTemp, maxTemp,
                 lowColor,normalColor,idealColor,highColor,
                 lowTextColor,normalTextColor,idealTextColor,highTextColor );
    }
}
