#include "TireProfiles.h"
#include <EEPROM.h>
#include <string.h>

extern HWCDC USBSerial;

// --- EEPROM layout (see MenuSystem.h: EEPROM_SIZE == 256) ---
// Menu bindings live at 0..49 and the menu "settings written" magic at EEPROM_SIZE-1
// (255). Profiles get their own region above the menu bindings so the two persistence
// paths never collide. Using EEPROM.put on the raw struct writes each byte verbatim,
// so camera offsets (including 0) round-trip correctly without the
// menu binding path's "byte can't store 0" caveat.
//
// Region map since #18 (sizeof(TireProfile) 25 -> 21: the per-corner baseline[4] was
// removed, so three slots run to 112):
//   50..70   slot 0      92..112   slot 2      113..124 free
//   71..91   slot 1      125       PROFILE_MAGIC        128..    session summary
// The magic stays at 125 (where #15 moved it, because the old 110 fell inside slot 2)
// rather than sliding back down -- there is no reason to churn addresses, and the gap at
// 113..124 gives the struct room to grow again. The static_assert below is the guard: it
// fails the build rather than letting a future field silently run the last slot over the
// magic (or into the session blob at 128).
static const uint16_t PROFILE_BASE        = 50;
static const uint16_t PROFILE_MAGIC_ADDR  = 125;
// Bumped for #14 (retuned ECF / EC02 seed windows), #15 (struct grew the leftOffset /
// rightOffset arrays) and now #18 (struct shrank by baseline[4]). Any layout change needs
// a new magic or the old bytes are misread field-for-field into the new shape. Forces a
// clean re-seed on first boot after the flash; intentionally wipes on-device profile
// customizations.
static const uint8_t  PROFILE_MAGIC       = 0x5D;

static_assert(PROFILE_BASE + PROFILE_COUNT * sizeof(TireProfile) <= PROFILE_MAGIC_ADDR,
              "TireProfile slots overrun PROFILE_MAGIC_ADDR -- move the magic and check "
              "the menu bindings / session blob for collisions before growing the struct");

// Menu-facing globals.
uint8_t     g_profileSel = 0;
TireProfile g_editProfile;
const char* g_profileNameLabels[PROFILE_COUNT] = { "", "", "" };

// The three slots.
static TireProfile s_profiles[PROFILE_COUNT];

// Camera crop offsets seeded into EVERY slot (#15). The cameras are mounted the same way
// regardless of which tire is fitted, so all three profiles start from one shared set and
// only diverge once somebody deliberately tunes a profile. Corner order FL, FR, RL, RR;
// still all-zero until they are re-tuned on the car.
static const uint8_t SEED_LEFT_OFFSET[4]  = { 0, 0, 0, 0 };
static const uint8_t SEED_RIGHT_OFFSET[4] = { 0, 0, 0, 0 };

// Compiled-in seed defaults. Windows are carcass-frame degrees F; K/tau are the
// literature defaults (design section 6).
static void seedDefaults() {
    // Slot 0 -- ECF (the subject tire, the Track-mode default profile): window
    // 120/160/200 carcass, K +20, tau 15.
    strncpy(s_profiles[0].name, "ECF", PROFILE_NAME_LEN);
    s_profiles[0].name[PROFILE_NAME_LEN] = '\0';
    s_profiles[0].windowMin = 120; s_profiles[0].windowIdeal = 160; s_profiles[0].windowMax = 200;
    s_profiles[0].offsetK = 20; s_profiles[0].tauSeconds = 15;

    // Slot 1 -- EC02 (the control tire, the Street-mode default profile): a lower window
    // suited to street temps.
    strncpy(s_profiles[1].name, "EC02", PROFILE_NAME_LEN);
    s_profiles[1].name[PROFILE_NAME_LEN] = '\0';
    s_profiles[1].windowMin = 110; s_profiles[1].windowIdeal = 140; s_profiles[1].windowMax = 170;
    s_profiles[1].offsetK = 20; s_profiles[1].tauSeconds = 15;

    // Slot 2 -- Custom: a neutral starting point (the previous Track window defaults).
    strncpy(s_profiles[2].name, "Custom", PROFILE_NAME_LEN);
    s_profiles[2].name[PROFILE_NAME_LEN] = '\0';
    s_profiles[2].windowMin = 100; s_profiles[2].windowIdeal = 160; s_profiles[2].windowMax = 180;
    s_profiles[2].offsetK = 20; s_profiles[2].tauSeconds = 15;

    // Camera crop offsets: identical in all three slots (#15). Seeding here also gives
    // resetSelectedToDefault() the right behaviour for free.
    for (int i = 0; i < PROFILE_COUNT; i++) {
        memcpy(s_profiles[i].leftOffset,  SEED_LEFT_OFFSET,  sizeof(SEED_LEFT_OFFSET));
        memcpy(s_profiles[i].rightOffset, SEED_RIGHT_OFFSET, sizeof(SEED_RIGHT_OFFSET));
    }
}

