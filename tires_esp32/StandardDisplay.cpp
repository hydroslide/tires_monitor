#include "StandardDisplay.h"

StandardDisplay::StandardDisplay(Adafruit_ST7789 &displayTFT)
  : tft(displayTFT) {}

// --- Display init/control ---
void StandardDisplay::init(uint16_t w, uint16_t h, uint8_t spiMode) { tft.init(w, h, spiMode); }
void StandardDisplay::setRotation(uint8_t r) { tft.setRotation(r); }
void StandardDisplay::setSPISpeed(uint32_t freq) { tft.setSPISpeed(freq); }
int16_t StandardDisplay::width() { return tft.width(); }
int16_t StandardDisplay::height() { return tft.height(); }

// --- Screen/rect drawing ---
void StandardDisplay::fillScreen(uint16_t color) { tft.fillScreen(color); }
void StandardDisplay::fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) { tft.fillRect(x, y, w, h, color); }
void StandardDisplay::fillRoundRect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t radius, uint16_t color) { tft.fillRoundRect(x, y, w, h, radius, color); }
void StandardDisplay::drawRoundRect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t radius, uint16_t color) { tft.drawRoundRect(x, y, w, h, radius, color); }
void StandardDisplay::drawFastVLine(int16_t x, int16_t y, int16_t h, uint16_t color) { tft.drawFastVLine(x, y, h, color); }

// --- Text ---
void StandardDisplay::setCursor(int16_t x, int16_t y) { tft.setCursor(x, y); }
void StandardDisplay::setTextColor(uint16_t color) { tft.setTextColor(color); }
void StandardDisplay::setTextColor(uint16_t color, uint16_t bg) { tft.setTextColor(color, bg); }
void StandardDisplay::setTextSize(uint8_t size) { tft.setTextSize(size); }
void StandardDisplay::setFont(const GFXfont *f) { tft.setFont(f); }
void StandardDisplay::getTextBounds(const char *str, int16_t x, int16_t y, int16_t *x1, int16_t *y1, uint16_t *w, uint16_t *h) { tft.getTextBounds(str, x, y, x1, y1, w, h); }
void StandardDisplay::getTextBounds(const String &str, int16_t x, int16_t y, int16_t *x1, int16_t *y1, uint16_t *w, uint16_t *h) { tft.getTextBounds(str, x, y, x1, y1, w, h); }

// --- Low-level SPI ---
void StandardDisplay::startWrite() { tft.startWrite(); }
void StandardDisplay::setAddrWindow(uint16_t x, uint16_t y, uint16_t w, uint16_t h) { tft.setAddrWindow(x, y, w, h); }
void StandardDisplay::writePixels(uint16_t *colors, uint32_t len) { tft.writePixels(colors, len); }
void StandardDisplay::endWrite() { tft.endWrite(); }

// --- Buffer/screen management ---
void StandardDisplay::clearScreen() { tft.fillScreen(ST77XX_BLACK); }
void StandardDisplay::drawScreen() { /* no-op: StandardDisplay draws directly to screen */ }
void StandardDisplay::pushPixels(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t *colors, uint32_t len) {
    tft.startWrite();
    tft.setAddrWindow(x, y, w, h);
    tft.writePixels(colors, len);
    tft.endWrite();
}

// --- Print interface ---
size_t StandardDisplay::write(uint8_t c) { return tft.write(c); }
