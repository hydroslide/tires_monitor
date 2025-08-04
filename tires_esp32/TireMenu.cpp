#include "TireMenu.h"
#include "MenuRenderer.h"

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

// -- Street Settings --
static uint8_t streetMin   = 40;
static uint8_t streetIdeal = 120;
static uint8_t streetMax   = 160;

// -- Track Settings --
static uint8_t trackMin   = 100;
static uint8_t trackIdeal = 160;
static uint8_t trackMax   = 180;

// -- Hardware Temp Sensor Indices --
static uint8_t frontLeftTempIndex  = 0;
static uint8_t frontRightTempIndex = 0;
static uint8_t rearLeftTempIndex   = 0;
static uint8_t rearRightTempIndex  = 0;

// -- Camera Offsets --
static uint8_t leftOffsetFrontLeft = 0;
static uint8_t leftOffsetFrontRight = 0;
static uint8_t leftOffsetRearLeft = 0;
static uint8_t leftOffsetRearRight = 0;

static uint8_t rightOffsetFrontLeft = 0;
static uint8_t rightOffsetFrontRight = 0;
static uint8_t rightOffsetRearLeft = 0;
static uint8_t rightOffsetRearRight = 0;

static bool useThermalGradient = true;
static bool testEnabled = true;
static bool showPixelOffsets = true;


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

// Street
static MenuValueBinding streetMinBinding = {
    VALUE_BYTE,
    &streetMin,
    nullptr,
    0,
    255,
    8,
    nullptr,
    0
};

static MenuValueBinding streetIdealBinding = {
    VALUE_BYTE,
    &streetIdeal,
    nullptr,
    0,
    255,
    6,
    nullptr,
    0
};

static MenuValueBinding streetMaxBinding = {
    VALUE_BYTE,
    &streetMax,
    nullptr,
    0,
    255,
    10,
    nullptr,
    0
};

// Track
static MenuValueBinding trackMinBinding = {
    VALUE_BYTE,
    &trackMin,
    nullptr,
    0,
    255,
    14,
    nullptr,
    0
};

static MenuValueBinding trackIdealBinding = {
    VALUE_BYTE,
    &trackIdeal,
    nullptr,
    0,
    255,
    16,
    nullptr,
    0
};

