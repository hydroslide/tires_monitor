#ifndef THERMALDISPLAY_H
#define THERMALDISPLAY_H

#include <Arduino.h>
#include <Adafruit_ST7789.h>
#include <stdint.h>

/**
 * ThermalDisplay
 *
 * Encapsulates mapping a 32×24 array of integer temperatures (either Celsius or Fahrenheit)
 * into a dynamically‐sized RGB565 buffer, then blitting that buffer to a specified region
 * of a ST7789 display.
 *
 * Constructor parameters:
 *  - displayTFT: reference to an initialized Adafruit_ST7789 object
 *  - areaX, areaY: upper‐left corner (in pixels) of the region on the 280×240 screen
 *  - areaW, areaH: width and height (in pixels) of the region to update (e.g. 240×180)
 *
 * Example: to center a 240×180 block in a 280×240 display, call
 *   ThermalDisplay(tft, 20, 30, 240, 180);
 */
class ThermalDisplay {

private:
    Adafruit_ST7789 &tft;   // reference to the TFT display object
    uint16_t *framebuf;     // dynamically allocated areaW×areaH RGB565 buffer
    int areaX, areaY;       // upper-left origin of the update region
    int areaW, areaH;       // width/height of the update region

    // Fixed camera dimensions
    static constexpr int CAMERA_WIDTH  = 32;
    static constexpr int CAMERA_HEIGHT = 24;

    // Temperature clipping range (in °C)
    static constexpr int MINTEMP = 20;
    static constexpr int MAXTEMP = 35;

    // Color palette (256-entry RGB565)
    static const uint16_t camColors[256];

public:
    /**
     * Constructor
     *
     * @param displayTFT   Reference to an already-initialized Adafruit_ST7789 instance
     * @param areaX        X coordinate of upper-left corner of update region
     * @param areaY        Y coordinate of upper-left corner of update region
     * @param areaW        Width (in pixels) of update region (e.g. 240)
     * @param areaH        Height (in pixels) of update region (e.g. 180)
     */
    ThermalDisplay(Adafruit_ST7789 &displayTFT, int areaX, int areaY, int areaW, int areaH);

    /** Destructor frees the allocated frame buffer. */
    ~ThermalDisplay();

    /**
     * updateDisplay
     *
     * @param temps[CAMERA_WIDTH * CAMERA_HEIGHT]  A 32×24 array of integer temperatures
     * @param isFahrenheit                         If true, temps[] is in °F; convert to °C
     *
     * Scales and color-maps the 32×24 data into an areaW×areaH RGB565 buffer, then writes it
     * to the region at (areaX, areaY) on the ST7789 display.
     */
    void updateDisplay(const int temps[CAMERA_WIDTH * CAMERA_HEIGHT]);

};

#endif // THERMALDISPLAY_H
