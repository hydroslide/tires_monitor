#include "LensSetup.h"

#include "TempReader.h"
#include "ThermalDisplay.h"

// The menu owns the persisted value; this mode is another editor for it, exactly as
// OffsetSetup is another editor for the profile's crop bytes.
extern uint8_t getLensFovDegrees();
extern void    setLensFovDegrees(uint8_t v);
extern bool    getLensCorrectEnabled();
extern bool    getLensFitToView();

namespace {

enum Stage : uint8_t {
    STAGE_EDIT,           // swiping the value
    STAGE_CONFIRM_SAVE,   // green border, one more NEXT keeps
    STAGE_CONFIRM_CANCEL  // red border, one more PREV discards
};

bool    active    = false;
Stage   stage     = STAGE_EDIT;
uint8_t savedFov  = 110;   // entry snapshot, so a discard has something to put back
uint8_t shownFov  = 0;     // what the readout currently says, to avoid repainting at loop rate

// Blink cadence, deliberately the same 350 ms OffsetSetup uses so the two modes pulse on
// one beat -- they are the same gesture language and should not feel like different
// devices.
const unsigned long PULSE_MS = 350;

// The border sits in the bezel margin around the four quadrants (x 8..271, y 15..224), so
// it never overlaps -- and is never overwritten by -- a camera image. Same geometry as
// OffsetSetup's border, for the same reason.
const int16_t SCREEN_W  = 280;
const int16_t SCREEN_H  = 240;
const int16_t BORDER_PX = 3;

// Confirm-border bookkeeping, so the border is painted on a change and not at loop rate.
bool  borderLit   = false;
Stage borderStage = STAGE_EDIT;

void paintBorder(DisplayBase& display, uint16_t color) {
    for (int16_t i = 0; i < BORDER_PX; i++)
        display.drawRect(i, i, SCREEN_W - 2 * i, SCREEN_H - 2 * i, color);
}

// Push the edit into the live correction so the NEXT frame is remapped. This is the whole
// point of the mode: the value has to move the picture under your thumb, not at menu
// close. The table rebuild is inside configureLens() and only fires on a real change.
void pushToReader() {
    TempReader::configureLens(getLensCorrectEnabled(), getLensFovDegrees(), getLensFitToView());
}

// The numeric readout, in the top bezel margin above the quadrants. Drawn with an opaque
// background so each repaint erases the last one -- no fill/redraw flicker, and nothing
// else ever draws in the margin.
//
// Without this you are swiping a number you cannot see. OffsetSetup can get away with no
// readout because the pulsing guide line IS the feedback; here there is no line, and "is
// that 104 or 118?" is not answerable from the image alone.
void paintReadout(DisplayBase& display, uint8_t fov) {
    display.setTextSize(2);
    display.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
    display.setCursor(BORDER_PX + 4, 0);
    display.print("FOV ");
    if (fov < 100) display.print(' ');   // fixed width, so the degree ring never jitters
    display.print((int)fov);
    display.print("\xF8");               // CP437 degree ring; setup() calls cp437(true)
}

void finish(bool keep) {
    if (!keep) {
        setLensFovDegrees(savedFov);
        pushToReader();
    }
    active = false;
    ThermalDisplay::setupActive = false;
    ThermalDisplay::setupCorner = -1;
    ThermalDisplay::setupGuides = true;   // hand the guides back to OffsetSetup / Show Offsets
}

void nudge(int delta) {
    int next = (int)getLensFovDegrees() + delta;
    if (next < (int)TempReader::LENS_FOV_MIN) next = TempReader::LENS_FOV_MIN;
    if (next > (int)TempReader::LENS_FOV_MAX) next = TempReader::LENS_FOV_MAX;
    setLensFovDegrees((uint8_t)next);
    pushToReader();
}

}  // namespace

namespace LensSetup {

bool hasEditableTargets(const TempReader* r) {
    if (!r) return false;
    for (uint8_t c = 0; c < TIRE_COUNT; c++)
        if (r->tireSensorIsCamera[c]) return true;
    return false;
}

void begin() {
    savedFov    = getLensFovDegrees();
    shownFov    = 0;             // force the first readout paint
    stage       = STAGE_EDIT;
    borderLit   = false;
    borderStage = STAGE_EDIT;
    active      = true;

    // Borrow OffsetSetup's full-frame view (never stretch-crop -- you are judging the
    // shape of the WHOLE frame, and a cropped image hides the edges where the distortion
    // is worst) but suppress its guides.
    ThermalDisplay::setupActive = true;
    ThermalDisplay::setupCorner = -1;
    ThermalDisplay::setupGuides = false;

    // Make sure what is on screen is actually being corrected by the value under edit,
    // even if the menu never pushed it.
    pushToReader();
}

bool isActive() {
    return active;
}

uint8_t currentFov() {
    return getLensFovDegrees();
}

void handleSwipe(Swipe swipe) {
    if (!active) return;

    switch (stage) {
    case STAGE_EDIT:
        switch (swipe) {
        case NUDGE_LEFT:  nudge(-1); break;
        case NUDGE_RIGHT: nudge(+1); break;
        // One value means there is nothing to walk between, so the first up/down swipe
        // goes straight to the question rather than stepping through a list of one.
        case NEXT:        stage = STAGE_CONFIRM_SAVE;   break;
        case PREV:        stage = STAGE_CONFIRM_CANCEL; break;
        }
        break;

    case STAGE_CONFIRM_SAVE:
        // Same direction again commits; the opposite one backs out to editing.
        if (swipe == NEXT)      finish(true);
        else if (swipe == PREV) stage = STAGE_EDIT;
        break;

    case STAGE_CONFIRM_CANCEL:
        if (swipe == PREV)      finish(false);
        else if (swipe == NEXT) stage = STAGE_EDIT;
        break;
    }
}

void service(DisplayBase& display) {
    if (!active) return;

    const bool lit = ((millis() / PULSE_MS) & 1UL) == 0;
    ThermalDisplay::setupBlinkOn = lit;

    const uint8_t fov = getLensFovDegrees();
    if (fov != shownFov) {
        paintReadout(display, fov);
        shownFov = fov;
    }

    // The border is only repainted when its state actually changes -- a full-screen rect
    // at loop rate would be a lot of SPI for a 350 ms blink. Painting black on the dark
    // half erases it; nothing else ever draws in the margin.
    const bool want = (stage != STAGE_EDIT) && lit;
    if (want != borderLit || stage != borderStage) {
        uint16_t color = ST77XX_BLACK;
        if (want) color = (stage == STAGE_CONFIRM_SAVE) ? ST77XX_GREEN : ST77XX_RED;
        paintBorder(display, color);
        borderLit   = want;
        borderStage = stage;
        // The border's black erase pass runs along the top margin too, so the readout has
        // to be put back after it.
        paintReadout(display, fov);
    }
}

}  // namespace LensSetup
