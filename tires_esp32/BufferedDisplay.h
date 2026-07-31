#ifndef BUFFERED_DISPLAY_H
#define BUFFERED_DISPLAY_H

#include "DisplayBase.h"
#include <Adafruit_ST7789.h>

class BufferedDisplay : public DisplayBase {
public:
    BufferedDisplay(Adafruit_ST7789 &displayTFT);

    // --- Display init/control ---
    void init(uint16_t w, uint16_t h, uint8_t spiMode) override;
    void setRotation(uint8_t r) override;
    void setSPISpeed(uint32_t freq) override;
    int16_t width() override;
    int16_t height() override;

    // --- Screen/rect drawing ---
    void fillScreen(uint16_t color) override;
    void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) override;
    void fillRoundRect(int16_t x, int16_t y, int16_t w, int16_t h,
                       int16_t radius, uint16_t color) override;
    void drawRoundRect(int16_t x, int16_t y, int16_t w, int16_t h,
                       int16_t radius, uint16_t color) override;
    void drawFastVLine(int16_t x, int16_t y, int16_t h, uint16_t color) override;

    // --- Text ---
    void setCursor(int16_t x, int16_t y) override;
    void setTextColor(uint16_t color) override;
    void setTextColor(uint16_t color, uint16_t bg) override;
    void setTextSize(uint8_t size) override;
    void setFont(const GFXfont *f) override;
    void getTextBounds(const char *str, int16_t x, int16_t y,
                       int16_t *x1, int16_t *y1, uint16_t *w, uint16_t *h) override;
    void getTextBounds(const String &str, int16_t x, int16_t y,
                       int16_t *x1, int16_t *y1, uint16_t *w, uint16_t *h) override;

    // --- Low-level SPI (forwarded to hardware for direct usage) ---
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
    GFXcanvas16 canvas;

    static constexpr int16_t SCREEN_WIDTH = 280;
    static constexpr int16_t SCREEN_HEIGHT = 240;
};

#endif // BUFFERED_DISPLAY_H
