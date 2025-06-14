#include "ThreeSectionTire.h"
#include <Arduino.h>          // for round()
extern Adafruit_ST7789 tft;   // from your main sketch
#include <Fonts/FreeSans9pt7b.h>
#include <Fonts/FreeSans12pt7b.h>
#include <Fonts/FreeSansBold12pt7b.h>
#include <Fonts/FreeSans18pt7b.h>
#include <Fonts/FreeMono9pt7b.h>
#include <Fonts/FreeMono18pt7b.h>
#include <Fonts/FreeMonoBold18pt7b.h>

extern HWCDC USBSerial;

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
   if (fCol != sectionFillColors[idx]) {
        crossedThreshold = true;
    } else {
      shouldResetThreshold=true;
    }
  sectionFillColors[idx] = fCol;
}

void ThreeSectionTire::draw(bool force) {


    // if (drawsSinceForce>= forceInterval){      
    //   force=true;
    // }
    // drawsSinceForce++;

  // Only redraw if forced or temp changed
  static float lastTemps[3] = { NAN, NAN, NAN };
  bool changed = force;
  static float sectionChanged[3] = { force,force,force };
  for (int i = 0; i < 3; i++) {
    if ( (int)round(sectionTemps[i]) !=  (int)round(lastTemps[i])) {
      changed = true;
      sectionChanged[i]=true;
      float lastTemp = lastTemps[i];
      float temperature = sectionTemps[i];
      if ((lastTemp>=100 && temperature<100) || (lastTemp<100 && temperature>=100) || (lastTemp<0 && temperature>=0) || (lastTemp>=0 && temperature<0))
            crossedThreshold=true;
    }
  }

      bool rectsDrawn = false;
    int bandW = width / 3;

  if (changed){


    if (force || crossedThreshold){
      tft.fillRect(x-bufferPix, y-bufferPix, width+(bufferPix*2), height+(bufferPix*2), ST77XX_BLACK);

      // fill three vertical bands

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
      rectsDrawn=true;
      drawsSinceForce=0;
    }
  

    // draw each temperature string center-aligned in its band
    
      //tft.setFont(&FreeSansBold12pt7b);
    //  tft.setFont(&FreeSans12pt7b);
    //tft.setFont(&FreeSans18pt7b);
    //tft.setFont(&FreeMono18pt7b);
    tft.setFont(&FreeMonoBold18pt7b);
    //tft.setFont(&FreeMono9pt7b);
    tft.setTextSize(1);
    //tft.setTextDatum(MC_DATUM);  // center-middle
    for (int i = 0; i < 3; i++) {
      if (sectionChanged[i]){
        char buf[8];
        int tInt = (int)round(sectionTemps[i]);
        //snprintf(buf, sizeof(buf), "%d%c", tInt, tempUnit);

        int cx = x + i*bandW + bandW/2;
        int cy = y + height/2;

        if (!rectsDrawn){
          // Redraw the last temp with background color
          tft.setTextColor(sectionFillColors[i], sectionFillColors[i]);    
          printTemp(lastTemps[i], i, bandW);
        }

        tft.setTextColor(sectionTextColors[i], sectionFillColors[i]);    
        String tempString = printTemp(sectionTemps[i], i, bandW);
        
        // USBSerial.print(i);
        // USBSerial.print(": `");
        // USBSerial.print(tempString);
        // USBSerial.print("`: ");
        // USBSerial.print(textWidth);
        // USBSerial.print("`: ");
        // USBSerial.print(c_x);
        // USBSerial.print(" | ");
        //tft.setCursor(cx,cy);//startX, y + (height / 2) - (textHeight / 2));
        //tft.println(buf);

        //tft.drawString(buf, cx, cy);
         lastTemps[i] = sectionTemps[i];
      }     
    }
  }
  if (shouldResetThreshold){
    shouldResetThreshold=false;
    if (initialized) crossedThreshold = false;
      else initialized = true;
  }
  USBSerial.println("");
}

String ThreeSectionTire::printTemp(int temp, int i, int bandW){
      
    String tempString = String(temp) ;//+ (char)0xF7 + tempUnit;
    uint16_t textWidth, textHeight;    
    int16_t c_x, c_y;
    tft.getTextBounds(tempString, 0, 0, &c_x, &c_y, &textWidth, &textHeight);
    int startX = (x+bandW*i);// - ((textWidth/2)+(bandW/2));
    //int yMod = (i==0)? (textHeight*-1):((i==2)?textHeight:0);
    int yMod = (i==0 || i==2)? (textHeight):0;
    int startY = (y + ((height+textHeight) / 2))+yMod;// - (textHeight / 2);
    tft.setCursor(startX, startY);
    tft.println(tempString);
    return tempString;
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
