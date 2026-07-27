#include "ImuGateBar.h"
#include "IMUGate.h"

extern IMUGate imuGate;        // from the main sketch
extern bool getShowGateBar();  // TireMenu

// -- Geometry -------------------------------------------------------------------------
// The gutter between the tire rows is 10 px tall (Wheels.cpp: tireH = (240 - 30)/2 = 105,
// so the front row ends at y 114 and the rear starts at y 125). The bar is 4 px centered
// in that gutter; the dot is the full 10 px so it reads as a distinct marker riding on
// the bar rather than a bulge in it. x matches the tire columns (10 .. 269) so the bar
// lines up with the car above and below it.
static const int16_t BAR_X  = 10;
static const int16_t BAR_W  = 260;
static const int16_t BAR_CX = BAR_X + BAR_W / 2;   // 140
static const int16_t BAR_Y  = 118;
static const int16_t BAR_H  = 4;
static const int16_t HALF_W = BAR_W / 2;           // 130

static const int16_t DOT_W  = 8;
static const int16_t DOT_H  = 10;
static const int16_t DOT_Y  = 115;                 // fills the gutter, 115..124
static const int16_t DOT_R  = 3;

// Full-scale deflection, g. This is DERIVED, not chosen: the spec is that the default
// 0.35 g threshold paints a center segment covering 20% of the bar (40% red each side).
// Half the bar is 130 px, so zoneHalf must be 26 px at 0.35 g => 0.35 / x * 130 = 26 =>
// x = 1.75. The dot must share this scale or the dot and the segment edge would disagree
// about where the threshold is, which would defeat the whole point. Retune feel here.
static const float FULL_SCALE_G = 1.75f;

// -- Cached paint state ---------------------------------------------------------------
static bool     forceRedraw     = true;
static bool     lastVisible     = false;
static int16_t  lastDotX        = -1;   // left edge of the dot as last drawn
static int16_t  lastZoneHalf    = -1;
static uint16_t lastCenterColor = 0;

void imuGateBarInvalidate() { forceRedraw = true; }

// Repaint the bar band across [x0, x1), splitting at the zone edges so each run gets the
// right color. Used both for the full bar and for restoring the strip under a moved dot.
static void paintBarSpan(Adafruit_ST7789& d, int16_t x0, int16_t x1,
                         int16_t zoneHalf, uint16_t centerColor) {
  if (x0 < BAR_X)          x0 = BAR_X;
  if (x1 > BAR_X + BAR_W)  x1 = BAR_X + BAR_W;
  if (x1 <= x0) return;

  const int16_t zL = BAR_CX - zoneHalf;
  const int16_t zR = BAR_CX + zoneHalf;
  int16_t a, b;

  a = x0;                 b = (x1 < zL) ? x1 : zL;   // left red run
  if (b > a) d.fillRect(a, BAR_Y, b - a, BAR_H, ST77XX_RED);

  a = (x0 > zL) ? x0 : zL; b = (x1 < zR) ? x1 : zR;  // center run
  if (b > a) d.fillRect(a, BAR_Y, b - a, BAR_H, centerColor);

  a = (x0 > zR) ? x0 : zR; b = x1;                   // right red run
  if (b > a) d.fillRect(a, BAR_Y, b - a, BAR_H, ST77XX_RED);
}

void drawImuGateBar(Adafruit_ST7789& d) {
  if (!getShowGateBar()) {
    // Toggled off mid-run: wipe the gutter once so the bar does not linger. The tire map
    // never draws here, so black is the correct background to leave behind.
    if (lastVisible) {
      d.fillRect(BAR_X, DOT_Y, BAR_W, DOT_H, ST77XX_BLACK);
      lastVisible = false;
      lastDotX = -1;
      lastZoneHalf = -1;
    }
    return;
  }

  // Zone half-width in px, straight off the live menu threshold -- so turning "Lateral cg"
  // resizes the center segment as you watch. Clamped to at least 1 px so the zone never
  // disappears entirely at the low end of the range.
  int16_t zoneHalf = (int16_t)((imuGate.thresholdGate() / FULL_SCALE_G) * (float)HALF_W + 0.5f);
  if (zoneHalf < 1)      zoneHalf = 1;
  if (zoneHalf > HALF_W) zoneHalf = HALF_W;

  // isCapturing() already folds in the capture dwell, so green means "the gate is taking
  // this data right now" and yellow covers both out-of-zone and serving-the-dwell.
  const uint16_t centerColor = imuGate.isCapturing() ? ST77XX_GREEN : ST77XX_YELLOW;

  // Dot position. Clipping at full scale is intended: under heavy cornering the dot pegs
  // at the end of the bar instead of running off it, and the pegging itself is useful
  // information ("that corner was well past the gate").
  float off = (imuGate.lateralG() / FULL_SCALE_G) * (float)HALF_W;
  if (off >  (float)HALF_W) off =  (float)HALF_W;
  if (off < -(float)HALF_W) off = -(float)HALF_W;
  int16_t dotX = BAR_CX + (int16_t)(off + (off >= 0.0f ? 0.5f : -0.5f)) - DOT_W / 2;
  if (dotX < BAR_X)                 dotX = BAR_X;
  if (dotX > BAR_X + BAR_W - DOT_W) dotX = BAR_X + BAR_W - DOT_W;

  const bool full = forceRedraw || !lastVisible ||
                    zoneHalf != lastZoneHalf || centerColor != lastCenterColor;

  if (full) {
    d.fillRect(BAR_X, DOT_Y, BAR_W, DOT_H, ST77XX_BLACK);
    paintBarSpan(d, BAR_X, BAR_X + BAR_W, zoneHalf, centerColor);
  } else if (dotX != lastDotX) {
    // Only the dot moved: erase its old footprint and restore the bar strip underneath,
    // rather than repainting 260 px of bar. This runs at loop rate, so the difference
    // matters -- a full repaint every pass is ~2.6k pixels of SPI for nothing.
    d.fillRect(lastDotX, DOT_Y, DOT_W, DOT_H, ST77XX_BLACK);
    paintBarSpan(d, lastDotX, lastDotX + DOT_W, zoneHalf, centerColor);
  } else {
    return;   // nothing changed
  }

  d.fillRoundRect(dotX, DOT_Y, DOT_W, DOT_H, DOT_R, ST77XX_WHITE);

  forceRedraw     = false;
  lastVisible     = true;
  lastDotX        = dotX;
  lastZoneHalf    = zoneHalf;
  lastCenterColor = centerColor;
}
