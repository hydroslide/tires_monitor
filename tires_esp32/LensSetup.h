#ifndef LENS_SETUP_H
#define LENS_SETUP_H

#include <Arduino.h>
#include "DisplayBase.h"

class TempReader;

// Interactive camera field-of-view setup (#31).
//
// The lens correction turns the raw fisheye frame into a rectilinear one, and the single
// number it needs is the lens's field of view. The datasheet says 110 degrees for the
// wide-angle MLX90640, but the real lens is not exactly equidistant and the datasheet
// figure is not exactly what the optics do -- so the value has to be found by eye, on the
// car, against a real tire.
//
// You cannot do that from a numeric menu field, for the same reason #23 exists: the field
// hides the very image you are judging. This mode puts the four live camera images on
// screen and walks the value under your thumb while you watch the tire's grooves
// straighten out.
//
//   nudge left / right : -1 / +1 degree, clamped to the correction's own range
//   prev / next        : enter a confirm (there is only one value, so there is nothing
//                        to walk between -- the first up/down swipe asks the question)
//   next at the confirm  -> pulsing GREEN border. next again keeps the value and returns
//                           to the menu; prev returns to editing.
//   prev at the confirm  -> pulsing RED border. prev again restores the entry value and
//                           returns to the menu; next returns to editing.
//
// "Keep" means keep in RAM. Persisting is root "Save Config" alone -- the same contract
// every other setting has, and the same one OffsetSetup honours.
//
// Unlike OffsetSetup there is no guide line to arm, so the value is drawn as text in the
// top bezel margin. That readout is not decoration: without it you are swiping a number
// you cannot see, with no idea where you are in the range.
namespace LensSetup {

// Named for what it does ON SCREEN, not for the touch library's gesture ids -- the two
// are not the same (the library's GESTURE_LEFT is the menu's select/descend swipe). The
// sketch owns that mapping; nothing below this point sees a raw gesture id.
enum Swipe {
    NUDGE_LEFT,   // -1 degree
    NUDGE_RIGHT,  // +1 degree
    PREV,         // back out / discard
    NEXT          // forward / keep
};

// True when at least one corner has a camera, i.e. when the mode has anything to show.
// The menu action checks this before closing the menu.
bool hasEditableTargets(const TempReader* reader);

// Enter the mode, snapshotting the current value so a discard has something to put back.
void begin();

// False once a confirm has been answered; the sketch polls this to tear the mode down.
bool isActive();

// Feed one gesture.
void handleSwipe(Swipe swipe);

// Per-frame: advance the blink phase, repaint the confirm border when it changed, and
// keep the numeric readout current. Call BEFORE the camera quadrants are repainted.
void service(DisplayBase& display);

// The value under edit, for anyone that needs to mirror it (the menu binding does).
uint8_t currentFov();

}  // namespace LensSetup

#endif  // LENS_SETUP_H
