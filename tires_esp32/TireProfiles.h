#ifndef TIRE_PROFILES_H
#define TIRE_PROFILES_H

#include <Arduino.h>

// Tire profiles (story 04). A profile bundles all the tire-specific calibration so
// switching tires swaps window + offset K + smoothing tau + per-corner baselines at
// once, instead of re-tuning each by hand. Track-mode only: the sketch reads the
// active profile to drive the window and the calculated-mode K/tau only when
// currentMode == 1; in Street mode the profile is inert.

#define PROFILE_COUNT     3
#define PROFILE_NAME_LEN  7   // visible characters (buffer is +1 for the null)

// Corner order matches the rest of the firmware: 0=FL, 1=FR, 2=RL, 3=RR.
struct TireProfile {
    char    name[PROFILE_NAME_LEN + 1]; // 8 bytes
    uint8_t windowMin;    // carcass-frame degrees (in the active temp unit's F seed)
    uint8_t windowIdeal;
    uint8_t windowMax;
    uint8_t offsetK;      // surface->carcass offset K, +degrees F (default 20)
    uint8_t tauSeconds;   // EMA smoothing time constant, seconds (default 15)
    int8_t  baseline[4];  // per-corner inflation baseline, signed degrees F
};

namespace TireProfiles {
    // Load persisted profiles + active selection from EEPROM, or seed the built-in
    // defaults (ECF / EC02 / Custom) on first boot. Must run after EEPROM.begin().
    void begin();
    // Persist all slots + the active selection to EEPROM.
    void save();

    // Runtime accessors (the sketch uses these to drive window / K / tau).
    uint8_t            activeIndex();
    const TireProfile& active();
    const TireProfile& at(uint8_t i);
    // Active profile's per-corner baseline (signed degrees F). Consumed by the
    // inflation indicator in story 06; exposed here so the value is available.
    int8_t             baselineF(uint8_t corner);

    // Menu-editing helpers. Editing operates on a working copy (g_editProfile); the
    // slot is only changed on an explicit commit.
    void loadEditFromSelected();   // copy the selected slot into the edit buffer
    void commitEditToSelected();   // copy the edit buffer into the selected slot
    void resetSelectedToDefault(); // restore the selected slot to its seed default
}

// Menu-facing globals (bound by TireMenu.cpp / edited by MenuRenderer's name editor).
extern uint8_t     g_profileSel;                     // active/selected slot (0..2)
extern const char* g_profileNameLabels[PROFILE_COUNT]; // enum labels -> slot names
extern TireProfile g_editProfile;                    // working copy for the menu

#endif // TIRE_PROFILES_H
