#ifndef IMU_GATE_BAR_H
#define IMU_GATE_BAR_H

#include <Arduino.h>
#include "DisplayBase.h"

// On-screen lateral-g test bar for the IMU capture gate (#20).
//
// The gate decides whether tire reads count as straight-line data, but it had no visible
// output at all -- so there was no way to tell, while driving, whether "Lateral cg" was
// set sensibly, whether the boot orientation calibration picked the right axis, or how
// much noise the mount contributes. This paints that decision live.
//
// The bar lives in the 10 px gutter between the front and rear tire rows and reads as:
//
//   x 10 ......... 113 | 114 ..... 165 | 166 ......... 269
//     RED (40%)        |  center (20%) |   RED (40%)
//                           ( o )  <- white dot = current lateral g
//
//   center GREEN  -> in the zone and the capture dwell is served: capturing now
//   center YELLOW -> out of the zone, OR in it but still serving the capture dwell
//
// The dot tracks the same smoothed lateralG() the gate compares against the threshold, so
// the dot crossing the segment edge IS the gate decision -- not an approximation of it.
// The center segment grows and shrinks live with "Lateral cg", which is what makes the
// setting tunable by eye.

// Paint the bar. Call once per loop pass while the running display owns the screen; the
// function is cheap when nothing has moved. Suppress while the menu or the full-screen
// summary is up -- they own the whole panel.
void drawImuGateBar(DisplayBase& d);

// Force a full repaint on the next drawImuGateBar() call. The bar's gutter sits inside
// the clear rect ThreeSectionTire uses (x-bufferPix .. y+height+bufferPix), so any tire
// repaint or fillScreen wipes it. Call this from those sites rather than repainting the
// whole bar every pass, which would flood SPI at loop rate.
void imuGateBarInvalidate();

#endif // IMU_GATE_BAR_H
