#include "TireMenu.h"
#include "MenuRenderer.h"
#include "TireProfiles.h"

extern MenuRenderer menuRenderer;

// ----------------------------------------------------
//  1) Global variables for each setting
// ----------------------------------------------------

//
// -- Enums --
static uint8_t currentMode = 0; // 0=Street, 1=Track
static const char* currentModeLabels[] = {"Street", "Track"};

static uint8_t temperatureScale = 0; // 0=Farenheit, 1=Celsius
static const char* tempScaleLabels[] = {"F", "C"};

static uint8_t nightBrightness   = 25;

// -- Per-mode default tire profile (#14) --
// A mode no longer carries its own temp window; it names the tire profile it starts from,
// and the profile is the single source of truth for Min/Ideal/Max (plus K / tau / crop).
// Both are enum indices into the profile slots: Street -> EC02 (1), Track -> ECF (0).
static uint8_t streetProfile = 1;
static uint8_t trackProfile  = 0;

// -- Calculated display mode (story 03; Track-mode only) --
// 0=Raw surface, 1=Calculated (EMA_tau(surface)+K carcass estimate).
static uint8_t calcDisplayMode = 0;
static const char* calcDisplayModeLabels[] = {"Raw", "Calculated"};

// -- Balance summary show/hide (story 05; Track-mode only) --
// Governs whether the front/rear + left/right balance readout is offered. When off the
// "Balance" summary item reports that it is hidden instead of opening.
static bool showBalance = true;

// -- Auto-seal on stationary (story 01; Track-mode only) --
// When on, a running session self-seals after a sustained still period (a backstop for
// a forgotten swipe-to-end). Best-effort IMU-stillness only (no speed channel), so it
// defaults off.
static bool autoSealStationary = false;

// -- Inflation indicator (story 06; Track-mode only) --
// Gates & latches the over/under inflation verdict: computed only on straight-line
// (captured) frames, from calculated temps, presented latched (#18 dropped the
// per-corner baseline correction).
// On by default; inert / hidden in Street mode.
static bool inflationIndicator = true;

// -- Hardware Temp Sensor Indices --
static uint8_t frontLeftTempIndex  = 0;
static uint8_t frontRightTempIndex = 0;
static uint8_t rearLeftTempIndex   = 0;
static uint8_t rearRightTempIndex  = 0;

// -- Camera Offsets --
// Per-corner crop offsets are no longer globals (#15): they live in TireProfile, are
// edited under Tire Profiles -> Offsets, and are read back through the accessors at the
// bottom of this file. "Camera Settings" keeps only the device-wide display toggles.

static bool useThermalGradient = true;
static bool testEnabled = true;
static bool showPixelOffsets = true;
static bool highFrequencyUpdates = false;
static bool showSegmentDeltas = false;
static uint8_t minInflationDeltaPct = 10;
static uint8_t minAlignmentDeltaPct = 15;

// -- IMU capture gate (global; story 02) --
static bool    imuGateEnabled       = true; // suppress reads while cornering
static uint8_t lateralGateCentiG    = 35;   // |lateral g| threshold, centi-g (0.35 g)
static uint8_t alertDwellTenths     = 25;   // latch dwell, tenths of a second (2.5 s)
static uint8_t imuOrient            = 0;    // lateral-axis map: 0=Auto,1=X,2=Y,3=Z
static const char* imuOrientLabels[] = {"Auto", "X", "Y", "Z"};


// ----------------------------------------------------
//  2) MenuValueBindings
// ----------------------------------------------------
static MenuValueBinding currentModeBinding = {
    VALUE_ENUM,
    &currentMode,
    nullptr,
    0, // minByte
    0, // maxByte
    0, // eepromAddress (or -1 if ignoring)
    currentModeLabels,
    2  // enumCount
};

static MenuValueBinding temperatureScaleBinding = {
    VALUE_ENUM,
    &temperatureScale,
    nullptr,
    0,
    0,
    2, // eepromAddress or -1
    tempScaleLabels,
    2
};

