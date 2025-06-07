#include "ThreeSectionTire.h"
#include <Arduino.h>          // for round()
extern Adafruit_ST7789 tft;   // from your main sketch
#include <Fonts/FreeSans9pt7b.h>
#include <Fonts/FreeSans12pt7b.h>
#include <Fonts/FreeMono9pt7b.h>

void ThreeSectionTire::setSectionTemps(const float temps[3],
                                       bool isFahrenheit,
                                       float minTemp, float idealTemp, float maxTemp,
                                       uint16_t lowColor,    uint16_t normalColor,
                                       uint16_t idealColor,  uint16_t highColor,
                                       uint16_t lowTextColor,    uint16_t normalTextColor,
                                       uint16_t idealTextColor,  uint16_t highTextColor)
{
  for (int i = 0; i < 3; i++) {
    float t = temps[i];
    // convert if needed
    //float tempC = isFahrenheit ? (t - 32.0f) * 5.0f / 9.0f : t;
    sectionTemps[i] = t;
    classifyOne(i, t,
                minTemp, idealTemp, maxTemp,
                lowColor, normalColor,
                idealColor, highColor,
                lowTextColor, normalTextColor,
                idealTextColor, highTextColor);
  }
}

// apply Tire’s threshold logic to one section
void ThreeSectionTire::classifyOne(int idx, float temp,
                                   float minTemp, float idealTemp, float maxTemp,
                                   uint16_t lowColor,    uint16_t normalColor,
                                   uint16_t idealColor,  uint16_t highColor,
                                   uint16_t lowTextColor,    uint16_t normalTextColor,
                                   uint16_t idealTextColor,  uint16_t highTextColor)
{
  uint16_t  fCol;
  uint16_t &tCol = sectionTextColors[idx];

  if (temp < minTemp) {
    fCol = lowColor;    tCol = lowTextColor;
  } else if (temp > maxTemp) {
    fCol = highColor;   tCol = highTextColor;
  } else if (temp >= idealTemp) {
    fCol = idealColor;  tCol = idealTextColor;
  } else {
    fCol = normalColor; tCol = normalTextColor;
  }
  sectionFillColors[idx] = fCol;
}

void ThreeSectionTire::draw(bool force) {
  // Only redraw if forced or temp changed
  static float lastTemps[3] = { NAN, NAN, NAN };
  bool changed = force;
  for (int i = 0; i < 3; i++) {
    if ((int)sectionTemps[i] != (int)lastTemps[i]) {
      changed = true;
      lastTemps[i] = sectionTemps[i];
    }
  }
  if (!changed) return;

  // fill three vertical bands
  int bandW = width / 3;
  bool triBand = false;
  if (triBand){
        int bx = x;
        int bw = bandW*2;
        tft.fillRoundRect(bx, y, bw, height, 8, sectionFillColors[0]);

        bw = (width - 2*bandW);
        bx = x + bandW;        
        tft.fillRect(bx, y, bw, height, sectionFillColors[2]);

        bw = bandW;        
        tft.fillRoundRect(bx, y, bw, height, 8, sectionFillColors[1]);
        
  }else{
    for (int i = 0; i < 3; i++) {
        int bx = x + i * bandW;
        int bw = (i == 2) ? (width - 2*bandW) : bandW;  // ensure full coverage
        tft.fillRoundRect(bx, y, bw, height, 8, sectionFillColors[i]);
    }  
  }

  // draw outer outline
  tft.drawRoundRect(x, y, width, height, 8, outlineColor);

  // draw each temperature string center-aligned in its band
  
    tft.setFont(&FreeSans12pt7b);
  //tft.setFont(&FreeMono9pt7b);
  tft.setTextSize(1);
  //tft.setTextDatum(MC_DATUM);  // center-middle
  for (int i = 0; i < 3; i++) {
    char buf[8];
    int tInt = (int)round(sectionTemps[i]);
    //snprintf(buf, sizeof(buf), "%d%c", tInt, tempUnit);

    int cx = x + i*bandW + bandW/2;
    int cy = y + height/2;
    tft.setTextColor(sectionTextColors[i], sectionFillColors[i]);

    
    String tempString = String(tInt) + (char)0xF7 + tempUnit;

    uint16_t textWidth, textHeight;
    
    int16_t c_x, c_y;
    //tft.setTextSize(4);
    tft.getTextBounds(tempString, 0, 0, &c_x, &c_y, &textWidth, &textHeight);
    int startX = (x+bandW*i) + (bandW - textWidth) / 2;
    int yMod = (i==0)? (textHeight*-1):((i==2)?textHeight:0);
    int startY = (y + (height / 2))+yMod;// - (textHeight / 2);
    tft.setCursor(startX, startY);
    tft.println(tempString);
    
    
    //tft.setCursor(cx,cy);//startX, y + (height / 2) - (textHeight / 2));
    //tft.println(buf);

    //tft.drawString(buf, cx, cy);
  }
}

void ThreeSectionTire::setTemps(const float *temps, size_t count, bool isFahrenheit, float minTemp, float idealTemp, float maxTemp, uint16_t lowColor,  uint16_t normalColor,uint16_t idealColor,  uint16_t highColor,uint16_t lowTextColor, uint16_t normalTextColor, uint16_t idealTextColor,  uint16_t highTextColor)
{
    if (count == 3) {
        setSectionTemps(temps, isFahrenheit,
                        minTemp,idealTemp,maxTemp,
                        lowColor,normalColor,
                        idealColor,highColor,
                        lowTextColor,normalTextColor,
                        idealTextColor,highTextColor);
    } else if (count >= 1) {
        // fallback
        setTemp(temps[0], minTemp,idealTemp,maxTemp,
                lowColor,normalColor,idealColor,highColor,
                lowTextColor,normalTextColor,idealTextColor,highTextColor);
    }
}
