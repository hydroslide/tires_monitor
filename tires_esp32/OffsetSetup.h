#ifndef OFFSET_SETUP_H
#define OFFSET_SETUP_H

#include <Arduino.h>
#include "DisplayBase.h"

class TempReader;

// Interactive camera crop-offset setup (#23).
//
// The eight per-corner crop offsets are a PIXEL AIM against a camera image, and until now
// the only way to set one was a numeric field two levels down in Tire Profiles -> Offsets ->
// Front Left -> Left: a screen that hides the very image being aimed. Every value was
// entered blind, checked by backing all the way out of the menu, and corrected by going back
// in. Eight times.
//
// This mode inverts that. The four live camera images fill the screen with their crop guides
// drawn on top, exactly one guide is armed (it blinks), and swipes walk that line across the
// image while you watch it land on the tire's edge.
//
//   nudge left / right : slide the ARMED line the way you swiped
//   next / previous    : move to the next / previous value
//   next past the last value  -> pulsing GREEN border, the about-to-keep warning.
//                                next again keeps the edits and returns to the menu;
//                                previous returns to the last value.
//   previous past the first    -> pulsing RED border, the about-to-discard warning.
//                                previous again restores the entry values and returns to the
//                                menu; next returns to the first value.
//
// "Keep" means keep in the profile, in RAM. Persisting is still root "Save Config" alone --
// the same contract the numeric offset fields have had since #15/#19. This mode is another
// editor for those bytes, not a new way to write EEPROM.
//
// The mode edits the ACTIVE profile slot, which is the slot the numeric fields already
// target (retargetProfileBindings in TireMenu.cpp), so the image on screen is being cropped
// by the values under edit.
namespace OffsetSetup {

// A gesture, named for what it does ON SCREEN rather than for the touch library's gesture
// ids. The two are not interchangeable: the library's GESTURE_LEFT is the menu's
// select/descend swipe, and mapping it straight to "increase" would move half the guides
// backwards (see the sign rule in offsetDelta()). The sketch owns that mapping; nothing
// below this point ever sees a raw gesture id.
enum Swipe {
    NUDGE_LEFT,   // slide the armed guide toward the left of the image
    NUDGE_RIGHT,  // slide the armed guide toward the right of the image
    PREV,         // previous value / back out of a confirm
    NEXT          // next value / accept a confirm
};

// True when at least one corner has a camera, i.e. when the mode has anything to aim.
// The menu action checks this before closing the menu.
bool hasEditableTargets(const TempReader* reader);

// Enter the mode against `profileSlot`, snapshotting its eight offsets so a discard has
// something to put back, and arm the first value. `reader` is the LIVE TempReader: every
// edit is pushed into it as well as into the profile, which is what makes the guide move on
// the next frame rather than at the next menu close.
void begin(TempReader* reader, uint8_t profileSlot);

// False once a confirm has been answered; the sketch polls this to tear the mode down.
bool isActive();

// Feed one gesture.
void handleSwipe(Swipe swipe);

// Per-frame: advance the blink phase and repaint the confirm border when -- and only when --
// it changed. Call BEFORE the camera quadrants are repainted so the armed guide blinks in
// step with the border.
void service(DisplayBase& display);

}  // namespace OffsetSetup

#endif  // OFFSET_SETUP_H
