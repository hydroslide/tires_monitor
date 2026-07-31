#ifndef QUADRANTFACTORY_H
#define QUADRANTFACTORY_H

#include <Arduino.h>
#include <Adafruit_ST7789.h>
#include "DisplayBase.h"
#include "ThermalDisplay.h"

/**
 * QuadrantFactory
 *
 * Given a full 280×240 ST7789 display and a margin in pixels, produces
 * ThermalDisplay instances cropped to one of the four quadrants. Each
 * quadrant is 140×120 in size; within that quadrant, the 32×24 image is
 * scaled to fit (preserving the 4:3 aspect ratio), and centered inside
 * the quadrant with the specified margin on all sides.
 *
 * Usage:
 *   QuadrantFactory factory(tft, margin);
 *   ThermalDisplay* upperLeft = factory.createDisplay(/* top=* /true, /* left=* /true);
 *   ThermalDisplay* lowerRight = factory.createDisplay(false, false);
 *
 * Caller is responsible for deleting each returned ThermalDisplay* when done.
 */
class QuadrantFactory {
public:
    /**
     * Constructor
     *
     * @param displayTFT  Reference to the initialized 280×240 Adafruit_ST7789
     * @param margin      Margin (in pixels) to leave on all four sides inside each quadrant
     */
    QuadrantFactory(DisplayBase &displayTFT, int margin);

    /** Destructor (no owned resources besides potential ThermalDisplay instances returned). */
    ~QuadrantFactory();

    /**
     * createDisplay
     *
     * @param top   If true, use the top half (0..119); else bottom half (120..239)
     * @param left  If true, use the left half (0..139); else right half (140..279)
     * @return      A pointer to a newly-allocated ThermalDisplay for that quadrant.
     *              Caller must delete it when no longer needed.
     */
    ThermalDisplay* createDisplay(bool top, bool left);

private:
    DisplayBase &tft;  // reference to the full 280×240 display
    int margin;           // margin inside each quadrant

    // Full-screen constants
    static constexpr int FULL_WIDTH   = 280;
    static constexpr int FULL_HEIGHT  = 240;
    static constexpr int QUAD_WIDTH   = FULL_WIDTH  / 2;  // 140
    static constexpr int QUAD_HEIGHT  = FULL_HEIGHT / 2;  // 120

    // Camera (source) dimensions
    static constexpr int CAMERA_WIDTH  = 32;
    static constexpr int CAMERA_HEIGHT = 24;
};

#endif // QUADRANTFACTORY_H
