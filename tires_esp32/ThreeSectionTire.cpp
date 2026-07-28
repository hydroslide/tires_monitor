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
extern bool getShowGateBar();   // TireMenu -- shared toggle for the IMU gate tuning aids

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
    return false;
}

void ThreeSectionTire::draw(bool force, bool textOnly) {

  // initialize() is a virtual override that the base Tire constructor cannot reach
  // (the vtable is still Tire's during base construction), so run it once here on the
  // first draw to seed lastDeltaColors[]/currentDeltaColors[]/lastSectionFillColors[].
  if (!deltaColorsInitialized) {
    initialize();
    deltaColorsInitialized = true;
  }

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
      continue; // out-of-range band: skip just this section, keep drawing the rest
    if (lastTemp != temperature) {
      changed = true;
      sectionChanged[i]=true;

      if ((lastTemp>=100 && temperature<100) || (lastTemp<100 && temperature>=100) || (lastTemp<0 && temperature>=0) || (lastTemp>=0 && temperature<0))
            crossedThreshold=true;
    }
  }

  // The inflation verdict is no longer derived from these temperatures (#21) -- it is
  // pushed in from the IMU gate's latch, which can flip while the rounded integer temps
  // sit still (evidence decaying on neutral frames, or a corner latching just as its
  // reading settles). Without this the delta bars would keep painting a stale verdict.
  if (latchedInflation != lastLatchedInflation) {
    changed = true;
    lastLatchedInflation = latchedInflation;
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

     
    // Segment-delta classification (story 08 / issue #9): compute the per-band delta
    // color unconditionally so the NBP instrumentation stream can ship the real,
    // displayed over/under/alignment color for the renderer even when the on-screen
    // overlay toggle is off. Only the painting of the delta rects stays gated by
    // showSegmentDeltas below.
    {
      int outer = 2;
      int inner = 0;
      if (tireIndex == 0 || tireIndex == 2){
        outer = 0;
        inner = 2;
      }
      int center = 1;
      float avgEdge = (float)(sectionTemps[outer]+sectionTemps[inner]) / 2.0f;
      float delta;
      // Inflation verdict comes from the IMU gate's per-tire latch now (#21), not from a
      // fresh comparison here. Same underlying test (edge vs centre against Inflation
      // Delta %), but evaluated only on captured straight-line frames and held until the
      // evidence decays -- so these bars stop flipping every time the car loads up.
      // -1 = edges hot (under-inflation), +1 = centre hot (over-inflation).
      if (latchedInflation < 0){
        currentDeltaColors[outer] = highDeltaColor;
        currentDeltaColors[center] = lowDeltaColor;
        currentDeltaColors[inner] = highDeltaColor;
      }else if (latchedInflation > 0)
      {
        currentDeltaColors[outer] = lowDeltaColor;
        currentDeltaColors[center] = highDeltaColor;
        currentDeltaColors[inner] = lowDeltaColor;
      }else{
        // No inflation verdict latched: fall back to the alignment check, which stays
        // instantaneous. It has no dwell model of its own, and camber wear is a slow
        // steady signal rather than something a single corner can manufacture.
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
      if (showSegmentDeltas){
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

  // Per-tire dwell bar (#21). Shows how much evidence this corner has accumulated toward
  // an inflation verdict, so the dwell is tunable by eye instead of by guesswork: the fill
  // grows from the centre while the corner reads over/under on captured frames, freezes
  // mid-corner (the gate stops feeding it), and leaks back on neutral ones. Reaching a
  // tick latches the verdict and colours the segment bars above.
  //
  // Drawn OUTSIDE the `changed || force` block on purpose -- the score moves continuously
  // while the temperatures may sit still for many seconds, and a frozen bar would be
  // read as "no evidence" rather than "not repainted".
  {
    const int dbH  = 4;
    const int dbY  = y + height - 7;        // below the delta bars, inside the tire fill
    const int half = width / 2;
    const int dbCx = x + half;

    if (dwellMax > 0 && getShowGateBar()) {
      if (rectsDrawn || !dwellBarDrawn || dwellScore != lastDwellScore) {
        // Black track repainted every time, which also erases the previous fill -- the
        // background here is the band colour, so there is nothing simpler to restore to.
        tft.fillRect(x, dbY, width, dbH, ST77XX_BLACK);

        long mag = (dwellScore < 0) ? -dwellScore : dwellScore;
        int  len = (int)(((long)half * mag) / dwellMax);
        if (len > half) len = half;
        if (len > 0) {
          if (dwellScore > 0) tft.fillRect(dbCx, dbY, len, dbH, highDeltaColor);
          else                tft.fillRect(dbCx - len, dbY, len, dbH, lowDeltaColor);
        }

        // Latch marks last so they stay legible once the fill passes them.
        int tick = (int)(((long)half * dwellLatch) / dwellMax);
        tft.drawFastVLine(dbCx - tick, dbY, dbH, 0x7BEF);
        tft.drawFastVLine(dbCx + tick, dbY, dbH, 0x7BEF);

        lastDwellScore = dwellScore;
        dwellBarDrawn  = true;
      }
    } else if (dwellBarDrawn) {
      // Turned off (or no gate data): give the strip back to the band colours.
      for (int i = 0; i < 3; i++)
        tft.fillRect(x + i * bandW, dbY, bandW, dbH, sectionFillColors[i]);
      dwellBarDrawn = false;
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
