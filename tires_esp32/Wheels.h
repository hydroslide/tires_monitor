#ifndef WHEELS_H
#define WHEELS_H

#include <Adafruit_ST7789.h>
#include <Adafruit_GFX.h>
#include "DisplayBase.h"
#include "Tire.h"
#define PURPLE 0xE01F
#define DARK_GREEN 	0x06A0//0x06E0//0x0680 //0x05E0
#include "ThreeSectionTire.h"

class Wheels {
public:
    char getTempUnit();
    void setTempUnit(char tempUnit);

    // your color/threshold fields…
    int bufferPix;
    uint16_t outlineColor, textColor;
    float minTemp, idealTemp, maxTemp;
    char tempUnit;
    bool fl3=false;
    bool fr3=false;
    bool rl3=false;
    bool rr3=false;

    // helper for setTireTemps
struct TireTemps {
  float values[3];
  size_t count;
  
  TireTemps() = default;             // 1) default‐ctor

  // single‐value constructor
  explicit TireTemps(float v)
    : values{v, 0, 0}, count(1)
  {}

  // three‐value constructor
  explicit TireTemps(const float (&arr)[3])
    : count(3)
  {
    memcpy(values, arr, 3*sizeof(float));
  }
};


    /** 
     * @param bufferPix      spacing between tires 
     * @param outlineColor   rounded‐rect outline color 
     * @param textColor      default text color (unused for three‐section) 
     * @param min/ideal/max  your temperature thresholds in °C 
     * @param tempUnit       'C' or 'F' 
     * @param fl3/fr3/rl3/rr3  true ⇒ use ThreeSectionTire, false ⇒ plain Tire 
     * @param lowTempColor…   optional palette arguments as before 
     */
    Wheels(int bufferPix,
           uint16_t outlineColor,
           uint16_t textColor,
           float minTemp, float idealTemp, float maxTemp,
           char tempUnit,
           bool showSegmentDeltas, byte minInflationDeltaPct, byte minAlignmentDeltaPct,
           bool fl3=false, bool fr3=false, bool rl3=false, bool rr3=false,
           uint16_t lowTempColor    = ST77XX_BLUE,
           uint16_t normalTempColor = DARK_GREEN,
           uint16_t idealTempColor  = PURPLE, //ST77XX_ORANGE,
           uint16_t highTempColor   = ST77XX_WHITE,
           uint16_t lowTempTextColor    = ST77XX_WHITE,
           uint16_t normalTempTextColor = ST77XX_WHITE,
           uint16_t idealTempTextColor  = ST77XX_WHITE,
           uint16_t highTempTextColor   = ST77XX_RED);

    ~Wheels();

    Wheels(Wheels* src, bool fl3, bool fr3, bool rl3, bool rr3);

    // draw all four
    void draw(bool force=false, bool textOnly=false);

    /** 
     * Dispatch 1-or-3 values per tire.  
     * fl, fr, rl, rr may have count==1 or 3. 
     */
    void setTireTemps(const TireTemps &fl,
                      const TireTemps &fr,
                      const TireTemps &rl,
                      const TireTemps &rr);

    bool flIsActive = true;
    bool frIsActive = true;
    bool rlIsActive = true;
    bool rrIsActive = true;

private:
    Tire *frontLeft, *frontRight, *rearLeft, *rearRight;


    bool showSegmentDeltas;
    byte minInflationDeltaPct;
    byte minAlignmentDeltaPct;
    uint16_t lowTempColor, normalTempColor, idealTempColor, highTempColor;
    uint16_t lowTempTextColor, normalTempTextColor, idealTempTextColor, highTempTextColor;
    //uint16_t *framebuf;     // dynamically allocated areaW×areaH RGB565 buffer
    void RefreshScreen();
};

#endif
