#ifndef DISPLAY_PROXY_H
#define DISPLAY_PROXY_H

#include "DisplayBase.h"

class DisplayProxy : public DisplayBase {
public:
    void setImplementation(DisplayBase *impl) { _impl = impl; }

    // --- Pure virtual ---
    int16_t width() override { return _impl->width(); }
    int16_t height() override { return _impl->height(); }

    // --- Display init/control ---
    void init(uint16_t w, uint16_t h, uint8_t spiMode) override { _impl->init(w, h, spiMode); }
    void setRotation(uint8_t r) override { _impl->setRotation(r); }
    void setSPISpeed(uint32_t freq) override { _impl->setSPISpeed(freq); }

    // --- Screen/rect drawing ---
    void fillScreen(uint16_t color) override { _impl->fillScreen(color); }
    void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) override { _impl->fillRect(x, y, w, h, color); }
    void fillRoundRect(int16_t x, int16_t y, int16_t w, int16_t h,
                       int16_t radius, uint16_t color) override { _impl->fillRoundRect(x, y, w, h, radius, color); }
    void drawRoundRect(int16_t x, int16_t y, int16_t w, int16_t h,
                       int16_t radius, uint16_t color) override { _impl->drawRoundRect(x, y, w, h, radius, color); }
    void drawFastVLine(int16_t x, int16_t y, int16_t h, uint16_t color) override { _impl->drawFastVLine(x, y, h, color); }

    // --- Text ---
    void setCursor(int16_t x, int16_t y) override { _impl->setCursor(x, y); }
    void setTextColor(uint16_t color) override { _impl->setTextColor(color); }
    void setTextColor(uint16_t color, uint16_t bg) override { _impl->setTextColor(color, bg); }
    void setTextSize(uint8_t size) override { _impl->setTextSize(size); }
    void setFont(const GFXfont *f) override { _impl->setFont(f); }
    void getTextBounds(const char *str, int16_t x, int16_t y,
                       int16_t *x1, int16_t *y1, uint16_t *w, uint16_t *h) override { _impl->getTextBounds(str, x, y, x1, y1, w, h); }
    void getTextBounds(const String &str, int16_t x, int16_t y,
                       int16_t *x1, int16_t *y1, uint16_t *w, uint16_t *h) override { _impl->getTextBounds(str, x, y, x1, y1, w, h); }

    // --- Low-level SPI ---
    void startWrite() override { _impl->startWrite(); }
    void setAddrWindow(uint16_t x, uint16_t y, uint16_t w, uint16_t h) override { _impl->setAddrWindow(x, y, w, h); }
    void writePixels(uint16_t *colors, uint32_t len) override { _impl->writePixels(colors, len); }
    void endWrite() override { _impl->endWrite(); }

    // --- Buffer/screen management ---
    void clearScreen() override { _impl->clearScreen(); }
    void drawScreen() override { _impl->drawScreen(); }
    void pushPixels(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t *colors, uint32_t len) override { _impl->pushPixels(x, y, w, h, colors, len); }

    // --- Print interface ---
    size_t write(uint8_t c) override { return _impl->write(c); }

private:
    DisplayBase *_impl = nullptr;
};

#endif // DISPLAY_PROXY_H