// Night Brightenss
static MenuValueBinding nightBrightnessBinding = {
    VALUE_BYTE,
    &nightBrightness,
    nullptr,
    0,
    100,
    28,
    nullptr,
    0
};

// Per-mode default tire profile (#14). Persisted at two of the addresses freed by the
// removed Street/Track Min/Ideal/Max bytes (6 and 8); the rest (10, 14, 16, 18) are now
// free. The labels are the live profile slot names, so renaming a profile renames the
// choices here too.
static MenuValueBinding streetProfileBinding = {
    VALUE_ENUM,
    &streetProfile,
    nullptr,
    0,
    0,
    6,
    g_profileNameLabels,
    PROFILE_COUNT
};

static MenuValueBinding trackProfileBinding = {
    VALUE_ENUM,
    &trackProfile,
    nullptr,
    0,
    0,
    8,
    g_profileNameLabels,
    PROFILE_COUNT
};

// Calculated display mode (EEPROM addr 1; free byte, magic-sentinel load makes 0 safe)
static MenuValueBinding calcDisplayModeBinding = {
    VALUE_ENUM,
    &calcDisplayMode,
    nullptr,
    0,
    0,
    1,
    calcDisplayModeLabels,
    2
};

// Balance summary show/hide (EEPROM addr 49; free byte between the menu bindings and
// the tire-profile region at 50+). The magic-sentinel load makes the false/0 case safe.
static MenuValueBinding showBalanceBinding = {
    VALUE_BOOL,
    &showBalance,
    nullptr,
    0,
    0,
    49,
    nullptr,
    0
};

// Auto-seal on stationary (EEPROM addr 3; free byte among the menu bindings). The
// magic-sentinel load makes the false/0 case safe.
static MenuValueBinding autoSealStationaryBinding = {
    VALUE_BOOL,
    &autoSealStationary,
    nullptr,
    0,
    0,
    3,
    nullptr,
    0
};

// Hardware -> Temp Sensor Indices
static MenuValueBinding frontLeftIndexBinding = {
    VALUE_BYTE,
    &frontLeftTempIndex,
    nullptr,
    0,
    7,
    20,
    nullptr,
    0
};

static MenuValueBinding frontRightIndexBinding = {
    VALUE_BYTE,
    &frontRightTempIndex,
    nullptr,
    0,
    7,
    22,
    nullptr,
    0
};

static MenuValueBinding rearLeftIndexBinding = {
    VALUE_BYTE,
    &rearLeftTempIndex,
    nullptr,
    0,
    7,
    24,
    nullptr,
    0
};

static MenuValueBinding rearRightIndexBinding = {
    VALUE_BYTE,
    &rearRightTempIndex,
    nullptr,
    0,
    7,
    26,
    nullptr,
    0
};

static MenuValueBinding useThermalGradientBinding = {
    VALUE_BOOL,
    &useThermalGradient,
    nullptr,
    0,
    0,
    30,
    nullptr,
    0
};

// EEPROM 31..38 used to hold the eight global camera offsets. #15 moved the offsets into
// the tire profile (persisted verbatim in the profile region), so 31 was re-purposed for
// the inflation indicator (see below) and 32..38 are now free.

static MenuValueBinding showPixelOffsetsBinding = {
    VALUE_BOOL,
    &showPixelOffsets,
    nullptr,
    0,
    0,
    39,
    nullptr,
    0
};

static MenuValueBinding testEnabledBinding = {
    VALUE_BOOL,
    &testEnabled,
    nullptr,
    0,
    0,
    40,
    nullptr,
    0
};
static MenuValueBinding highFrequencyUpdatesBinding = {
    VALUE_BOOL,
    &highFrequencyUpdates,
    nullptr,
    0,
    0,
    41,
    nullptr,
    0
};
static MenuValueBinding showSegmentDeltasBinding = {
    VALUE_BOOL,
    &showSegmentDeltas,
    nullptr,
    0,
    0,
    42,
    nullptr,
    0
};

