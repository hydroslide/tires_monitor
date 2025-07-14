#ifndef THREESECTIONTIRE_H
#define THREESECTIONTIRE_H

#include "Tire.h"
#include <Adafruit_GFX.h>
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


private:
    int    sectionTemps[3];
    int    lastTemps[3];
    uint16_t sectionFillColors[3];
    uint16_t sectionTextColors[3];
    bool initialized = false;
    bool crossedThreshold = true;
    bool shouldResetThreshold = false;
    int forceInterval = 5;
    int drawsSinceForce = 0;

    String printTemp(int temp, int i, int bandW);

    // Helper: classify one section’s color/text from temp + thresholds
    void classifyOne(int idx, float tempC,
                     float minTemp, float idealTemp, float maxTemp,
                     uint16_t lowColor,    uint16_t normalColor,
                     uint16_t idealColor,  uint16_t highColor,
                     uint16_t lowTextColor,    uint16_t normalTextColor,
                     uint16_t idealTextColor,  uint16_t highTextColor);
};

#endif // THREESECTIONTIRE_H
