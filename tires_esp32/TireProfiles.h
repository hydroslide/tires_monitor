#ifndef TIRE_PROFILES_H
#define TIRE_PROFILES_H

#include <Arduino.h>

// Tire profiles (story 04). A profile bundles all the tire-specific calibration so
// switching tires swaps window + offset K + smoothing tau + per-corner camera crop
// offsets at once, instead of re-tuning each by hand.
//
// The per-corner inflation baselines were removed in #18. They were meant to be the
// "honest straight-line residual" left after the IMU gate strips the mid-corner artifact,
// but the shipped numbers encoded one track's load pattern (Lime Rock: FL/RL loaded, FR
// barely worked), not a property of the car -- static camber would push FL and FR the same
// way, and these pushed opposite ways. The geometry artifact they appeared to correct is
// already gone on a straight, which is the gate's whole job. Any genuinely static
// component is a camera AIM error, measured in pixels, and belongs in the per-corner crop
// offsets (#15) where it also fixes the displayed band temps -- not in a degrees-F fudge
// that only shifts the edge-vs-center delta. If residual cornering heat ever proves to
// bias the verdict, the fix is a straight-line dwell (one global number), not twelve
// hand-entered per-profile ones.
//
// Since #14 a profile is the single source of truth for the tire temp window in BOTH
// modes: Street and Track no longer carry their own Min/Ideal/Max, they each name a
// default profile (Street Settings / Track Settings -> "Default Profile") and the active
// profile supplies the window. The active selection is transient -- it is resolved from
// the current mode's default at boot and re-snapped on every mode change (see
// applyModeDefaultProfile() in TireMenu), and is never persisted. Calculated display reads
// this profile's K/tau in BOTH modes as of #16; the inflation indicator and the session /
// balance features are what still gate on currentMode == 1.

#define PROFILE_COUNT      3
#define PROFILE_NAME_LEN   7   // visible characters (buffer is +1 for the null)
#define PROFILE_OFFSET_MAX 16  // camera crop offset ceiling, in thermal pixel columns

// Corner order matches the rest of the firmware: 0=FL, 1=FR, 2=RL, 3=RR.
//
// Kept byte-packed on purpose (every member is 1 byte, so alignment is 1 and there is no
// padding): the whole struct is persisted verbatim with EEPROM.put, and PROFILE_COUNT
// slots have to fit between PROFILE_BASE and PROFILE_MAGIC_ADDR. Adding a wider member
// would both grow the region and introduce padding -- see the static_assert and the
// region map in TireProfiles.cpp before changing anything here.
struct TireProfile {
    char    name[PROFILE_NAME_LEN + 1]; // 8 bytes
    uint8_t windowMin;    // carcass-frame degrees (in the active temp unit's F seed)
    uint8_t windowIdeal;
    uint8_t windowMax;
    uint8_t offsetK;      // surface->carcass offset K, +degrees F (default 20)
                          // menu label: "Carcass Offset" (#17)
    uint8_t tauSeconds;   // EMA smoothing time constant, seconds (default 15)
                          // menu label: "Carcass Lag" (#17)
    // Camera crop offsets (#15), in thermal-frame pixel columns, 0..PROFILE_OFFSET_MAX.
    // These were eight loose globals under "Camera Settings"; different tires and
    // mountings want different crops, so they belong to the profile. Read back through
    // getLeftPixelOffset() / getRightPixelOffset() in TireMenu, which resolve against the
    // ACTIVE profile -- TempReader / ThermalDisplay / NBP see no change.
    uint8_t leftOffset[4];   // left-edge crop, per corner
    uint8_t rightOffset[4];  // right-edge crop, per corner
};

namespace TireProfiles {
    // Load persisted profiles from EEPROM, or seed the built-in defaults (ECF / EC02 /
    // Custom) on first boot. Must run after EEPROM.begin(). The active slot is NOT read
    // back here (#14) -- the caller resolves it from the current mode's default profile.
    void begin();
    // Persist all slots to EEPROM. The active selection is transient and not saved.
    void save();

    // Runtime accessors (the sketch uses these to drive window / K / tau).
    uint8_t            activeIndex();
    const TireProfile& active();
    const TireProfile& at(uint8_t i);

    void resetSelectedToDefault(); // restore the selected slot to its seed default
}

// Menu-facing globals (bound by TireMenu.cpp / edited by MenuRenderer's name editor).
extern uint8_t     g_profileSel;                     // active/selected slot (0..2), transient
extern const char* g_profileNameLabels[PROFILE_COUNT]; // enum labels -> slot names

// The live slots. #19 removed the old single g_editProfile working copy: with one shared
// buffer the selector could only hold one slot at a time, so switching profiles had to
// either discard or commit the pending edits -- editing A, switching to B, editing B and
// saving lost A. All PROFILE_COUNT profiles (63 bytes total) simply stay resident instead,
// and TireMenu re-points its bindings at the selected slot (retargetProfileBindings), so
// switching only changes WHICH slot the UI edits. Edits therefore land in the slot
// immediately and root "Save Settings" persists every slot at once -- the same live-global
// + explicit-save model every other menu setting already uses.
//
// Exposed (rather than file-static) so TireMenu can take &g_profiles[i].field. That is a
// link-time address constant, so there is no static-init-order hazard.
extern TireProfile g_profiles[PROFILE_COUNT];

#endif // TIRE_PROFILES_H