static MenuValueBinding minInflationDeltaPctBinding = {
    VALUE_BYTE,
    &minInflationDeltaPct,
    nullptr,
    0,
    16,
    43,
    nullptr,
    0
};
static MenuValueBinding minAlignmentDeltaPctBinding = {
    VALUE_BYTE,
    &minAlignmentDeltaPct,
    nullptr,
    0,
    16,
    44,
    nullptr,
    0
};

// IMU capture gate settings (EEPROM 45..48; magic-sentinel load makes 0 safe)
static MenuValueBinding imuGateEnabledBinding = {
    VALUE_BOOL,
    &imuGateEnabled,
    nullptr,
    0,
    0,
    45,
    nullptr,
    0
};
static MenuValueBinding lateralGateCentiGBinding = {
    VALUE_BYTE,
    &lateralGateCentiG,
    nullptr,
    10,   // 0.10 g
    100,  // 1.00 g
    46,
    nullptr,
    0
};
static MenuValueBinding alertDwellTenthsBinding = {
    VALUE_BYTE,
    &alertDwellTenths,
    nullptr,
    5,    // 0.5 s
    100,  // 10 s
    47,
    nullptr,
    0
};
static MenuValueBinding imuOrientBinding = {
    VALUE_ENUM,
    &imuOrient,
    nullptr,
    0,
    0,
    48,
    imuOrientLabels,
    4
};

// Inflation indicator on/off. On by default; the magic-sentinel load makes 0 safe.
// EEPROM 31 -- one of the bytes freed by #15. It previously shared byte 49 with Show
// Balance, so toggling either one silently clobbered the other on the next save/load;
// #15 freed a whole run of bytes, so the two get one each.
static MenuValueBinding inflationIndicatorBinding = {
    VALUE_BOOL,
    &inflationIndicator,
    nullptr,
    0,
    0,
    31,
    nullptr,
    0
};

