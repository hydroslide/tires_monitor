#include "BufferedDisplay.h"
#include <string.h>

BufferedDisplay::BufferedDisplay(Adafruit_ST7789 &displayTFT)
  : tft(displayTFT), canvas(SCREEN_WIDTH, SCREEN_HEIGHT) {}

// --- Display init/control (delegate to physical hardware) ---
void BufferedDisplay::init(uint16_t w, uint16_t h, uint8_t spiMode) { tft.init(w, h, spiMode); }
void BufferedDisplay::setRotation(uint8_t r) { tft.setRotation(r); }
void BufferedDisplay::setSPISpeed(uint32_t freq) { tft.setSPISpeed(freq); }
int16_t BufferedDisplay::width() { return canvas.width(); }
int16_t BufferedDisplay::height() { return canvas.height(); }

// --- Screen/rect drawing (draw to canvas) ---
void BufferedDisplay::fillScreen(uint16_t color) { canvas.fillScreen(color); }
void BufferedDisplay::fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) { canvas.fillRect(x, y, w, h, color); }
void BufferedDisplay::fillRoundRect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t radius, uint16_t color) { canvas.fillRoundRect(x, y, w, h, radius, color); }
void BufferedDisplay::drawRoundRect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t radius, uint16_t color) { canvas.drawRoundRect(x, y, w, h, radius, color); }
void BufferedDisplay::drawFastVLine(int16_t x, int16_t y, int16_t h, uint16_t color) { canvas.drawFastVLine(x, y, h, color); }

// --- Text (draw to canvas) ---
void BufferedDisplay::setCursor(int16_t x, int16_t y) { canvas.setCursor(x, y); }
void BufferedDisplay::setTextColor(uint16_t color) { canvas.setTextColor(color); }
void BufferedDisplay::setTextColor(uint16_t color, uint16_t bg) { canvas.setTextColor(color, bg); }
void BufferedDisplay::setTextSize(uint8_t size) { canvas.setTextSize(size); }
void BufferedDisplay::setFont(const GFXfont *f) { canvas.setFont(f); }
void BufferedDisplay::getTextBounds(const char *str, int16_t x, int16_t y, int16_t *x1, int16_t *y1, uint16_t *w, uint16_t *h) { canvas.getTextBounds(str, x, y, x1, y1, w, h); }
void BufferedDisplay::getTextBounds(const String &str, int16_t x, int16_t y, int16_t *x1, int16_t *y1, uint16_t *w, uint16_t *h) { canvas.getTextBounds(str, x, y, x1, y1, w, h); }

// --- Low-level SPI (forward to hardware for direct usage) ---
void BufferedDisplay::startWrite() { tft.startWrite(); }
void BufferedDisplay::setAddrWindow(uint16_t x, uint16_t y, uint16_t w, uint16_t h) { tft.setAddrWindow(x, y, w, h); }
void BufferedDisplay::writePixels(uint16_t *colors, uint32_t len) { tft.writePixels(colors, len); }
void BufferedDisplay::endWrite() { tft.endWrite(); }

// --- Buffer/screen management ---
void BufferedDisplay::clearScreen() {
    canvas.fillScreen(0);
}

void BufferedDisplay::drawScreen() {
    tft.startWrite();
    tft.setAddrWindow(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
    tft.writePixels(canvas.getBuffer(), SCREEN_WIDTH * SCREEN_HEIGHT);
    tft.endWrite();
}

void BufferedDisplay::pushPixels(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t *colors, uint32_t len) {
    uint16_t *buf = canvas.getBuffer();
    if (!buf) return;
    for (uint16_t row = 0; row < h; row++) {
        memcpy(&buf[(y + row) * SCREEN_WIDTH + x], &colors[row * w], w * sizeof(uint16_t));
    }
}

// --- Print interface (draw to canvas) ---
size_t BufferedDisplay::write(uint8_t c) { return canvas.write(c); }
