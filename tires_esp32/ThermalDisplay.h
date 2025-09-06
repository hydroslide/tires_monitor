#ifndef THERMALDISPLAY_H
#define THERMALDISPLAY_H

#include <Arduino.h>
#include <Adafruit_ST7789.h>
#include <Arduino_GFX_Library.h>
#include "TempReader.h"
#include "Wheels.h"
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
    // deep‐violet, from your original camColors[0]
    static constexpr uint16_t COLOR_COLD_START    = 0x480F;
    // pure cyan = (R=0, G=63, B=31)
    static constexpr uint16_t COLOR_CYAN          = 0x07FF;
    // approx “yellowish‐orange” halfway between yellow and red
    //  (R=31, G≈49,B=0)
    static constexpr uint16_t COLOR_YELLOW_ORANGE = 0xFD20;
    static constexpr uint16_t COLOR_ORANGE = 0xFAE0;
    
    // pure red = (31,0,0)
    static constexpr uint16_t COLOR_RED           = 0xF800;
    // pure white = (31,63,31)
    static constexpr uint16_t COLOR_WHITE         = 0xFFFF;

    static constexpr uint16_t COLOR_PURPLE = 0xE01F;
     static constexpr uint16_t COLOR_PURPLE_HOT = 0xFB7C;

    static constexpr uint16_t COLOR_COLD         = ST77XX_BLUE;
    static constexpr uint16_t COLOR_WARM         = DARK_GREEN;
    static constexpr uint16_t COLOR_IDEAL         = 0xE01F;//ST77XX_ORANGE;
    static constexpr uint16_t COLOR_HOT         =  0xFF9F;//ST77XX_WHITE;
    
    static constexpr uint16_t OFFSET_LINE_COLOR = 0xF81F;

    Adafruit_ST7789 &tft;   // reference to the TFT display object
    static uint16_t *framebuf;     // dynamically allocated areaW×areaH RGB565 buffer
    int areaX, areaY;       // upper-left origin of the update region
    int areaW, areaH;       // width/height of the update region
    
    int tempIndex;

    // Fixed camera dimensions
    static constexpr int CAMERA_WIDTH  = 32;
    static constexpr int CAMERA_HEIGHT = 24;

    // Temperature clipping range (in °C)
    static constexpr int MINTEMP = -30;
    static constexpr int MAXTEMP = 100;

    // Color palette (256-entry RGB565)
    static const uint16_t camColors[256];

 // Shared by all instances:
    static int thresholdMin;     // bottom clamp
    static int thresholdIdeal;   // split point
    static int thresholdMax;     // top clamp

    // ─── STATIC HELPER ───────────────────────────────────
    // Maps a Celsius value → [0…255] over two 128-entry bands
    static uint8_t getColorIndexForTemp(int celsius);
    static float fahrenheitToCelsius(float f);



    // ─── palette storage ────────────────────────────────
    static uint16_t camPalette[256];
    static int      lenCold, lenWarm, lenIdeal, lenHot;  

    // ─── helpers ────────────────────────────────────────
    // rebuilds camColors[] after thresholdMin/Ideal/Max change
    static void    generatePalette();

    // linearly interpolate 565 colors
    static uint16_t interpolate565(uint16_t c1, uint16_t c2, float t);

    void drawPixelOffsets(int _tempIndex);

        // ---- HSV/RGB helpers (keep interpolation saturated) ----
    static inline void rgb565_to_888(uint16_t c, uint8_t& r,uint8_t& g,uint8_t& b) {
    r = ((c>>11)&0x1F); g=((c>>5)&0x3F); b=(c&0x1F);
    r = (r<<3)|(r>>2); g=(g<<2)|(g>>4); b=(b<<3)|(b>>2);
    }
    static inline uint16_t rgb888_to_565(uint8_t r,uint8_t g,uint8_t b) {
    return (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
    }

    // RGB<->HSV (h in [0,360), s,v in [0,1])
    static void rgb2hsv(uint8_t R,uint8_t G,uint8_t B, float& h,float& s,float& v);
    static void hsv2rgb888(float h,float s,float v, uint8_t& R,uint8_t& G,uint8_t& B);

    // Saturated hue-walk between two RGB565 colors (S=1, V=1 inside the band)
    // Endpoints are kept EXACTLY as your anchors.
    static uint16_t interpolateHSV_saturated(uint16_t c1, uint16_t c2, float t);

    // Desaturating tail: keep hue from the start color, ramp S↓ toward target,
    // and V toward target (for your purple_hot → hot end segment).
    static uint16_t interpolateDesaturateToTarget(uint16_t cStart, uint16_t cEnd, float t);

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

    void setTempIndex(int _tempIndex);

    static TempReader* tempReader;
    bool isActive=true;

    /**
     * updateDisplay
     *
     * @param temps[CAMERA_WIDTH * CAMERA_HEIGHT]  A 32×24 array of integer temperatures
     * @param isFahrenheit                         If true, temps[] is in °F; convert to °C
     *
     * Scales and color-maps the 32×24 data into an areaW×areaH RGB565 buffer, then writes it
     * to the region at (areaX, areaY) on the ST7789 display.
     */
    void updateDisplay(const int temps[CAMERA_WIDTH * CAMERA_HEIGHT], int _tempIndex);

    void updateDisplay();

    void updateDisplay(int _tempIndex);

    // Call once to set the min/ideal/max for *every* ThermalDisplay
    static void setTemperatureRangeC(int minTemp, int idealTemp, int maxTemp);
    static void setTemperatureRangeF(int minTemp, int idealTemp, int maxTemp);
    static bool useGradient;
    static bool showPixelOffsets;

};

#endif // THERMALDISPLAY_H