// -- Tire profiles (story 04) --
// The selector and all edit fields bind to TireProfiles globals. Since #14 the selector
// is TRANSIENT (EEPROM_NO_PERSIST): the active slot is always resolved from the current
// mode's default profile at boot and on every mode change, so persisting a "last profile"
// would only fight that. A manual pick here still takes effect immediately and holds
// until the next mode change or reboot.
//
// Since #15 every edit field below is TRANSIENT too. They edit a working copy
// (g_editProfile) that "Save Prof" commits into the selected slot, and the slot is what
// gets persisted -- byte-verbatim, in the profile region. They used to ALSO be tree-walked
// to EEPROM 113..121 as a harmless duplicate, but #15 grew TireProfile to 25 bytes, so
// three slots now run to byte 124 and would have overwritten exactly those bytes. Rather
// than shuffle the duplicate elsewhere it is gone: the edit buffer is derived state (both
// TireProfiles::begin() and applyModeDefaultProfile() reload it from the slot), so there
// was never anything worth persisting. Dropping it also spares the new offset fields the
// menu path's "a VALUE_BYTE stored as 0 is ambiguous" history (see MenuSystem.cpp).
static MenuValueBinding profileSelectBinding = {
    VALUE_ENUM,
    &g_profileSel,
    nullptr,
    0,
    0,
    EEPROM_NO_PERSIST,
    g_profileNameLabels,
    PROFILE_COUNT
};
static MenuValueBinding profileWindowMinBinding = {
    VALUE_BYTE, &g_editProfile.windowMin, nullptr, 0, 255, EEPROM_NO_PERSIST, nullptr, 0
};
static MenuValueBinding profileWindowIdealBinding = {
    VALUE_BYTE, &g_editProfile.windowIdeal, nullptr, 0, 255, EEPROM_NO_PERSIST, nullptr, 0
};
static MenuValueBinding profileWindowMaxBinding = {
    VALUE_BYTE, &g_editProfile.windowMax, nullptr, 0, 255, EEPROM_NO_PERSIST, nullptr, 0
};
static MenuValueBinding profileOffsetKBinding = {
    VALUE_BYTE, &g_editProfile.offsetK, nullptr, 0, 80, EEPROM_NO_PERSIST, nullptr, 0
};
static MenuValueBinding profileTauBinding = {
    VALUE_BYTE, &g_editProfile.tauSeconds, nullptr, 1, 60, EEPROM_NO_PERSIST, nullptr, 0
};
// Per-corner camera crop offsets (#15), corner order FL, FR, RL, RR. Same edit-buffer /
// transient treatment as the fields above; the range mirrors the old global offsets.
static MenuValueBinding profileLeftOffsetFLBinding = {
    VALUE_BYTE, &g_editProfile.leftOffset[0], nullptr, 0, PROFILE_OFFSET_MAX, EEPROM_NO_PERSIST, nullptr, 0
};
static MenuValueBinding profileRightOffsetFLBinding = {
    VALUE_BYTE, &g_editProfile.rightOffset[0], nullptr, 0, PROFILE_OFFSET_MAX, EEPROM_NO_PERSIST, nullptr, 0
};
static MenuValueBinding profileLeftOffsetFRBinding = {
    VALUE_BYTE, &g_editProfile.leftOffset[1], nullptr, 0, PROFILE_OFFSET_MAX, EEPROM_NO_PERSIST, nullptr, 0
};
static MenuValueBinding profileRightOffsetFRBinding = {
    VALUE_BYTE, &g_editProfile.rightOffset[1], nullptr, 0, PROFILE_OFFSET_MAX, EEPROM_NO_PERSIST, nullptr, 0
};
static MenuValueBinding profileLeftOffsetRLBinding = {
    VALUE_BYTE, &g_editProfile.leftOffset[2], nullptr, 0, PROFILE_OFFSET_MAX, EEPROM_NO_PERSIST, nullptr, 0
};
static MenuValueBinding profileRightOffsetRLBinding = {
    VALUE_BYTE, &g_editProfile.rightOffset[2], nullptr, 0, PROFILE_OFFSET_MAX, EEPROM_NO_PERSIST, nullptr, 0
};
static MenuValueBinding profileLeftOffsetRRBinding = {
    VALUE_BYTE, &g_editProfile.leftOffset[3], nullptr, 0, PROFILE_OFFSET_MAX, EEPROM_NO_PERSIST, nullptr, 0
};
static MenuValueBinding profileRightOffsetRRBinding = {
    VALUE_BYTE, &g_editProfile.rightOffset[3], nullptr, 0, PROFILE_OFFSET_MAX, EEPROM_NO_PERSIST, nullptr, 0
};

// ----------------------------------------------------
//  3) Submenu Item Arrays
// ----------------------------------------------------
// "Display" (Raw/Calculated) is one shared global byte surfaced in both mode menus (#16),
// not a per-mode setting -- changing it here changes it under Track Settings too.
static MenuItem streetSettingsMenu[] = {
    { "Default Profile", MENU_VALUE, nullptr, nullptr, 0, &streetProfileBinding   },
    { "Display",         MENU_VALUE, nullptr, nullptr, 0, &calcDisplayModeBinding }
};

// Balance summary action (defined in section 6). Opens the front/rear + left/right
// readout; Track-mode-only and honors the Show Balance toggle.
static void doShowBalance();
// Session summary recall action (story 01). Opens the last sealed summary; Track-only.
static void doViewSummary();