// Ensure a loaded slot's name is always null-terminated regardless of EEPROM contents.
static void sanitizeNames() {
    for (int i = 0; i < PROFILE_COUNT; i++)
        s_profiles[i].name[PROFILE_NAME_LEN] = '\0';
}

static void refreshLabels() {
    for (int i = 0; i < PROFILE_COUNT; i++)
        g_profileNameLabels[i] = s_profiles[i].name;
}

void TireProfiles::begin() {
    // The active slot is not restored (#14): it is transient and the caller resolves it
    // from the current mode's default profile right after this. Slot 0 is only a safe
    // placeholder so the edit buffer below has something coherent to copy.
    g_profileSel = 0;
    if (EEPROM.read(PROFILE_MAGIC_ADDR) == PROFILE_MAGIC) {
        for (int i = 0; i < PROFILE_COUNT; i++)
            EEPROM.get(PROFILE_BASE + i * (int)sizeof(TireProfile), s_profiles[i]);
        sanitizeNames();
        USBSerial.println("Tire profiles loaded from EEPROM");
    } else {
        seedDefaults();
        USBSerial.println("No saved tire profiles; using seeded defaults");
    }
    refreshLabels();
    loadEditFromSelected();
}

void TireProfiles::save() {
    for (int i = 0; i < PROFILE_COUNT; i++)
        EEPROM.put(PROFILE_BASE + i * (int)sizeof(TireProfile), s_profiles[i]);
    // The active slot is deliberately not persisted (#14) -- it is derived from the
    // current mode's default profile on every boot / mode change.
    EEPROM.write(PROFILE_MAGIC_ADDR, PROFILE_MAGIC);
    EEPROM.commit();
    refreshLabels();
    USBSerial.println("Tire profiles saved to EEPROM");
}

uint8_t TireProfiles::activeIndex() {
    return (g_profileSel < PROFILE_COUNT) ? g_profileSel : 0;
}

const TireProfile& TireProfiles::active() {
    return s_profiles[activeIndex()];
}

const TireProfile& TireProfiles::at(uint8_t i) {
    return s_profiles[(i < PROFILE_COUNT) ? i : 0];
}

void TireProfiles::loadEditFromSelected() {
    g_editProfile = s_profiles[activeIndex()];
    g_editProfile.name[PROFILE_NAME_LEN] = '\0';
}

void TireProfiles::commitEditToSelected() {
    g_editProfile.name[PROFILE_NAME_LEN] = '\0';
    s_profiles[activeIndex()] = g_editProfile;
    refreshLabels();
}

void TireProfiles::resetSelectedToDefault() {
    uint8_t keep = g_profileSel;
    TireProfile saved[PROFILE_COUNT];
    for (int i = 0; i < PROFILE_COUNT; i++) saved[i] = s_profiles[i];
    seedDefaults();
    // Restore the other slots; only the selected one reverts to its seed default.
    for (int i = 0; i < PROFILE_COUNT; i++)
        if (i != keep) s_profiles[i] = saved[i];
    refreshLabels();
    loadEditFromSelected();
}