static MenuValueBinding trackMaxBinding = {
    VALUE_BYTE,
    &trackMax,
    nullptr,
    0,
    255,
    18,
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

static MenuValueBinding leftOffsetFrontLeftBinding = {
    VALUE_BYTE,
    &leftOffsetFrontLeft,
    nullptr,
    0,
    16,
    31,
    nullptr,
    0
};

static MenuValueBinding leftOffsetFrontRightBinding = {
    VALUE_BYTE,
    &leftOffsetFrontRight,
    nullptr,
    0,
    16,
    32,
    nullptr,
    0
};

static MenuValueBinding leftOffsetRearLeftBinding = {
    VALUE_BYTE,
    &leftOffsetRearLeft,
    nullptr,
    0,
    16,
    33,
    nullptr,
    0
};

static MenuValueBinding leftOffsetRearRightBinding = {
    VALUE_BYTE,
    &leftOffsetRearRight,
    nullptr,
    0,
    16,
    34,
    nullptr,
    0
};

static MenuValueBinding rightOffsetFrontLeftBinding = {
    VALUE_BYTE,
    &rightOffsetFrontLeft,
    nullptr,
    0,
    16,
    35,
    nullptr,
    0
};

static MenuValueBinding rightOffsetFrontRightBinding = {
    VALUE_BYTE,
    &rightOffsetFrontRight,
    nullptr,
    0,
    16,
    36,
    nullptr,
    0
};

static MenuValueBinding rightOffsetRearLeftBinding = {
    VALUE_BYTE,
    &rightOffsetRearLeft,
    nullptr,
    0,
    16,
    37,
    nullptr,
    0
};

static MenuValueBinding rightOffsetRearRightBinding = {
    VALUE_BYTE,
    &rightOffsetRearRight,
    nullptr,
    0,
    16,
    38,
    nullptr,
    0
};

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


// ----------------------------------------------------
//  3) Submenu Item Arrays
// ----------------------------------------------------
static MenuItem streetSettingsMenu[] = {
    { "Min",   MENU_VALUE,  nullptr, nullptr, 0, &streetMinBinding   },
    { "Ideal", MENU_VALUE,  nullptr, nullptr, 0, &streetIdealBinding },
    { "Max",   MENU_VALUE,  nullptr, nullptr, 0, &streetMaxBinding   }
};

static MenuItem trackSettingsMenu[] = {
    { "Min",   MENU_VALUE,  nullptr, nullptr, 0, &trackMinBinding   },
    { "Ideal", MENU_VALUE,  nullptr, nullptr, 0, &trackIdealBinding },
    { "Max",   MENU_VALUE,  nullptr, nullptr, 0, &trackMaxBinding   }
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

static MenuItem frontLeftPixelOffsetsMenu[] = {
    { "Left",  MENU_VALUE, nullptr, nullptr, 0, &leftOffsetFrontLeftBinding},
    { "Right", MENU_VALUE, nullptr, nullptr, 0, &rightOffsetFrontLeftBinding }
};

static MenuItem frontRightPixelOffsetsMenu[] = {
    { "Left",  MENU_VALUE, nullptr, nullptr, 0, &leftOffsetFrontRightBinding},
    { "Right", MENU_VALUE, nullptr, nullptr, 0, &rightOffsetFrontRightBinding }
};

static MenuItem rearLeftPixelOffsetsMenu[] = {
    { "Left",  MENU_VALUE, nullptr, nullptr, 0, &leftOffsetRearLeftBinding},
    { "Right", MENU_VALUE, nullptr, nullptr, 0, &rightOffsetRearLeftBinding }
};

static MenuItem rearRightPixelOffsetsMenu[] = {
    { "Left",  MENU_VALUE, nullptr, nullptr, 0, &leftOffsetRearRightBinding},
    { "Right", MENU_VALUE, nullptr, nullptr, 0, &rightOffsetRearRightBinding }
};

static MenuItem pixelOffsetsMenu[] = {
    { "Front Left",  MENU_SUBMENU, nullptr, frontLeftPixelOffsetsMenu, sizeof(frontLeftPixelOffsetsMenu)/sizeof(MenuItem), nullptr},
    { "Front Right", MENU_SUBMENU, nullptr, frontRightPixelOffsetsMenu, sizeof(frontRightPixelOffsetsMenu)/sizeof(MenuItem), nullptr},
    { "Rear Left",   MENU_SUBMENU, nullptr, rearLeftPixelOffsetsMenu, sizeof(rearLeftPixelOffsetsMenu)/sizeof(MenuItem), nullptr},
    { "Rear Right",  MENU_SUBMENU, nullptr, rearRightPixelOffsetsMenu, sizeof(rearRightPixelOffsetsMenu)/sizeof(MenuItem), nullptr},
    {
        "Show Offsets",
        MENU_VALUE,
        nullptr,
        nullptr,
        0,
        &showPixelOffsetsBinding
    },
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
        "Thermal Gradient",
        MENU_VALUE,
        nullptr,
        nullptr,
        0,
        &useThermalGradientBinding
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
    // Provide visual feedback
    menuRenderer.setStatusMessage("Settings Saved!");
    // Force a re-render if you want immediate update
    menuRenderer.render();
}

static void doLoad()
{
    tireMenuSystem.loadFromEEPROM();
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

uint8_t getStreetMin() {
    return streetMin;
}


uint8_t getStreetIdeal() {
    return streetIdeal;
}

uint8_t getStreetMax() {
    return streetMax;
}

uint8_t getTrackMin() {
    return trackMin;
}

uint8_t getTrackIdeal() {
    return trackIdeal;
}

uint8_t getTrackMax() {
    return trackMax;
}

bool getShowPixelOffsets() {
    return showPixelOffsets;
}

byte getLeftPixelOffset(int index){
    switch(index){
        case 0:
            return leftOffsetFrontLeft;
            break;
        case 1:
            return leftOffsetFrontRight;
            break;
        case 2:
            return leftOffsetRearLeft;
            break;
        case 3:
            return leftOffsetRearRight;
            break;
        default:
            return 0;
    }
}

byte getRightPixelOffset(int index){
    switch(index){
        case 0:
            return rightOffsetFrontLeft;
            break;
        case 1:
            return rightOffsetFrontRight;
            break;
        case 2:
            return rightOffsetRearLeft;
            break;
        case 3:
            return rightOffsetRearRight;
            break;
        default:
            return 0;
    }
}