static MenuItem trackSettingsMenu[] = {
    { "Default Profile", MENU_VALUE, nullptr,      nullptr, 0, &trackProfileBinding    },
    { "Display",      MENU_VALUE,  nullptr,        nullptr, 0, &calcDisplayModeBinding },
    { "Show Balance", MENU_VALUE,  nullptr,        nullptr, 0, &showBalanceBinding     },
    { "Balance",      MENU_ACTION, doShowBalance,  nullptr, 0, nullptr                 },
    { "View Summary", MENU_ACTION, doViewSummary,  nullptr, 0, nullptr                 },
    { "Auto-Seal",    MENU_VALUE,  nullptr,        nullptr, 0, &autoSealStationaryBinding },
    { "Inflation",    MENU_VALUE,  nullptr,        nullptr, 0, &inflationIndicatorBinding }
};

static MenuItem tempSensorIndicesMenu[] = {
    { "Front Left",  MENU_VALUE, nullptr, nullptr, 0, &frontLeftIndexBinding  },
    { "Front Right", MENU_VALUE, nullptr, nullptr, 0, &frontRightIndexBinding },
    { "Rear Left",   MENU_VALUE, nullptr, nullptr, 0, &rearLeftIndexBinding   },
    { "Rear Right",  MENU_VALUE, nullptr, nullptr, 0, &rearRightIndexBinding  }
};

static MenuItem hardwareSettingsMenu[] = {
    {
      "Temp Sensor Indices",
      MENU_SUBMENU,
      nullptr,
      tempSensorIndicesMenu,
      sizeof(tempSensorIndicesMenu)/sizeof(MenuItem),
      nullptr
    }
};

// "Camera Settings". The per-corner offset VALUES moved to Tire Profiles -> Offsets (#15);
// what is left here is genuinely device-wide display behaviour, so it stays global.
static MenuItem pixelOffsetsMenu[] = {
    {
        "Show Offsets",
        MENU_VALUE,
        nullptr,
        nullptr,
        0,
        &showPixelOffsetsBinding
    },
    {
        "Thermal Gradient",
        MENU_VALUE,
        nullptr,
        nullptr,
        0,
        &useThermalGradientBinding
    },
    {
        "Hi Freq Updates",
        MENU_VALUE,
        nullptr,
        nullptr,
        0,
        &highFrequencyUpdatesBinding
    },
    {
        "Segment Deltas",
        MENU_VALUE,
        nullptr,
        nullptr,
        0,
        &showSegmentDeltasBinding
    },
    { "Inflation Delta %", MENU_VALUE, nullptr, nullptr, 0, &minInflationDeltaPctBinding },
    { "Alignment Delta %", MENU_VALUE, nullptr, nullptr, 0, &minAlignmentDeltaPctBinding },
};



static MenuItem imuGateMenu[] = {
    { "Gate Enable",     MENU_VALUE, nullptr, nullptr, 0, &imuGateEnabledBinding   },
    { "Lateral cg",      MENU_VALUE, nullptr, nullptr, 0, &lateralGateCentiGBinding },
    { "Dwell 0.1s",      MENU_VALUE, nullptr, nullptr, 0, &alertDwellTenthsBinding  },
    { "Orientation",     MENU_VALUE, nullptr, nullptr, 0, &imuOrientBinding         },
};

// Per-profile camera crop offsets (#15). Mirrors the corner / Left-Right shape the old
// global "Camera Settings" offsets had, so the tuning gestures are unchanged -- only the
// values now belong to the selected profile's edit buffer, saved by "Save Prof".
static MenuItem profileOffsetsFrontLeftMenu[] = {
    { "Left",  MENU_VALUE, nullptr, nullptr, 0, &profileLeftOffsetFLBinding  },
    { "Right", MENU_VALUE, nullptr, nullptr, 0, &profileRightOffsetFLBinding }
};

static MenuItem profileOffsetsFrontRightMenu[] = {
    { "Left",  MENU_VALUE, nullptr, nullptr, 0, &profileLeftOffsetFRBinding  },
    { "Right", MENU_VALUE, nullptr, nullptr, 0, &profileRightOffsetFRBinding }
};

static MenuItem profileOffsetsRearLeftMenu[] = {
    { "Left",  MENU_VALUE, nullptr, nullptr, 0, &profileLeftOffsetRLBinding  },
    { "Right", MENU_VALUE, nullptr, nullptr, 0, &profileRightOffsetRLBinding }
};

