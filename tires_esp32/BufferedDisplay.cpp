#include "BufferedDisplay.h"
#include <string.h>
#include <esp_heap_caps.h>

extern HWCDC USBSerial;

BufferedDisplay::BufferedDisplay(Adafruit_ST7789 &displayTFT)
  : tft(displayTFT) {}

BufferedDisplay::~BufferedDisplay() {
    delete canvas;
    canvas = nullptr;
}

bool BufferedDisplay::begin() {
    if (canvas) return true;   // idempotent

    canvas = new GFXcanvas16(SCREEN_WIDTH, SCREEN_HEIGHT);
    if (!canvas || !canvas->getBuffer()) {
        delete canvas;
        canvas = nullptr;
        USBSerial.println("BufferedDisplay: canvas allocation FAILED");
        return false;
    }

    // Report where it landed. Internal DRAM is scarce and shared with the SoftAP;
    // PSRAM is 8 MB and effectively free. If this ever says INTERNAL, something
    // moved the allocation back before psramAddToHeap() and the SoftAP will be
    // the thing that breaks, not the display.
    const bool external = esp_ptr_external_ram(canvas->getBuffer());
    USBSerial.printf("BufferedDisplay: %d byte canvas in %s (internal free: %u)\n",
                     SCREEN_WIDTH * SCREEN_HEIGHT * 2,
                     external ? "PSRAM" : "INTERNAL DRAM",
                     (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL));

    canvas->fillScreen(0);
    return true;
}

// --- Display init/control (delegate to physical hardware) ---
void BufferedDisplay::init(uint16_t w, uint16_t h, uint8_t spiMode) { tft.init(w, h, spiMode); }
void BufferedDisplay::setRotation(uint8_t r) { tft.setRotation(r); }
void BufferedDisplay::setSPISpeed(uint32_t freq) { tft.setSPISpeed(freq); }
int16_t BufferedDisplay::width() { return SCREEN_WIDTH; }
int16_t BufferedDisplay::height() { return SCREEN_HEIGHT; }

// --- Screen/rect drawing (draw to canvas) ---
//
// Every one of these is a no-op before begin() succeeds. That is the right
// behaviour -- the alternative is a null deref -- and it cannot silently hide a
// bug, because a failed begin() is reported on serial and falls the whole system
// back to StandardDisplay.
void BufferedDisplay::fillScreen(uint16_t color) { if (canvas) canvas->fillScreen(color); }
void BufferedDisplay::fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) { if (canvas) canvas->fillRect(x, y, w, h, color); }
void BufferedDisplay::drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) { if (canvas) canvas->drawRect(x, y, w, h, color); }
void BufferedDisplay::fillRoundRect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t radius, uint16_t color) { if (canvas) canvas->fillRoundRect(x, y, w, h, radius, color); }
void BufferedDisplay::drawRoundRect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t radius, uint16_t color) { if (canvas) canvas->drawRoundRect(x, y, w, h, radius, color); }
void BufferedDisplay::fillCircle(int16_t x, int16_t y, int16_t r, uint16_t color) { if (canvas) canvas->fillCircle(x, y, r, color); }
void BufferedDisplay::drawFastVLine(int16_t x, int16_t y, int16_t h, uint16_t color) { if (canvas) canvas->drawFastVLine(x, y, h, color); }
void BufferedDisplay::drawFastHLine(int16_t x, int16_t y, int16_t w, uint16_t color) { if (canvas) canvas->drawFastHLine(x, y, w, color); }

