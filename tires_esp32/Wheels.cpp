#include "Wheels.h"
extern Adafruit_ST7789 tft;  // your global display

Wheels::Wheels(int _bufferPix,
               uint16_t _outlineColor,
               uint16_t _textColor,
               float _minTemp, float _idealTemp, float _maxTemp,
               char _tempUnit,
               bool _fl3, bool _fr3, bool _rl3, bool _rr3,
               uint16_t _lowTempColor,    uint16_t _normalTempColor,
               uint16_t _idealTempColor,  uint16_t _highTempColor,
               uint16_t _lowTempTextColor,    uint16_t _normalTempTextColor,
               uint16_t _idealTempTextColor,  uint16_t _highTempTextColor)
  : bufferPix(_bufferPix), outlineColor(_outlineColor), textColor(_textColor),
    minTemp(_minTemp), idealTemp(_idealTemp), maxTemp(_maxTemp),
    tempUnit(_tempUnit),
    fl3(_fl3), fr3(_fr3), rl3(_rl3), rr3(_rr3),
    lowTempColor(_lowTempColor), normalTempColor(_normalTempColor),
    idealTempColor(_idealTempColor), highTempColor(_highTempColor),
    lowTempTextColor(_lowTempTextColor), normalTempTextColor(_normalTempTextColor),
    idealTempTextColor(_idealTempTextColor), highTempTextColor(_highTempTextColor)
{
    int tireW  = (tft.width()  - bufferPix*3) / 2;
    int tireH  = (tft.height() - bufferPix*3) / 2;
    int x0 = bufferPix, x1 = x0 + tireW + bufferPix;
    int y0 = bufferPix, y1 = y0 + tireH + bufferPix;

    // lambda to pick class based on the bool
    auto mk = [&](bool three, int x, int y){
      if (three)
        return (Tire*)new ThreeSectionTire(x,y,tireW,tireH, bufferPix,
                                          outlineColor,textColor,tempUnit);
      else
        return (Tire*)new Tire(x,y,tireW,tireH, bufferPix,
                               outlineColor,textColor,tempUnit);
    };

    frontLeft  = mk(fl3, x0,y0);
    frontRight = mk(fr3, x1,y0);
    rearLeft   = mk(rl3, x0,y1);
    rearRight  = mk(rr3, x1,y1);

    //   // Attempt to allocate frame buffer in PSRAM
    // framebuf = (uint16_t *)heap_caps_malloc(
    //     tft.width()* tft.height() * sizeof(uint16_t),
    //     MALLOC_CAP_SPIRAM
    // );
    // if (!framebuf) {
    //     // Fall back to normal heap
    //     framebuf = (uint16_t *)malloc(tft.width()* tft.height() * sizeof(uint16_t));
    // }
    // if (!framebuf) {
    //     // If allocation fails, halt
    //     while (true) { delay(1000); }
    // }


    // for (int yy = yStart; yy < yEnd; yy++) {
    //     for (int xx = xStart; xx < xEnd; xx++) {
    //         framebuf[yy * areaW + xx] = color;
    //     }
    //     // Yield to avoid watchdog reset
    //     //yield();
    // }
}

Wheels::Wheels(Wheels* src, bool _fl3, bool _fr3, bool _rl3, bool _rr3)
  : Wheels(
      src->bufferPix,
      src->outlineColor,
      src->textColor,
      src->minTemp, src->idealTemp, src->maxTemp,
      src->tempUnit,
      _fl3, _fr3, _rl3, _rr3,
      src->lowTempColor,
      src->normalTempColor,
      src->idealTempColor,
      src->highTempColor,
      src->lowTempTextColor,
      src->normalTempTextColor,
      src->idealTempTextColor,
      src->highTempTextColor
    )
{}

Wheels::~Wheels() {
    delete frontLeft;
    delete frontRight;
    delete rearLeft;
    delete rearRight;

    // if (framebuf) {
    //     free(framebuf);
    //     framebuf = nullptr;
    // }
}

void Wheels::draw(bool force) {
    //RefreshScreen();
    if (flIsActive)
        frontLeft->draw(force);
    if (frIsActive)
        frontRight->draw(force);
    if (rlIsActive)        
        rearLeft->draw(force);
    if (rrIsActive)
        rearRight->draw(force);
}

void Wheels::RefreshScreen(){
        // Push the entire buffer to ST7789 at (areaX, areaY)
    // tft.startWrite();
    // tft.setAddrWindow(0, 0, tft.width(), tft.height());
    // tft.writePixels(framebuf, tft.width() * tft.height());
    // tft.endWrite();
}

void Wheels::setTireTemps(const TireTemps &fl,
                         const TireTemps &fr,
                         const TireTemps &rl,
                         const TireTemps &rr)
{
    struct P { Tire *t; const TireTemps &v; };
    P list[4] = {
      {frontLeft,  fl},
      {frontRight, fr},
      {rearLeft,   rl},
      {rearRight,  rr}
    };
    for (auto &p : list) {
      p.t->setTemps(p.v.values, p.v.count, (tempUnit=='F'),
                    minTemp, idealTemp, maxTemp,
                    lowTempColor, normalTempColor,
                    idealTempColor, highTempColor,
                    lowTempTextColor, normalTempTextColor,
                    idealTempTextColor, highTempTextColor);
    }
}

    char Wheels::getTempUnit(){ return
     tempUnit;}
    void Wheels::setTempUnit(char _tempUnit){
        tempUnit=_tempUnit;
        frontLeft->tempUnit=tempUnit;
        frontRight->tempUnit=tempUnit;
        rearLeft->tempUnit=tempUnit;
        rearRight->tempUnit=tempUnit;
    }