static MenuItem profileOffsetsRearRightMenu[] = {
    { "Left",  MENU_VALUE, nullptr, nullptr, 0, &profileLeftOffsetRRBinding  },
    { "Right", MENU_VALUE, nullptr, nullptr, 0, &profileRightOffsetRRBinding }
};

static MenuItem profileOffsetsMenu[] = {
    { "Front Left",  MENU_SUBMENU, nullptr, profileOffsetsFrontLeftMenu,  sizeof(profileOffsetsFrontLeftMenu)/sizeof(MenuItem),  nullptr },
    { "Front Right", MENU_SUBMENU, nullptr, profileOffsetsFrontRightMenu, sizeof(profileOffsetsFrontRightMenu)/sizeof(MenuItem), nullptr },
    { "Rear Left",   MENU_SUBMENU, nullptr, profileOffsetsRearLeftMenu,   sizeof(profileOffsetsRearLeftMenu)/sizeof(MenuItem),   nullptr },
    { "Rear Right",  MENU_SUBMENU, nullptr, profileOffsetsRearRightMenu,  sizeof(profileOffsetsRearRightMenu)/sizeof(MenuItem),  nullptr }
};

// Tire-profile action callbacks (defined in section 6).
static void doNameProfile();
static void doResetProfile();

// No "Load" / "Save Prof" items (#18). The edit buffer now follows the Profile selector
// automatically via serviceProfileEditSync(), so there is nothing to load by hand; and the
// root menu's "Save Settings" already commits the edit buffer and writes all three slots
// (doSave -> commitEditToSelected + TireProfiles::save), so a profile-only save was a
// strict subset of it. "Reset" stays -- reverting one slot to its seed is distinct.
static MenuItem tireProfilesMenu[] = {
    { "Profile",   MENU_VALUE,  nullptr,        nullptr, 0, &profileSelectBinding     },
    { "Name",      MENU_ACTION, doNameProfile,  nullptr, 0, nullptr                   },
    { "Min",       MENU_VALUE,  nullptr,        nullptr, 0, &profileWindowMinBinding  },
    { "Ideal",     MENU_VALUE,  nullptr,        nullptr, 0, &profileWindowIdealBinding},
    { "Max",       MENU_VALUE,  nullptr,        nullptr, 0, &profileWindowMaxBinding  },
    // Display names only (#17) -- the fields stay offsetK / tauSeconds in code. "\xF8" is
    // the CP437 degree ring; tft.cp437(true) in setup() makes it render literally.
    { "Carcass Offset \xF8", MENU_VALUE, nullptr, nullptr, 0, &profileOffsetKBinding   },
    { "Carcass Lag s", MENU_VALUE,  nullptr,        nullptr, 0, &profileTauBinding     },
    { "Offsets",   MENU_SUBMENU, nullptr, profileOffsetsMenu, sizeof(profileOffsetsMenu)/sizeof(MenuItem), nullptr },
    { "Reset",     MENU_ACTION, doResetProfile, nullptr, 0, nullptr                   },
};

// ----------------------------------------------------
//  4) Save/Load Action Callbacks
// ----------------------------------------------------
static void doSave();
static void doLoad();

