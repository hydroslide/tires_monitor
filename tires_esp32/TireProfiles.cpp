#include "TireProfiles.h"
#include <EEPROM.h>
#include <string.h>

extern HWCDC USBSerial;

// --- EEPROM layout (see MenuSystem.h: EEPROM_SIZE == 128) ---
// Menu bindings live at 0..48 and the menu "settings written" magic at EEPROM_SIZE-1
// (127). Profiles get their own region above the menu bindings so the two persistence
// paths never collide. Using EEPROM.put on the raw struct writes each byte verbatim,
// so signed baselines (including 0) round-trip correctly without the menu binding
// path's "byte can't store 0" caveat.
static const uint16_t PROFILE_BASE        = 50;
static const uint16_t PROFILE_MAGIC_ADDR  = 110;
static const uint16_t PROFILE_ACTIVE_ADDR = 111;
static const uint8_t  PROFILE_MAGIC       = 0x5A;

// Menu-facing globals.
uint8_t     g_profileSel = 0;
TireProfile g_editProfile;
const char* g_profileNameLabels[PROFILE_COUNT] = { "", "", "" };

// The three slots.
static TireProfile s_profiles[PROFILE_COUNT];

// Compiled-in seed defaults. Windows are carcass-frame degrees F; K/tau are the
// literature defaults (design section 6). Baselines are the straight-line spread at
// the known-good pressure (design 3.1), corner order FL, FR, RL, RR.
static void seedDefaults() {
    // Slot 0 -- ECF (the subject tire): window ~160-200 carcass, K +20, tau 15,
    // baselines FL -3 / FR +4 / RL -6 / RR 0.
    strncpy(s_profiles[0].name, "ECF", PROFILE_NAME_LEN);
    s_profiles[0].name[PROFILE_NAME_LEN] = '\0';
    s_profiles[0].windowMin = 160; s_profiles[0].windowIdeal = 180; s_profiles[0].windowMax = 200;
    s_profiles[0].offsetK = 20; s_profiles[0].tauSeconds = 15;
    s_profiles[0].baseline[0] = -3; s_profiles[0].baseline[1] = 4;
    s_profiles[0].baseline[2] = -6; s_profiles[0].baseline[3] = 0;

    // Slot 1 -- EC02 (the control tire): its own window and milder baselines.
    strncpy(s_profiles[1].name, "EC02", PROFILE_NAME_LEN);
    s_profiles[1].name[PROFILE_NAME_LEN] = '\0';
    s_profiles[1].windowMin = 150; s_profiles[1].windowIdeal = 175; s_profiles[1].windowMax = 195;
    s_profiles[1].offsetK = 20; s_profiles[1].tauSeconds = 15;
    s_profiles[1].baseline[0] = -2; s_profiles[1].baseline[1] = 2;
    s_profiles[1].baseline[2] = -3; s_profiles[1].baseline[3] = 0;

    // Slot 2 -- Custom: a neutral starting point (the previous Track window defaults).
    strncpy(s_profiles[2].name, "Custom", PROFILE_NAME_LEN);
    s_profiles[2].name[PROFILE_NAME_LEN] = '\0';
    s_profiles[2].windowMin = 100; s_profiles[2].windowIdeal = 160; s_profiles[2].windowMax = 180;
    s_profiles[2].offsetK = 20; s_profiles[2].tauSeconds = 15;
    s_profiles[2].baseline[0] = 0; s_profiles[2].baseline[1] = 0;
    s_profiles[2].baseline[2] = 0; s_profiles[2].baseline[3] = 0;
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
    if (EEPROM.read(PROFILE_MAGIC_ADDR) == PROFILE_MAGIC) {
        for (int i = 0; i < PROFILE_COUNT; i++)
            EEPROM.get(PROFILE_BASE + i * (int)sizeof(TireProfile), s_profiles[i]);
        uint8_t a = EEPROM.read(PROFILE_ACTIVE_ADDR);
        g_profileSel = (a < PROFILE_COUNT) ? a : 0;
        sanitizeNames();
        USBSerial.println("Tire profiles loaded from EEPROM");
    } else {
        seedDefaults();
        g_profileSel = 0;
        USBSerial.println("No saved tire profiles; using seeded defaults");
    }
    refreshLabels();
    loadEditFromSelected();
}

void TireProfiles::save() {
    for (int i = 0; i < PROFILE_COUNT; i++)
        EEPROM.put(PROFILE_BASE + i * (int)sizeof(TireProfile), s_profiles[i]);
    EEPROM.write(PROFILE_ACTIVE_ADDR, g_profileSel);
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

int8_t TireProfiles::baselineF(uint8_t corner) {
    if (corner >= 4) return 0;
    return s_profiles[activeIndex()].baseline[corner];
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
