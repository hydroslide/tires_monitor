#include "OffsetSetup.h"

#include "TempReader.h"
#include "TireProfiles.h"
#include "ThermalDisplay.h"

namespace {

// One editable value: which corner, and which of its two edges.
struct Target {
    uint8_t corner;   // 0=FL, 1=FR, 2=RL, 3=RR -- the firmware's order everywhere
    bool    isRight;  // false = the left-edge offset, true = the right-edge offset
};

// Walk order is fixed: FL, FR, RL, RR, and Left before Right within a corner. Fixed rather
// than "nearest first" or "worst first" on purpose -- your eyes are on the tire images, not
// on the screen chrome, so the sequence has to be predictable enough to run without looking.
// Corners with no camera are left OUT of the list entirely: there is no image to aim there,
// so stopping on one would be a dead step you have to swipe past.
Target  targets[TIRE_COUNT * 2];
uint8_t targetCount = 0;
uint8_t targetIdx   = 0;

enum Stage : uint8_t {
    STAGE_EDIT,           // one of the values is armed and blinking
    STAGE_CONFIRM_SAVE,   // walked off the end: green border, one more NEXT keeps
    STAGE_CONFIRM_CANCEL  // walked off the front: red border, one more PREV discards
};

bool        active = false;
Stage       stage  = STAGE_EDIT;
uint8_t     slot   = 0;
TempReader* reader = nullptr;

// Entry snapshot. The mode edits the profile in place -- that is what makes the guides move
// under the live image -- so a discard needs the originals to put back. Nothing here ever
// reaches EEPROM either way; root "Save Config" remains the only writer.
uint8_t savedLeft[TIRE_COUNT];
uint8_t savedRight[TIRE_COUNT];

// Confirm-border bookkeeping, so the border is painted on a change and not at loop rate.
bool  borderLit   = false;
Stage borderStage = STAGE_EDIT;

// Blink cadence, shared by the armed guide line and the confirm border so the whole screen
// pulses on one beat.
const unsigned long PULSE_MS = 350;

// The border sits in the bezel margin around the four quadrants (which occupy x 8..271,
// y 15..224), so it never overlaps -- and is never overwritten by -- a camera image.
const int16_t SCREEN_W  = 280;
const int16_t SCREEN_H  = 240;
const int16_t BORDER_PX = 3;

void paintBorder(Adafruit_ST7789& tft, uint16_t color) {
    for (int16_t i = 0; i < BORDER_PX; i++)
        tft.drawRect(i, i, SCREEN_W - 2 * i, SCREEN_H - 2 * i, color);
}

// Push a corner's pair into the live TempReader. ThermalDisplay reads its guides from there,
// not from the profile, and outside this mode the two are only ever synced by
// initializeSystem() at menu close -- which is exactly the round trip this mode exists to
// avoid.
void pushToReader(uint8_t corner) {
    if (!reader) return;
    reader->leftPixelOffset[corner]  = g_profiles[slot].leftOffset[corner];
    reader->rightPixelOffset[corner] = g_profiles[slot].rightOffset[corner];
}

// Point ThermalDisplay at the value under edit. A confirm screen arms nothing: every guide
// then sits steady and the pulsing border is the only moving thing, which is what makes the
// warning read as "this is about the whole set", not about one line.
void syncArmed() {
    if (stage == STAGE_EDIT && targetCount > 0) {
        ThermalDisplay::setupCorner    = (int8_t)targets[targetIdx].corner;
        ThermalDisplay::setupRightSide = targets[targetIdx].isRight;
    } else {
        ThermalDisplay::setupCorner = -1;
    }
}

// Move the armed guide one thermal pixel column the way it was swiped.
//
// THE SIGN RULE: a swipe names a direction ON SCREEN, not a direction for the byte. Both
// offsets are measured inward from their own edge with 0 at the extreme, so a left offset
// grows rightward while a right offset grows LEFTWARD. Applying one fixed sign to both would
// send half the guides the wrong way under the finger.
void nudge(bool towardRight) {
    if (targetCount == 0) return;
    const Target& t = targets[targetIdx];

    int8_t delta = towardRight ? +1 : -1;   // where the LINE should go
    if (t.isRight) delta = -delta;          // ...and what that means for THIS byte

    uint8_t& v = t.isRight ? g_profiles[slot].rightOffset[t.corner]
                           : g_profiles[slot].leftOffset[t.corner];

    int next = (int)v + delta;
    if (next < 0) next = 0;
    if (next > PROFILE_OFFSET_MAX) next = PROFILE_OFFSET_MAX;
    v = (uint8_t)next;

    pushToReader(t.corner);
}

// Leave the mode. keep == false puts the entry snapshot back in both the profile and the
// live reader, so a discard is invisible downstream.
void finish(bool keep) {
    if (!keep) {
        for (uint8_t c = 0; c < TIRE_COUNT; c++) {
            g_profiles[slot].leftOffset[c]  = savedLeft[c];
            g_profiles[slot].rightOffset[c] = savedRight[c];
            pushToReader(c);
        }
    }
    active = false;
    ThermalDisplay::setupActive = false;
    ThermalDisplay::setupCorner = -1;
}

}  // namespace

