#ifndef BUFFERED_DISPLAY_H
#define BUFFERED_DISPLAY_H

#include "DisplayBase.h"
#include <Adafruit_ST7789.h>

// Composes a full frame into an off-screen canvas, then pushes the whole thing to
// the panel in one SPI burst (drawScreen()). Removes the flicker you get drawing
// straight to the glass, where a half-finished frame is visible.
//
// THE CANVAS IS RETAINED -- never cleared between frames. That is deliberate and
// load-bearing. The tire map is composed on the 1 Hz update cadence while the
// flush runs at 10 Hz, so clearing each frame would blank the map on 9 of every 10
// flushes. It also means every existing skip-if-unchanged gate and
// erase-by-repainting-the-background trick keeps working exactly as it did when
// drawing direct: the canvas is a faithful shadow of the glass. clearScreen() is
// provided but nothing calls it.
class BufferedDisplay : public DisplayBase {
public:
    BufferedDisplay(Adafruit_ST7789 &displayTFT);
    ~BufferedDisplay() override;

    // Allocate the canvas. Idempotent; returns false if allocation failed.
    //
    // MUST be called from setup()/initializeSystem(), never at static init. The
    // canvas is 280*240*2 = 134,400 bytes, and PSRAM does not join the heap until
    // psramAddToHeap() runs inside initArduino() -- which app_main() calls AFTER
    // C++ global constructors (CONFIG_SPIRAM_BOOT_INIT is not set on this core).
    // A by-value canvas in this class would therefore land 131 KB in internal DRAM
    // before the SoftAP starts, and would do it even when the buffered path is
    // switched off. Deferring to here puts it in PSRAM and makes a failure
    // reportable instead of a silent boot hang.
    bool begin();

    bool isReady() const { return canvas != nullptr; }

    // --- Display init/control ---
    void init(uint16_t w, uint16_t h, uint8_t spiMode) override;
    void setRotation(uint8_t r) override;
    void setSPISpeed(uint32_t freq) override;
    int16_t width() override;
    int16_t height() override;

    // --- Screen/rect drawing ---
    void fillScreen(uint16_t color) override;
    void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) override;
    void drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) override;
    void fillRoundRect(int16_t x, int16_t y, int16_t w, int16_t h,
                       int16_t radius, uint16_t color) override;
    void drawRoundRect(int16_t x, int16_t y, int16_t w, int16_t h,
                       int16_t radius, uint16_t color) override;
    void fillCircle(int16_t x, int16_t y, int16_t r, uint16_t color) override;
    void drawFastVLine(int16_t x, int16_t y, int16_t h, uint16_t color) override;
    void drawFastHLine(int16_t x, int16_t y, int16_t w, uint16_t color) override;

    // --- Text ---
    void setCursor(int16_t x, int16_t y) override;
    void setTextColor(uint16_t color) override;
    void setTextColor(uint16_t color, uint16_t bg) override;
    void setTextSize(uint8_t size) override;
    void setFont(const GFXfont *f) override;
    void setTextWrap(bool w) override;
    void cp437(bool x) override;
    void getTextBounds(const char *str, int16_t x, int16_t y,
                       int16_t *x1, int16_t *y1, uint16_t *w, uint16_t *h) override;
    void getTextBounds(const String &str, int16_t x, int16_t y,
                       int16_t *x1, int16_t *y1, uint16_t *w, uint16_t *h) override;

    // --- Low-level SPI (forwarded to hardware; bypasses the canvas -- see DisplayBase) ---
    void startWrite() override;
    void setAddrWindow(uint16_t x, uint16_t y, uint16_t w, uint16_t h) override;
    void writePixels(uint16_t *colors, uint32_t len) override;
    void endWrite() override;

    // --- Buffer/screen management ---
    void clearScreen() override;
    void drawScreen() override;
    void pushPixels(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t *colors, uint32_t len) override;

    // --- Print interface ---
    size_t write(uint8_t c) override;

private:
    Adafruit_ST7789 &tft;
    GFXcanvas16 *canvas = nullptr;

    static constexpr int16_t SCREEN_WIDTH = 280;
    static constexpr int16_t SCREEN_HEIGHT = 240;
};

#endif // BUFFERED_DISPLAY_H