// --- Text (draw to canvas) ---
//
// All text state (cursor, colours, font, wrap, cp437) lives on the canvas, and
// every text method forwards there, so the state stays self-consistent. Note this
// means text settings applied while a DIFFERENT implementation was active do not
// carry over -- cp437 is re-asserted in initializeSystem() for exactly that reason.
void BufferedDisplay::setCursor(int16_t x, int16_t y) { if (canvas) canvas->setCursor(x, y); }
void BufferedDisplay::setTextColor(uint16_t color) { if (canvas) canvas->setTextColor(color); }
void BufferedDisplay::setTextColor(uint16_t color, uint16_t bg) { if (canvas) canvas->setTextColor(color, bg); }
void BufferedDisplay::setTextSize(uint8_t size) { if (canvas) canvas->setTextSize(size); }
void BufferedDisplay::setFont(const GFXfont *f) { if (canvas) canvas->setFont(f); }
void BufferedDisplay::setTextWrap(bool w) { if (canvas) canvas->setTextWrap(w); }
void BufferedDisplay::cp437(bool x) { if (canvas) canvas->cp437(x); }
void BufferedDisplay::getTextBounds(const char *str, int16_t x, int16_t y, int16_t *x1, int16_t *y1, uint16_t *w, uint16_t *h) { if (canvas) canvas->getTextBounds(str, x, y, x1, y1, w, h); }
void BufferedDisplay::getTextBounds(const String &str, int16_t x, int16_t y, int16_t *x1, int16_t *y1, uint16_t *w, uint16_t *h) { if (canvas) canvas->getTextBounds(str, x, y, x1, y1, w, h); }

// --- Low-level SPI (forward to hardware -- bypasses the canvas, see DisplayBase) ---
void BufferedDisplay::startWrite() { tft.startWrite(); }
void BufferedDisplay::setAddrWindow(uint16_t x, uint16_t y, uint16_t w, uint16_t h) { tft.setAddrWindow(x, y, w, h); }
void BufferedDisplay::writePixels(uint16_t *colors, uint32_t len) { tft.writePixels(colors, len); }
void BufferedDisplay::endWrite() { tft.endWrite(); }

// --- Buffer/screen management ---
void BufferedDisplay::clearScreen() {
    if (canvas) canvas->fillScreen(0);
}

void BufferedDisplay::drawScreen() {
    if (!canvas) return;
    tft.startWrite();
    tft.setAddrWindow(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
    tft.writePixels(canvas->getBuffer(), SCREEN_WIDTH * SCREEN_HEIGHT);
    tft.endWrite();
}

// Blit a caller-owned RGB565 block into the canvas. This is how ThermalDisplay
// gets its camera quadrants in; it is the buffered equivalent of the
// setAddrWindow/writePixels pair.
//
// Clipped on all four sides. The quadrant callers are all well inside the canvas
// today, but the March version did an unclipped memcpy that trusted x/y/w/h
// blindly -- one bad offset would have corrupted the heap past the buffer with no
// obvious symptom.
void BufferedDisplay::pushPixels(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t *colors, uint32_t len) {
    if (!canvas || !colors || w == 0 || h == 0) return;
    uint16_t *buf = canvas->getBuffer();
    if (!buf) return;

    // Clip right/bottom. x and y are unsigned, so an off-screen origin can only
    // be past the far edge -- caught by the cw/ch <= 0 test below.
    int32_t cw = (int32_t)w, ch = (int32_t)h;
    if ((int32_t)x + cw > SCREEN_WIDTH)  cw = SCREEN_WIDTH  - (int32_t)x;
    if ((int32_t)y + ch > SCREEN_HEIGHT) ch = SCREEN_HEIGHT - (int32_t)y;
    if (cw <= 0 || ch <= 0) return;

    for (int32_t row = 0; row < ch; row++) {
        // Honour len: never read past the block the caller actually gave us.
        const uint32_t srcOffset = (uint32_t)row * w;
        if (srcOffset + (uint32_t)cw > len) break;
        memcpy(&buf[((int32_t)y + row) * SCREEN_WIDTH + (int32_t)x],
               &colors[srcOffset],
               (size_t)cw * sizeof(uint16_t));
    }
}

// --- Print interface (draw to canvas) ---
size_t BufferedDisplay::write(uint8_t c) { return canvas ? canvas->write(c) : 1; }
