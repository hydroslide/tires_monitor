#ifndef THREESECTIONTIRE_H
#define THREESECTIONTIRE_H

#include "Tire.h"
#include <Adafruit_GFX.h>
#include "DisplayBase.h"
#include <Fonts/FreeSans9pt7b.h>  // scalable 11-pixel font

/**
 * ThreeSectionTire
 *
 * Inherits Tire but instead of one temperature, takes three. Splits
 * the rounded rectangle into three equal vertical bands, colors each
 * band according to its temperature, and prints each temp centered
 * in that band using FreeSans9pt7b.
 */
class ThreeSectionTire : public Tire {
public:
    // Inherit Tire’s constructors:
    using Tire::Tire;

    /** 
     * Set three temperatures at once (C or °F depending on flag).
     * Uses the same thresholds & palette logic as Tire::setTemp, but
     * independently for each section.
     */
    void setSectionTemps(const float temps[3],
                         bool isFahrenheit,
                         float minTemp, float idealTemp, float maxTemp,
                         uint16_t lowColor,    uint16_t normalColor,
                         uint16_t idealColor,  uint16_t highColor,
                         uint16_t lowTextColor,    uint16_t normalTextColor,
                         uint16_t idealTextColor,  uint16_t highTextColor);

    /** 
     * Override draw to fill three bands, draw outline, then print 3 temps
     */
    void draw(bool force=false, bool textOnly = false) override;



        // Declaration in header, with override:
    void setTemps(const float *temps,
                  size_t count,
                  bool isFahrenheit,
                  float minTemp,
                  float idealTemp,
                  float maxTemp,
                  uint16_t lowColor,
                  uint16_t normalColor,
                  uint16_t idealColor,
                  uint16_t highColor,
                  uint16_t lowTextColor,
                  uint16_t normalTextColor,
                  uint16_t idealTextColor,
                  uint16_t highTextColor) override;

    bool showSegmentDeltas;
    byte minInflationDeltaPct;
    byte minAlignmentDeltaPct;

    // Instrumentation getters (story 08 / issue #9). Expose the per-band colors the
    // display actually computed so the NBP stream can ship them as hex for the renderer
    // without re-deriving the classifier. fill = temp/window band color; delta =
    // over/under/alignment band color. Both are RGB565.
    uint16_t sectionFillColor(int i) const {
        return (i >= 0 && i < 3) ? sectionFillColors[i] : 0;
    }
    uint16_t sectionDeltaColor(int i) const {
        return (i >= 0 && i < 3) ? currentDeltaColors[i] : 0;
    }

    // Latched inflation verdict from the IMU gate (#21): +1 over, -1 under, 0 none.
    // The tire no longer decides this for itself -- it used to recompute the edge-vs-
    // centre comparison on every draw, un-gated and un-dwelled, which made the delta bars
    // flicker through every corner as body roll manufactured a fake centre-hot.
    void setInflationVerdict(int8_t verdict) { latchedInflation = verdict; }

    // Signed evidence score behind that verdict, plus the latch point and saturation
    // bound, for the per-tire dwell bar. Bounds are supplied rather than assumed so the
    // bar rescales when the Dwell setting changes.
    void setDwellProgress(long score, long latch, long max) {
        dwellScore = score; dwellLatch = latch; dwellMax = max;
    }


private:

    int    sectionTemps[3];
    int    lastTemps[3];
    uint16_t sectionFillColors[3];
    uint16_t lastSectionFillColors[3];
    uint16_t sectionTextColors[3];
    
    
    // Latched inflation verdict + the evidence behind it, pushed in from the IMU gate.
    int8_t latchedInflation = 0;    // +1 over, -1 under, 0 none
    int8_t lastLatchedInflation = 0;
    long   dwellScore = 0;          // signed evidence, ms
    long   dwellLatch = 0;          // |score| at which the verdict trips
    long   dwellMax   = 0;          // |score| saturation bound (0 => bar hidden)
    long   lastDwellScore = 0;
    bool   dwellBarDrawn = false;

    bool initialized = false;
    bool deltaColorsInitialized = false;
    bool crossedThreshold = true;
    bool shouldResetThreshold = false;
    int forceInterval = 5;
    int drawsSinceForce = 0;


    bool anySectionColorChanged();


    String printTemp(int temp, int i, int bandW, bool drawOutline = false);

    // Helper: classify one section’s color/text from temp + thresholds
    void classifyOne(int idx, float tempC,
                     float minTemp, float idealTemp, float maxTemp,
                     uint16_t lowColor,    uint16_t normalColor,
                     uint16_t idealColor,  uint16_t highColor,
                     uint16_t lowTextColor,    uint16_t normalTextColor,
                     uint16_t idealTextColor,  uint16_t highTextColor);

protected:
    uint16_t normalDeltaColor  = 0x06A0; 
    uint16_t lowDeltaColor  = ST77XX_CYAN;
    uint16_t highDeltaColor  = ST77XX_YELLOW;
    uint16_t lastDeltaColors[3];
    uint16_t currentDeltaColors[3];
    void initialize();
};

#endif // THREESECTIONTIRE_H