namespace OffsetSetup {

bool hasEditableTargets(const TempReader* r) {
    if (!r) return false;
    for (uint8_t c = 0; c < TIRE_COUNT; c++)
        if (r->tireSensorIsCamera[c]) return true;
    return false;
}

void begin(TempReader* r, uint8_t profileSlot) {
    reader = r;
    slot   = (profileSlot < PROFILE_COUNT) ? profileSlot : 0;

    targetCount = 0;
    for (uint8_t c = 0; c < TIRE_COUNT; c++) {
        savedLeft[c]  = g_profiles[slot].leftOffset[c];
        savedRight[c] = g_profiles[slot].rightOffset[c];
        if (!reader || !reader->tireSensorIsCamera[c]) continue;
        targets[targetCount++] = { c, false };
        targets[targetCount++] = { c, true  };
    }

    targetIdx   = 0;
    stage       = STAGE_EDIT;
    borderLit   = false;
    borderStage = STAGE_EDIT;

    // Nothing to aim (no camera on any corner). Stay inactive; the caller's first poll of
    // isActive() tears the mode straight back down. The menu action checks
    // hasEditableTargets() first, so this is the belt to that braces.
    active = (targetCount > 0);
    ThermalDisplay::setupActive = active;
    syncArmed();
}

bool isActive() {
    return active;
}

void handleSwipe(Swipe swipe) {
    if (!active) return;

    switch (stage) {
    case STAGE_EDIT:
        switch (swipe) {
        case NUDGE_LEFT:  nudge(false); break;
        case NUDGE_RIGHT: nudge(true);  break;
        case NEXT:
            // Off the end is not a wrap: it is the exit, and it asks first.
            if (targetIdx + 1 < targetCount) targetIdx++;
            else                             stage = STAGE_CONFIRM_SAVE;
            break;
        case PREV:
            if (targetIdx > 0) targetIdx--;
            else               stage = STAGE_CONFIRM_CANCEL;
            break;
        }
        break;

    case STAGE_CONFIRM_SAVE:
        // Same direction again commits; the opposite one backs out to the last value.
        if (swipe == NEXT)      finish(true);
        else if (swipe == PREV) { stage = STAGE_EDIT; targetIdx = targetCount - 1; }
        break;

    case STAGE_CONFIRM_CANCEL:
        if (swipe == PREV)      finish(false);
        else if (swipe == NEXT) { stage = STAGE_EDIT; targetIdx = 0; }
        break;
    }

    syncArmed();
}

void service(Adafruit_ST7789& tft) {
    if (!active) return;

    const bool lit = ((millis() / PULSE_MS) & 1UL) == 0;
    ThermalDisplay::setupBlinkOn = lit;

    // The border is only repainted when its state actually changes -- a full-screen rect at
    // loop rate would be a lot of SPI for a 350 ms blink. Painting black on the dark half
    // erases it; nothing else ever draws in the margin.
    const bool want = (stage != STAGE_EDIT) && lit;
    if (want != borderLit || stage != borderStage) {
        uint16_t color = ST77XX_BLACK;
        if (want) color = (stage == STAGE_CONFIRM_SAVE) ? ST77XX_GREEN : ST77XX_RED;
        paintBorder(tft, color);
        borderLit   = want;
        borderStage = stage;
    }
}

}  // namespace OffsetSetup
