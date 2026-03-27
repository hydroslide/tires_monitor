#ifndef DISPLAY_BASE_H
#define DISPLAY_BASE_H

#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>

class DisplayBase : public Print {
public:
    virtual ~DisplayBase() {}

    // --- Pure virtual (every display MUST implement) ---
    virtual int16_t width() = 0;
    virtual int16_t height() = 0;

    // --- Display init/control (no-op defaults) ---
    virtual void init(uint16_t w, uint16_t h, uint8_t spiMode) {}
    virtual void setRotation(uint8_t r) {}
    virtual void setSPISpeed(uint32_t freq) {}

    // --- Screen/rect drawing (no-op defaults) ---
    virtual void fillScreen(uint16_t color) {}
    virtual void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {}
    virtual void fillRoundRect(int16_t x, int16_t y, int16_t w, int16_t h,
                               int16_t radius, uint16_t color) {}
    virtual void drawRoundRect(int16_t x, int16_t y, int16_t w, int16_t h,
                               int16_t radius, uint16_t color) {}
    virtual void drawFastVLine(int16_t x, int16_t y, int16_t h, uint16_t color) {}

    // --- Text (no-op defaults) ---
    virtual void setCursor(int16_t x, int16_t y) {}
    virtual void setTextColor(uint16_t color) {}
    virtual void setTextColor(uint16_t color, uint16_t bg) {}
    virtual void setTextSize(uint8_t size) {}
    virtual void setFont(const GFXfont *f) {}
    virtual void getTextBounds(const char *str, int16_t x, int16_t y,
                               int16_t *x1, int16_t *y1, uint16_t *w, uint16_t *h) {}
    virtual void getTextBounds(const String &str, int16_t x, int16_t y,
                               int16_t *x1, int16_t *y1, uint16_t *w, uint16_t *h) {}

    // --- Low-level SPI (no-op defaults) ---
    virtual void startWrite() {}
    virtual void setAddrWindow(uint16_t x, uint16_t y, uint16_t w, uint16_t h) {}
    virtual void writePixels(uint16_t *colors, uint32_t len) {}
    virtual void endWrite() {}

    // --- Buffer/screen management (no-op defaults) ---
    virtual void clearScreen() {}
    virtual void drawScreen() {}
    virtual void pushPixels(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t *colors, uint32_t len) {}

    // --- Print interface (no-op default) ---
    size_t write(uint8_t c) override { return 1; }
};

#endif // DISPLAY_BASE_H