// ----------------------------------------------------
//  5) Main Menu Definition (with Save/Load items)
// ----------------------------------------------------
static MenuItem mainMenu[] = {
    {
      "Current Mode",
      MENU_VALUE,
      nullptr,
      nullptr,
      0,
      &currentModeBinding
    },
    {
      "Temp Scale",
      MENU_VALUE,
      nullptr,
      nullptr,
      0,
      &temperatureScaleBinding
    },
    {
      "Street Settings",
      MENU_SUBMENU,
      nullptr,
      streetSettingsMenu,
      sizeof(streetSettingsMenu)/sizeof(MenuItem),
      nullptr
    },
    {
      "Track Settings",
      MENU_SUBMENU,
      nullptr,
      trackSettingsMenu,
      sizeof(trackSettingsMenu)/sizeof(MenuItem),
      nullptr
    },
    {
        "Night Brightness",
        MENU_VALUE,
        nullptr,
        nullptr,
        0,
        &nightBrightnessBinding
    },    
    {
        "Test",
        MENU_VALUE,
        nullptr,
        nullptr,
        0,
        &testEnabledBinding
    },
    {
      "Hardware Settings",
      MENU_SUBMENU,
      nullptr,
      hardwareSettingsMenu,
      sizeof(hardwareSettingsMenu)/sizeof(MenuItem),
      nullptr
    },
    {
      "Camera Settings",
      MENU_SUBMENU,
      nullptr,
      pixelOffsetsMenu,
      sizeof(pixelOffsetsMenu)/sizeof(MenuItem),
      nullptr
    },
    {
      "IMU Gate",
      MENU_SUBMENU,
      nullptr,
      imuGateMenu,
      sizeof(imuGateMenu)/sizeof(MenuItem),
      nullptr
    },
    {
      "Tire Profiles",
      MENU_SUBMENU,
      nullptr,
      tireProfilesMenu,
      sizeof(tireProfilesMenu)/sizeof(MenuItem),
      nullptr
    },
    {
      "Save Config",
      MENU_ACTION,
      doSave,
      nullptr,
      0,
      nullptr
    }
};

// We create a global MenuSystem instance for all items
static MenuSystem tireMenuSystem(
    mainMenu,
    sizeof(mainMenu)/sizeof(MenuItem)
);

// ----------------------------------------------------
//  6) Implementation of the Save/Load callbacks
// ----------------------------------------------------
static void doSave()
{
    tireMenuSystem.saveToEEPROM();
    // Persist tire profiles alongside the menu settings so one "Save Config" covers
    // everything. Commit the on-screen edits into the selected slot first.
    TireProfiles::commitEditToSelected();
    TireProfiles::save();
    // Provide visual feedback
    menuRenderer.setStatusMessage("Settings Saved!");
    // Force a re-render if you want immediate update
    menuRenderer.render();
}

static void doLoad()
{
    tireMenuSystem.loadFromEEPROM();
    TireProfiles::begin();
    // The active slot is transient: resolve it from the mode we just loaded (#14).
    applyModeDefaultProfile();
}

// -- Mode -> default tire profile (#14) --
// The mode picks which profile is active; the profile is the single source of truth for
// the temp window. Runs at boot and on every mode change, so the window always matches
// the mode's default profile without anything being persisted per-boot.
void applyModeDefaultProfile()
{
    uint8_t sel = (currentMode == 1) ? trackProfile : streetProfile;
    if (sel >= PROFILE_COUNT) sel = 0;   // guard against a stale/garbage stored index
    g_profileSel = sel;
    // Keep the Tire Profiles edit buffer pointed at the slot we just adopted, so the
    // Min/Ideal/Max/K/tau items show the profile that is actually driving the display.
    TireProfiles::loadEditFromSelected();
}

// -- Profile selector -> edit buffer (#18) --
// MenuValueBinding carries no change-callback, so moving the "Profile" item only updates
// g_profileSel: the edit buffer would keep showing the PREVIOUS slot's values, and saving
// would then write them into the newly selected slot. That is what the manual "Load" item
// existed to work around. Watching the selection here closes the hole and lets both "Load"
// and "Save Prof" go away. Called from loop() alongside serviceModeProfileSnap(), so it
// also fires while the menu is open. Idempotent -- applyModeDefaultProfile() already
// resyncs on a mode change, and a second resync is a no-op.
void serviceProfileEditSync()
{
    static uint8_t lastSel = 0xFF;      // 0xFF forces a sync on the first call
    if (g_profileSel == lastSel) return;
    lastSel = g_profileSel;
    TireProfiles::loadEditFromSelected();
}

