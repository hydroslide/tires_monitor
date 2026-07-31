#ifndef DISPLAY_BASE_H
#define DISPLAY_BASE_H

#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>

// The drawing surface every rendering component talks to. Two implementations:
// BufferedDisplay (composes into an off-screen canvas, flushed by drawScreen())
// and StandardDisplay (straight to the glass -- the escape hatch). DisplayProxy
// picks between them at runtime.
//
// EVERY METHOD HERE IS PURE VIRTUAL, DELIBERATELY.
//
// These were no-op default bodies (`{}`) until #28. That meant a method dev
// called but DisplayBase did not declare would bind to Adafruit_ST7789's -- or,
// once the type changed, fail to compile -- while a method DisplayBase declared
// but an implementation forgot to override would compile clean and SILENTLY DRAW
// NOTHING. That is the worst possible failure mode on a device you can only test
// by flashing it: a blank region with no diagnostic.
//
// Five methods were already missing when this was written (drawRect,
// drawFastHLine, fillCircle, setTextWrap, cp437). One of them, drawRect, is the
// only draw in the entire offset-setup border.
//
// With `= 0` the compiler enforces completeness. If you add a method here, all
// three implementations must implement it or the build fails. Keep it that way.
class DisplayBase : public Print {
public:
    virtual ~DisplayBase() {}

    // --- Geometry ---
    virtual int16_t width() = 0;
    virtual int16_t height() = 0;

    // --- Display init/control ---
    virtual void init(uint16_t w, uint16_t h, uint8_t spiMode) = 0;
    virtual void setRotation(uint8_t r) = 0;
    virtual void setSPISpeed(uint32_t freq) = 0;

    // --- Screen/rect drawing ---
    virtual void fillScreen(uint16_t color) = 0;
    virtual void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) = 0;
    virtual void drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) = 0;
    virtual void fillRoundRect(int16_t x, int16_t y, int16_t w, int16_t h,
                               int16_t radius, uint16_t color) = 0;
    virtual void drawRoundRect(int16_t x, int16_t y, int16_t w, int16_t h,
                               int16_t radius, uint16_t color) = 0;
    virtual void fillCircle(int16_t x, int16_t y, int16_t r, uint16_t color) = 0;
    virtual void drawFastVLine(int16_t x, int16_t y, int16_t h, uint16_t color) = 0;
    virtual void drawFastHLine(int16_t x, int16_t y, int16_t w, uint16_t color) = 0;

    // --- Text ---
    virtual void setCursor(int16_t x, int16_t y) = 0;
    virtual void setTextColor(uint16_t color) = 0;
    virtual void setTextColor(uint16_t color, uint16_t bg) = 0;
    virtual void setTextSize(uint8_t size) = 0;
    virtual void setFont(const GFXfont *f) = 0;
    virtual void setTextWrap(bool w) = 0;
    virtual void cp437(bool x) = 0;
    virtual void getTextBounds(const char *str, int16_t x, int16_t y,
                               int16_t *x1, int16_t *y1, uint16_t *w, uint16_t *h) = 0;
    virtual void getTextBounds(const String &str, int16_t x, int16_t y,
                               int16_t *x1, int16_t *y1, uint16_t *w, uint16_t *h) = 0;

    // --- Low-level SPI ---
    //
    // Escape hatch, kept because the interface would be lossy without it. Nothing
    // in the tree calls these any more: ThermalDisplay's quadrant blits went
    // through pushPixels() in #28. Prefer pushPixels() -- it is the one that works
    // on both implementations. These bypass the canvas entirely on the buffered
    // path (they forward to the hardware), so anything drawn with them is wiped by
    // the next drawScreen().
    virtual void startWrite() = 0;
    virtual void setAddrWindow(uint16_t x, uint16_t y, uint16_t w, uint16_t h) = 0;
    virtual void writePixels(uint16_t *colors, uint32_t len) = 0;
    virtual void endWrite() = 0;

    // --- Buffer/screen management ---
    virtual void clearScreen() = 0;
    virtual void drawScreen() = 0;
    virtual void pushPixels(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t *colors, uint32_t len) = 0;

    // --- Print interface (drives print/println for all text) ---
    size_t write(uint8_t c) override = 0;
};

#endif // DISPLAY_BASE_H
