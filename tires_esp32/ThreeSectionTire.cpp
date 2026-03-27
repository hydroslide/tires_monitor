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
    int roundedTemp =  (int)round(t);
    sectionTemps[i] = roundedTemp;
    classifyOne(i, roundedTemp,
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

void ThreeSectionTire::initialize(){
   for (int i = 0; i < 3; i++) {
    lastDeltaColors[i] = ST77XX_BLACK;
    currentDeltaColors[i] = normalDeltaColor;
    lastSectionFillColors[i] = ST77XX_BLACK;
   }
}

bool ThreeSectionTire::anySectionColorChanged(){
    for (int i = 0; i < 3; i++) {
      if (sectionFillColors[i] != lastSectionFillColors[i])
        return true;
    }
}

void ThreeSectionTire::draw(bool force, bool textOnly) {


    // if (drawsSinceForce>= forceInterval){      
    //   force=true;
    // }
    // drawsSinceForce++;

  // Only redraw if forced or temp changed  
  bool changed = force;
  bool sectionChanged[3];
  for (int i = 0; i < 3; i++) {
    sectionChanged[i]=force;
    int lastTemp = lastTemps[i];
    int temperature = sectionTemps[i];
    if (temperature > 300 || temperature < -30)
      return; // F
    if (lastTemp != temperature) {
      changed = true;
      sectionChanged[i]=true;

      if ((lastTemp>=100 && temperature<100) || (lastTemp<100 && temperature>=100) || (lastTemp<0 && temperature>=0) || (lastTemp>=0 && temperature<0))
            crossedThreshold=true;
    }
  }

      bool rectsDrawn = false;
    int bandW = width / 3;

  if (changed || force){


    if (((!textOnly) && (force || crossedThreshold)) || (textOnly && crossedThreshold)){
      tft.fillRect(x-bufferPix, y-bufferPix, width+(bufferPix*2), height+(bufferPix*2), ST77XX_BLACK);

      // fill three vertical bands
      for (int i = 0; i < 3; i++) {
          int bx = x + i * bandW;
          int bw = bandW; 
          lastSectionFillColors[i] = sectionFillColors[i];
          tft.fillRoundRect(bx, y, bw, height, 8, sectionFillColors[i]);
          //tft.drawRoundRect(bx, y, bw, height, 8, sectionTextColors[i]);
      }       

      // draw outer outline
      //tft.drawRoundRect(x, y, width, height, 8, ST77XX_WHITE);
      rectsDrawn=true;
      drawsSinceForce=0;
    }

     
    if (showSegmentDeltas){
      int outer = 2; 
      int inner = 0;
      if (tireIndex == 0 || tireIndex == 2){
        outer = 0;
        inner = 2;
      }      
      int center = 1;
      float avgEdge = (float)(sectionTemps[outer]+sectionTemps[inner]) / 2.0f;
      float delta = avgEdge-sectionTemps[center]; // First measure for inflation delta
      float minInflationDelta = avgEdge * (minInflationDeltaPct/100.0f);
      if (delta >= minInflationDelta){
        currentDeltaColors[outer] = highDeltaColor;
        currentDeltaColors[center] = lowDeltaColor;
        currentDeltaColors[inner] = highDeltaColor;
      }else if (delta <= minInflationDelta*-1)
      {
        currentDeltaColors[outer] = lowDeltaColor;
        currentDeltaColors[center] = highDeltaColor;
        currentDeltaColors[inner] = lowDeltaColor;
      }else{
        // If no inflation delta tripped, measure for alignment delta
        delta = sectionTemps[outer] - sectionTemps[inner];
        float minAlignmentDelta = avgEdge * (minAlignmentDeltaPct/100.0f);
        if (delta >= minAlignmentDelta){
          currentDeltaColors[outer] = highDeltaColor;
          currentDeltaColors[center] = normalDeltaColor;
          currentDeltaColors[inner] = normalDeltaColor;
        }else if (delta <= minAlignmentDelta*-1){
          currentDeltaColors[outer] = normalDeltaColor;
          currentDeltaColors[center] = normalDeltaColor;
          currentDeltaColors[inner] = highDeltaColor;
        }else{
          currentDeltaColors[outer] = normalDeltaColor;
          currentDeltaColors[center] = normalDeltaColor;
          currentDeltaColors[inner] = normalDeltaColor;
        }
      }
      for (int i = 0; i < 3; i++) {
        if (rectsDrawn || currentDeltaColors[i] != lastDeltaColors[i]){
          lastDeltaColors[i] = currentDeltaColors[i];
          int bx = x + i * bandW;
          int bw = bandW; 
          int bandH = height/8;
          int startY = (y+height) - (bandH *2);
          tft.fillRect(bx, startY, bw, bandH,  currentDeltaColors[i]);
          //tft.fillRoundRect(bx, y, bw, height, 8, sectionFillColors[i]);
          //tft.drawRoundRect(bx, y, bw, height, 8, sectionTextColors[i]);
        }
      }       
    }
  

    // draw each temperature string center-aligned in its band
    
    tft.setFont(&FreeMonoBold18pt7b);
    tft.setTextSize(1);

    for (int i = 0; i < 3; i++) {
      if (sectionChanged[i] || true){
        char buf[8];

        if (!rectsDrawn && !textOnly){
          // Redraw the last temp with background color
          tft.setTextColor(sectionFillColors[i], sectionFillColors[i]);    
          printTemp(lastTemps[i], i, bandW, textOnly);
        }

        uint16_t textColor = (textOnly) ? ST77XX_BLACK : sectionTextColors[i];
        tft.setTextColor(textColor, sectionFillColors[i]);    
        String tempString = printTemp(sectionTemps[i], i, bandW, false);

        lastTemps[i] = sectionTemps[i];
      }     
    }
  }
  if (shouldResetThreshold){
    shouldResetThreshold=false;
    if (initialized) crossedThreshold = false;
      else initialized = true;
  }
  //USBSerial.println("");
}

String ThreeSectionTire::printTemp(int temp, int i, int bandW, bool drawOutline){
      
    String tempString = String(temp) ;//+ (char)0xF7 + tempUnit;
    uint16_t textWidth, textHeight;    
    int16_t c_x, c_y;
    int extraYBuffer = 7;
    int extraXBuffer = -3;

    tft.getTextBounds(tempString, 0, 0, &c_x, &c_y, &textWidth, &textHeight);

    int xShift = 0;
    int xShiftDir = 0;
    if (temp>=100){
      xShift = 3;
      if (i==0)
        xShiftDir=1;
      else if(i==2)
        xShiftDir=-1;
    }
    xShift *=xShiftDir;

    int centerX = x + i*bandW + bandW/2;
    int halfTextWidth = textWidth/2;
    int startX = (centerX-halfTextWidth) + extraXBuffer + xShift;

    int yDir = -1;
    int yMod = (i==0 || i==2)? ((textHeight+extraYBuffer)*yDir):0;
    int startY = (y + ((height+textHeight) / 2))+yMod;// - (textHeight / 2);

    if (drawOutline){
      tft.setTextColor(sectionFillColors[i], sectionFillColors[i]);   
      tft.setCursor(startX+2, startY+2);
      tft.println(tempString);
      tft.setTextColor(sectionTextColors[i], sectionFillColors[i]);   
    }

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