// -- Tire-profile actions (story 04) --
static void doNameProfile()
{
    // Hand off to the small-screen name editor (swipe up/down = letter, left = lock).
    menuRenderer.beginNameEdit(g_editProfile.name, PROFILE_NAME_LEN);
}

static void doResetProfile()
{
    TireProfiles::resetSelectedToDefault();
    menuRenderer.setStatusMessage("Profile reset");
}

// -- Balance summary action (story 05) --
static void doShowBalance()
{
    // Track-mode-only feature: inert in Street mode.
    if (currentMode != 1) {
        menuRenderer.setStatusMessage("Track mode only");
        return;
    }
    // Respect the show/hide setting.
    if (!showBalance) {
        menuRenderer.setStatusMessage("Balance hidden");
        return;
    }
    // Hand off to the full-screen readout; the touch loop re-renders it and any gesture
    // returns to the menu.
    menuRenderer.showBalance();
}

// -- Session summary recall action (story 01) --
static void doViewSummary()
{
    // Track-mode-only feature: inert in Street mode.
    if (currentMode != 1) {
        menuRenderer.setStatusMessage("Track mode only");
        return;
    }
    // Full-screen multi-page recall; the touch loop pages/dismisses it.
    menuRenderer.showSummary();
}

// ----------------------------------------------------
//  7) Provide global access to the Tire MenuSystem
// ----------------------------------------------------
MenuSystem& getTireMenuSystem()
{
    return tireMenuSystem;
}

// Extern getters for the values, so the main sketch can read them
uint8_t getCurrentModeValue() {
    return currentMode;
}

uint8_t getTemperatureScaleValue() {
    return temperatureScale;
}

uint8_t getNightBrightness() {
    return nightBrightness;
}

bool getUseThermalGradient() {
    return useThermalGradient;
}

bool getTestEnabled() {
    return testEnabled;
}

// Per-mode default profile (#14). The sketch does not need these -- it reads the resolved
// active profile -- but they keep the settings readable for logging/diagnostics.
uint8_t getStreetProfile() {
    return streetProfile;
}

uint8_t getTrackProfile() {
    return trackProfile;
}

uint8_t getCalcDisplayMode() {
    return calcDisplayMode;
}

bool getShowBalance() {
    return showBalance;
}

bool getAutoSealStationary() {
    return autoSealStationary;
}

bool getInflationIndicator() {
    return inflationIndicator;
}

bool getShowPixelOffsets() {
    return showPixelOffsets;
}

bool getHighFrequencyUpdates() {
    return highFrequencyUpdates;
}

bool getShowSegmentDeltas() {
    return showSegmentDeltas;
}

uint8_t getminInflationDeltaPct() {
    return minInflationDeltaPct;
}

uint8_t getminAlignmentDeltaPct() {
    return minAlignmentDeltaPct;
}

bool getImuGateEnabled() {
    return imuGateEnabled;
}

uint8_t getLateralGateCentiG() {
    return lateralGateCentiG;
}

uint8_t getAlertDwellTenths() {
    return alertDwellTenths;
}

uint8_t getImuOrient() {
    return imuOrient;
}

// Camera crop offsets, per corner (0=FL, 1=FR, 2=RL, 3=RR). Since #15 these resolve
// against the ACTIVE tire profile rather than eight globals, so switching profiles swaps
// the crop along with the window / K / tau. Signatures are unchanged on purpose: the
// consumers (tires_esp32.ino -> TempReader, ThermalDisplay, NBP boot metadata) still read
// them exactly as before, and initializeSystem() re-pushes them whenever the menu closes,
// which is also when a profile change takes effect.
byte getLeftPixelOffset(int index){
    if (index < 0 || index >= 4) return 0;
    return TireProfiles::active().leftOffset[index];
}

byte getRightPixelOffset(int index){
    if (index < 0 || index >= 4) return 0;
    return TireProfiles::active().rightOffset[index];
}
