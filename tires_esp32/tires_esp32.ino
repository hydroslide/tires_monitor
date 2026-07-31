#include <Adafruit_GFX.h>    // Core graphics library
#include <Adafruit_ST7789.h> // Hardware-specific library for ST7789
#include <SPI.h>
#include <Wire.h>
#include <EEPROM.h>

#include "Arduino_GFX_Library.h"
#include "pin_config.h"
#include "HWCDC.h"
#include "Tire.h"
#include "Wheels.h"
#include "TempReader.h"
#include "NBPProtocol.h"
#include "WifiSerial.h"
#include "IMUGate.h"
#include "ImuGateBar.h"
#include "TireProfiles.h"
#include "SessionManager.h"
#include "Version.h"

// #include "MyCST816Touch.h"
#include "CST816Touch_SWMode.h"


#include "TireMenu.h"           // Our custom menu structure
#include "MenuRenderer.h"       // Renders the TireMenu items
#include "TouchMenuHandler.h"   // Handles gestures to navigate the menu
#include "QuadrantFactory.h"
#include "ThermalDisplay.h"
#include "OffsetSetup.h"        // Interactive camera crop-offset mode (#23)
#include "DisplayBase.h"
#include "StandardDisplay.h"
#include "BufferedDisplay.h"
#include "DisplayProxy.h"

#define WIFI_SSID "TireTempMonitor"
#define WIFI_PASSWORD "esp32"
#define WIFI_PORT 8080
byte THERMAL_MODES =4;

bool testMode = false;
int forceDrawAfterInit = 0;
bool highFrequencyUpdates = false;
bool enableThermalTemps = false;

// Calculated (surface->carcass) mode K/tau come from the active tire profile (story 04),
// which since #14 is the single source of truth in both modes -- there is no longer a
// mode-level fallback pair here. The seed values live in TireProfiles::seedDefaults().

HWCDC USBSerial;
SPIClass hspi(HSPI);
Adafruit_ST7789 tft = Adafruit_ST7789(&hspi, LCD_CS, LCD_DC, LCD_RST);
StandardDisplay standardDisplay(tft);
BufferedDisplay bufferedDisplay(tft);
DisplayProxy displayProxy;
DisplayBase& display = displayProxy;

// WifiSerial instance
WifiSerial wifiSerial;
NBPProtocol nbp(wifiSerial);

// Existing objects
Wheels* wheels = nullptr;
TempReader* tempReader = nullptr;

// On-board QMI8658C IMU: lateral-g capture gate + latched over/under (story 02).
IMUGate imuGate;

// Session lifecycle + end-of-session summary (story 01). Owns start/seal, per-corner
// accumulation, the persisted summary, and the auto-seal backstop. Referenced extern by
// MenuRenderer (View Summary recall).
SessionManager sessionManager;

// Swipe-feedback + auto-summary display state (story 01). On start a red "recording"
// dot shows for 5 s; on end a black "stop" square shows for 5 s, then the summary is
// presented and paged with up/down swipes (Track mode only).
enum SwipeFeedback { FB_NONE, FB_START, FB_END };
static SwipeFeedback fbState = FB_NONE;
static unsigned long fbSetMs = 0;
static const unsigned long FB_MS = 5000;
static bool summaryAutoView = false;   // full-screen summary is showing (running mode)
static int  summaryAutoPage = 0;
static bool summaryAutoDirty = false;  // needs a (re)paint

// Debounce for the session-toggle swipe (Track mode). The touch library often reports a
// single physical swipe twice, so starting a session would immediately end it; and the
// swipe that dismisses the summary can bleed into starting a fresh one. After any
// session start/end -- and after dismissing the summary -- we ignore the toggle swipe for
// SWIPE_LOCK_MS so the second registration is dropped.
static unsigned long swipeLockSetMs = 0;
static const unsigned long SWIPE_LOCK_MS = 1000;
static void armSwipeLock(){ swipeLockSetMs = millis(); }
static bool sessionSwipeLocked(){ return (millis() - swipeLockSetMs) < SWIPE_LOCK_MS; }

// ... after initializing tft in setup() ...
QuadrantFactory factory(display, /*margin=*/ 5);

// // To create the upper-left ThermalDisplay:
ThermalDisplay* UL = factory.createDisplay(/*top=*/ true, /*left=*/ true);
ThermalDisplay* UR = factory.createDisplay(/*top=*/ true, /*left=*/ false);
ThermalDisplay* LL = factory.createDisplay(/*top=*/ false, /*left=*/ true);
ThermalDisplay* LR = factory.createDisplay(/*top=*/ false, /*left=*/ false);


ThermalDisplay* thermalDisplays[4] = {UL, UR, LL, LR};


uint8_t thermalMode = 0; // 4 modes total

// Keep track of time
unsigned long previousTime = 0;
long millisSinceLastUpdate = 0;
long updateIntervalMillis = 1000;
long readIntervalMillis = 100;
long millisSinceLastRead = 0;

// Time helper
long timeDelta()
{
  unsigned long currentTime = millis();
  long delta = (long)(currentTime - previousTime);
  previousTime = currentTime;
  return delta;
}



  // 1) Create a CST816Touch object (from mjdonders/CST816_TouchLib)
  CST816Touch_SWMode cstTouch;

  // 2) Retrieve Tire Menu system
  MenuSystem &menuSystem = getTireMenuSystem();

  // 3) Create MenuRenderer
  MenuRenderer menuRenderer(menuSystem, display);
  //MenuRenderer menuRenderer(menuSystem, standardDisplay)

  // 4) Create TouchMenuHandler
  TouchMenuHandler menuHandler(menuSystem, menuRenderer, cstTouch);


// Forward declarations
static void applyMenuConfig();
static void cleanupObjects();
static void initializeSystem();
static void sendBootMetadata();
static void exitSummaryAutoView();
static void toggleSession();
static void serviceModeProfileSnap();
static void applyThermalActivation();
static void beginOffsetSetup();
static void endOffsetSetup();
static void checkOffsetSetupSwipes();
static void doOffsetSetupMode(int time_delta);
extern uint8_t getCurrentModeValue();
extern bool getAutoSealStationary();

// Last Current Mode seen by the profile snap (#14). Seeded in setup() right after the
// boot resolve so the watcher in loop() only fires on an actual mode change.
static uint8_t lastModeForProfile = 0;

void checkForWheelsReset(){
  if (tempReader->tireSensorIsCamera[0] != wheels->fl3 ||
      tempReader->tireSensorIsCamera[1] != wheels->fr3 ||
      tempReader->tireSensorIsCamera[2] != wheels->rl3 ||
      tempReader->tireSensorIsCamera[3] != wheels->rr3){
        USBSerial.println("Found a different sensor. resetting Wheels");
        Wheels* oldWheels = wheels;
        USBSerial.println("oldWheels Point Created");
        USBSerial.print("Free heap before alloc: ");
        USBSerial.println(ESP.getFreeHeap());
        wheels = new Wheels(oldWheels,
          tempReader->tireSensorIsCamera[0],
          tempReader->tireSensorIsCamera[1],
          tempReader->tireSensorIsCamera[2],
          tempReader->tireSensorIsCamera[3]);
        USBSerial.println("new Wheels object initialized");
        delete oldWheels;
        USBSerial.println("oldWheels deleted");
        oldWheels = nullptr;
        USBSerial.println("oldWheels pointer set to null");
        activateTires();
      }
}

void switchThermalMode(bool up){
  int dir = (up)?1:-1;
  int tMode = (thermalMode + dir + THERMAL_MODES) % THERMAL_MODES;
  setThermalMode(tMode);
}


   

void updateThermalDisplays(){
  if (enableThermalTemps){
    if (thermalMode ==0){
      // Do Nothing
    }else{
      thermalDisplays[0]->updateDisplay(0);
      thermalDisplays[1]->updateDisplay(1);
      thermalDisplays[2]->updateDisplay(2);
      thermalDisplays[3]->updateDisplay(3);
    }
  }
  else{
    switch (thermalMode)
    {
    case 0:
      // Do Nothing
      break;

    case 1:
      thermalDisplays[2]->updateDisplay(0);
      thermalDisplays[3]->updateDisplay(1);
      break;
    
    case 2:
      thermalDisplays[0]->updateDisplay(2);
      thermalDisplays[1]->updateDisplay(3);
      break;
    
    case 3:
      thermalDisplays[0]->updateDisplay(0);
      thermalDisplays[1]->updateDisplay(1);
      thermalDisplays[2]->updateDisplay(2);
      thermalDisplays[3]->updateDisplay(3);
      break;
    
    default:
      break;
    }
  }
}    

// Which quadrants show a camera image for the current thermalMode. Split out of
// setThermalMode() (#23) because the offset-setup mode has to force all four on, and put
// them back on exit, WITHOUT the repaint below -- the menu is what takes the screen next, so
// painting the running display first would only flash it.
static void applyThermalActivation(){
  if (enableThermalTemps){
    // Test mode drives all four from one frame set: it is all of them or none.
    const bool on = (thermalMode != 0);
    UL->isActive=on;
    UR->isActive=on;
    LL->isActive=on;
    LR->isActive=on;
  }else{
    // 0 = none, 1 = rears, 2 = fronts, 3 = all four. switchThermalMode() keeps thermalMode
    // inside that range with its modulo, so there is no other case to answer for.
    UL->isActive = (thermalMode == 2 || thermalMode == 3);
    UR->isActive = (thermalMode == 2 || thermalMode == 3);
    LL->isActive = (thermalMode == 1 || thermalMode == 3);
    LR->isActive = (thermalMode == 1 || thermalMode == 3);
  }
}

void setThermalMode(uint8_t _thermalMode){
  thermalMode = _thermalMode;
  applyThermalActivation();
  display.fillScreen(ST77XX_BLACK);
  imuGateBarInvalidate();
  activateTires();
  if (!(enableThermalTemps && thermalMode ==2))
    wheels->draw(true);
}

// Over/under source for the latch (story 02, refined by story 06). Computes the
// overall center-hot vote across the camera tires from the working section temps --
// which are the CALCULATED (offset + smoothed) values when calculated mode is on
// (calculated-everywhere). The IMU gate applies this vote only on captured
// (straight-line) frames, which is what strips the mid-corner body-roll artifact.
// The per-corner baseline subtraction was removed in #18 -- see TireProfiles.h for why
// (the shipped values encoded one track's load pattern, and the static part of the error
// is a camera aim problem belonging in the pixel crop offsets).
// #21 keeps the per-corner verdicts instead of collapsing them to one majority vote. The
// old shape threw away exactly the information the feature exists to deliver -- WHICH tire
// is wrong -- and let one clearly-over corner be cancelled by its neighbours' neutrals.
// This is now the ONLY place the inflation comparison is computed; the display and the
// instrumentation stream both consume the latched result rather than re-deriving it.
extern byte getminInflationDeltaPct();
static void computeInflationVotes(int8_t out[TIRE_COUNT])
{
  float pct = getminInflationDeltaPct() / 100.0f;
  for (int t = 0; t < TIRE_COUNT; t++) {
    out[t] = 0;
    if (!tempReader->tireSensorIsCamera[t]) continue;
    float edge = (tempReader->tireSectionTemps[t][0] +
                  tempReader->tireSectionTemps[t][2]) / 2.0f;
    float center = tempReader->tireSectionTemps[t][1];
    if (edge <= 0.0f) continue;                  // skip unread/invalid frames
    float delta = edge - center;                 // negative => center hotter
    float minDelta = edge * pct;
    if (delta <= -minDelta) out[t] = 1;          // center-hot => over-inflation
    else if (delta >= minDelta) out[t] = -1;     // edges-hot => under-inflation
  }
}

// Normal Running Mode
void doRunningMode(int time_delta)
{
  millisSinceLastUpdate += time_delta;
  millisSinceLastRead += time_delta;
  if (millisSinceLastRead >= readIntervalMillis)
  {
    long readDelta = millisSinceLastRead;
    millisSinceLastRead = 0;

    // Read tire temps
    tempReader->readTemps();

    // Story 03: fold the raw surface medians into the calculated (EMA_tau + K) working
    // value on the read cadence. When calculated mode is active this replaces the
    // working temps that feed every display and decision below; the raw surface is kept
    // in rawSectionTemps for a separate diagnostic channel set. Runs after readTemps so
    // the raw validity filter is untouched.
    tempReader->updateCalculated(readDelta);

    // Advance the IMU capture gate + latch on the read cadence (feature acts only
    // in Track mode). Feed the interim inflation verdict and log the calibrated
    // accel/gyro over NBP. Kept off the 1 Hz display path so loop timing is intact.
    bool trackMode = (getCurrentModeValue() == 1);
    imuGate.update(readDelta, trackMode);
    // Feed each corner's own verdict. Track-mode only -- in Street the gate is inert and
    // zeroes the scores anyway. #21 dropped the separate "Inflation" toggle that used to
    // gate this: now that the latch drives the on-screen segment colors, switching it off
    // left those bars with no inflation verdict but a live alignment one, which is a
    // confusing half-state. "Segment Deltas" controls whether any of it is painted.
    int8_t inflVotes[TIRE_COUNT] = {0, 0, 0, 0};
    if (trackMode) computeInflationVotes(inflVotes);
    for (int t = 0; t < TIRE_COUNT; t++) imuGate.feedCondition(t, inflVotes[t]);
    if (!testMode && imuGate.isPresent()) {
      nbp.sendIMU(imuGate.accelG(0), imuGate.accelG(1), imuGate.accelG(2),
                  imuGate.gyroDps(0), imuGate.gyroDps(1), imuGate.gyroDps(2),
                  imuGate.lateralG());
    }

    // Session accumulation (story 01): fold this frame's calculated working temps into
    // the running per-corner stats. Track-mode only; once sealed, running is false so
    // post-seal frames are excluded. Uses the whole-tire working average per corner
    // (calculated value when calculated mode is on), matching the balance readout.
    if (trackMode && sessionManager.isRunning()) {
      float sTemps[TIRE_COUNT];
      bool  sValid[TIRE_COUNT];
      for (int t = 0; t < TIRE_COUNT; t++) {
        float v = tempReader->tireSensorIsCamera[t]
                    ? (tempReader->tireSectionTemps[t][0] +
                       tempReader->tireSectionTemps[t][1] +
                       tempReader->tireSectionTemps[t][2]) / 3.0f
                    : tempReader->tireTemps[t];
        sTemps[t] = v;
        sValid[t] = (v > 0.0f);
      }
      sessionManager.accumulate(readDelta, sTemps, sValid);

      // Track per-corner inflation on-time over captured (straight-line) frames, so the
      // summary can surface each tire's verdict when it held for >= 50% of that session's
      // captured time. Latched per-tire alert -> signed verdict per tire (#21).
      {
        int8_t av[TIRE_COUNT];
        for (int t = 0; t < TIRE_COUNT; t++) {
          IMUGate::Alert a = imuGate.alertState(t);
          av[t] = (a == IMUGate::ALERT_OVER) ? 1 : (a == IMUGate::ALERT_UNDER) ? -1 : 0;
        }
        sessionManager.accumulateInflation(readDelta, imuGate.isCapturing(), av);
      }

      // Auto-seal backstop: seal after sustained IMU stillness (near-zero horizontal
      // accel + gyro). Best-effort -- there is no speed channel -- and off by default.
      bool still = imuGate.isPresent() &&
                   fabsf(imuGate.accelG(0)) < 0.03f && fabsf(imuGate.accelG(1)) < 0.03f &&
                   fabsf(imuGate.gyroDps(0)) < 3.0f && fabsf(imuGate.gyroDps(1)) < 3.0f &&
                   fabsf(imuGate.gyroDps(2)) < 3.0f;
      if (sessionManager.pollAutoSeal(readDelta, getAutoSealStationary(), still)) {
        if (!testMode) nbp.sendSessionSummary(sessionManager.summary());
        fbState = FB_END; fbSetMs = millis();
      }
    }

    updateThermalDisplays();

    if (millisSinceLastUpdate >= updateIntervalMillis || highFrequencyUpdates){
      millisSinceLastUpdate=0;
      checkForWheelsReset();

      Wheels::TireTemps fl;           
      Wheels::TireTemps fr; 
      Wheels::TireTemps rl;
      Wheels::TireTemps rr;

      if (testMode){
          //wheels->setTireTemps(25,55,120,200);
        fl =  Wheels::TireTemps( 25 );            // single‐value
        fr = Wheels::TireTemps(55); // three‐value
        rl = Wheels::TireTemps( 120);
        rr = Wheels::TireTemps(200);         
        }
        else {
          fl = (tempReader->tireSensorIsCamera[0])? Wheels::TireTemps( tempReader->tireSectionTemps[0] ):  Wheels::TireTemps( tempReader->tireTemps[0]);  // three‐value      
          fr = (tempReader->tireSensorIsCamera[1])? Wheels::TireTemps( tempReader->tireSectionTemps[1] ):  Wheels::TireTemps( tempReader->tireTemps[1]);    // single‐value
          rl = (tempReader->tireSensorIsCamera[2])? Wheels::TireTemps( tempReader->tireSectionTemps[2] ):  Wheels::TireTemps( tempReader->tireTemps[2]);
          rr = (tempReader->tireSensorIsCamera[3])? Wheels::TireTemps( tempReader->tireSectionTemps[3] ):  Wheels::TireTemps( tempReader->tireTemps[3]); 
        }

      if (!testMode) {
        // Active value (calculated when enabled) under the original channel labels.
        nbp.setAllTireTemps(fl, fr, rl, rr, (wheels->getTempUnit() == 'F'));

        // Story 03: always keep the raw surface signal as its own channel set while the
        // feature is in play (Track mode), so K/tau can be re-derived offline even when
        // calculated mode is driving the original labels.
        if (trackMode) {
          bool farenheit = (wheels->getTempUnit() == 'F');
          Wheels::TireTemps flRaw = (tempReader->tireSensorIsCamera[0]) ? Wheels::TireTemps(tempReader->rawSectionTemps[0]) : Wheels::TireTemps(tempReader->rawSectionTemps[0][0]);
          Wheels::TireTemps frRaw = (tempReader->tireSensorIsCamera[1]) ? Wheels::TireTemps(tempReader->rawSectionTemps[1]) : Wheels::TireTemps(tempReader->rawSectionTemps[1][0]);
          Wheels::TireTemps rlRaw = (tempReader->tireSensorIsCamera[2]) ? Wheels::TireTemps(tempReader->rawSectionTemps[2]) : Wheels::TireTemps(tempReader->rawSectionTemps[2][0]);
          Wheels::TireTemps rrRaw = (tempReader->tireSensorIsCamera[3]) ? Wheels::TireTemps(tempReader->rawSectionTemps[3]) : Wheels::TireTemps(tempReader->rawSectionTemps[3][0]);
          nbp.setRawTireTemps(flRaw, frRaw, rlRaw, rrRaw, farenheit);
        }
      }

      wheels->setTireTemps(fl, fr, rl, rr);

      // Hand the display the LATCHED per-corner verdict plus the raw evidence score, so
      // the segment delta bars paint the gated, dwelled answer instead of re-deriving an
      // instantaneous one of their own (#21). This is the change that stops those bars
      // flickering mid-corner: the tire no longer gets a vote while the car is loaded up.
      for (int c = 0; c < TIRE_COUNT; c++) {
        IMUGate::Alert a = imuGate.alertState(c);
        wheels->setInflationVerdict(c, (a == IMUGate::ALERT_OVER) ? 1
                                     : (a == IMUGate::ALERT_UNDER) ? -1 : 0);
        wheels->setDwellProgress(c, imuGate.inflScore(c),
                                 imuGate.inflScoreLatch(), imuGate.inflScoreMax());
      }

      if (forceDrawAfterInit>0){
          display.fillScreen(ST77XX_BLACK);
          wheels->draw(true);
          forceDrawAfterInit--;
      }else if (enableThermalTemps && thermalMode == 2)
        wheels->draw(true, true);
      else
        wheels->draw();

      // The tire map just repainted. ThreeSectionTire clears an area inflated by
      // bufferPix on every side (ThreeSectionTire.cpp:121), which covers the whole gutter
      // the g bar lives in -- so assume the bar was wiped and rebuild it next pass. This
      // is the common case; a band only has to cross a color threshold to trigger it.
      imuGateBarInvalidate();

      // Story 08 (#9): emit the device's own verdict + per-segment colors so the
      // downstream renderer applies them directly with no re-derivation. Track-mode
      // only -- the raw/active temp channel sets above stay on in all modes. Runs after
      // draw() so the per-band colors are exactly the ones just painted. Delta is the
      // plain edge-vs-center spread (#18 dropped the baseline correction). Delta and
      // Threshold stay RAW here on purpose -- they are the un-gated diagnostic signal, and
      // logging them lets the gate's effect be re-derived offline. The per-corner Verdict
      // is now the LATCHED value (#21) rather than a third re-implementation of the
      // comparison; Overall is the majority of those latches.
      if (!testMode && trackMode) {
        float   insDelta[TIRE_COUNT]  = {0};
        float   insThresh[TIRE_COUNT] = {0};
        int8_t  insVerdict[TIRE_COUNT] = {0};
        bool    insCam[TIRE_COUNT];
        uint16_t fillCols[TIRE_COUNT][3]  = {{0}};
        uint16_t deltaCols[TIRE_COUNT][3] = {{0}};
        float pct = getminInflationDeltaPct() / 100.0f;
        for (int c = 0; c < TIRE_COUNT; c++) {
          insCam[c] = tempReader->tireSensorIsCamera[c];
          if (!insCam[c]) continue;
          float edge = (tempReader->tireSectionTemps[c][0] +
                        tempReader->tireSectionTemps[c][2]) / 2.0f;
          float center = tempReader->tireSectionTemps[c][1];
          insDelta[c]  = edge - center;
          insThresh[c] = edge * pct;
          IMUGate::Alert a = imuGate.alertState(c);
          insVerdict[c] = (a == IMUGate::ALERT_OVER) ? 1
                        : (a == IMUGate::ALERT_UNDER) ? -1 : 0;
          wheels->cornerColors(c, fillCols[c], deltaCols[c]);
        }
        int8_t overall = (imuGate.alertState() == IMUGate::ALERT_OVER)  ?  1
                       : (imuGate.alertState() == IMUGate::ALERT_UNDER) ? -1 : 0;
        nbp.sendInstrumentation(insDelta, insThresh, insVerdict, overall,
                                insCam, fillCols, deltaCols,
                                (wheels->getTempUnit() == 'F'));
      }

          // WifiSerial
      wifiSerial.loop();
    }

    // Overlays last, so they sit on top of the tire map and the camera quadrants, and --
    // critically -- so they are in the canvas before the flush below. Moved here out of
    // loop() (#28); see the note at that call site. Both read state that only changes on
    // this read cadence, so sampling them at 10 Hz rather than loop rate costs nothing:
    // the g bar follows imuGate, and the badge is a 500 ms blink.
    drawSessionFeedback();
    drawImuGateBar(display);

    // F5: the running display's frame is complete. 10 Hz, unchanged from March.
    // Flush buffered display to screen (no-op for StandardDisplay)
    display.drawScreen();
  }


}

bool nightMode=false;
int nightBrightness=12;

void setup()
{
  EEPROM.begin(EEPROM_SIZE);

  USBSerial.begin(9600);
  USBSerial.println("Top of ESP32 Tires Setup");

  pinMode(LCD_BL, OUTPUT);
  digitalWrite(LCD_BL, HIGH);

  // Start WiFi
  if (!wifiSerial.begin(WIFI_SSID, WIFI_PASSWORD, WIFI_PORT)) {
    USBSerial.println("Failed to start WiFi!");
    while (true);
  }
  USBSerial.println("WiFi initialized.");

  nbp.sendMetadata("NAME", "Tire Temp Reader");
  nbp.sendMetadata("VERSION", "0.1");

  // Set default display implementation
  displayProxy.setImplementation(&standardDisplay);

  // SPI + TFT
  hspi.begin(LCD_SCK, -1, LCD_MOSI, LCD_CS);
  //tft.setSPISpeed(80000000);
  // Slow down SPI to 40MHz (more stable than 80MHz)
  display.setSPISpeed(40000000);
  display.init(240, 280, SPI_MODE0);
  // Use the real CP437 charset (#17). Adafruit_GFX otherwise applies a legacy shift --
  // `if (!_cp437 && (c >= 176)) c++` -- which would silently render the degree ring
  // (0xF8) as the stray dot at 0xF9. Only affects chars >= 176; all our strings are
  // ASCII, so nothing else changes. MenuRenderer references this same display.
  //
  // Re-asserted in initializeSystem() after the implementation is chosen: this call lands
  // on whichever impl is active NOW (StandardDisplay), and the buffered canvas does not
  // exist yet, so it would otherwise never receive the flag.
  display.cp437(true);
  display.setRotation(3);
  display.fillScreen(ST77XX_BLACK);

  // I2C for touch
  Wire.setPins(IIC_SDA, IIC_SCL);
  Wire.begin();

  // Initialize the CST816Touch library
  if (!cstTouch.begin(Wire, TP_INT, TP_RST)) {
    USBSerial.println("Failed to init CST816Touch!");
    while(true){
      delay(100);
    }
         
  } else {
    cstTouch.setSwapXY(true);	
	  cstTouch.enableGestureFactory(240, 280);	
    //cstTouch.enableDoubleClickFactory_Elegant();
    cstTouch.enableDoubleClickFactory_Quick();
    USBSerial.println("CST816Touch initialized.");
  }

  // Load config from EEPROM
  menuSystem.loadFromEEPROM();
  USBSerial.println("EEPROM values loaded");

  // Load tire profiles (story 04) or seed defaults. Runs after the menu load so the
  // per-mode default-profile settings are already in hand.
  TireProfiles::begin();

  // Resolve the active profile from the current mode's default (#14). The active slot is
  // never persisted, so this -- not EEPROM -- is what decides the window at boot. Must
  // run before initializeSystem() builds the display from the profile.
  applyModeDefaultProfile();
  lastModeForProfile = getCurrentModeValue();

  // Recall the last persisted session summary (story 01) so View Summary works after a
  // reboot. Its EEPROM region sits above the profiles; the menu load never touches it.
  sessionManager.begin();

  // Bring up the on-board IMU and auto-calibrate orientation at rest. The car
  // should be stationary/level at boot; a manual axis override is the fallback.
  if (imuGate.begin(Wire))
    USBSerial.println("QMI8658C IMU initialized + orientation calibrated");
  else
    USBSerial.println("QMI8658C IMU not detected; capture gate inert");

  UL->isActive=false;
  UR->isActive=false;
  LL->isActive=false;
  LR->isActive=false;

  // Initialize system objects
  initializeSystem();

  // Story 08 (#9): emit the self-describing boot metadata once, after the active config
  // (profile / window / K / tau / crop offsets) is resolved, so a log is interpretable
  // months later without external notes.
  if (!testMode) sendBootMetadata();

  // F2: the boot paint happens here, outside loop(). loop() then returns early for its
  // first 2 seconds (the firstRun USB grace period), so without this flush the buffered
  // path would sit black for 2 s while the direct path came up instantly -- which reads
  // as a bug when comparing the two.
  display.drawScreen();

  USBSerial.println("Bottom of ESP32 Tires Setup");
}

// Start or end a session (story 01). End seals immediately and emits the summary over
// NBP; start/end both raise the on-screen swipe feedback. Track-mode only (the caller
// gates this), reading the window/unit from the active display config.
static void toggleSession(){
  if (sessionManager.isRunning()){
    sessionManager.end();
    if (!testMode) nbp.sendSessionSummary(sessionManager.summary());
    fbState = FB_END; fbSetMs = millis();
  } else {
    sessionManager.start(wheels->getTempUnit(),
                         wheels->minTemp, wheels->idealTemp, wheels->maxTemp);
    fbState = FB_START; fbSetMs = millis();
  }
  // Lock out the toggle swipe so a double-registered swipe can't immediately reverse this.
  armSwipeLock();
}

// -- Interactive camera crop-offset setup (#23) --
// A third thing that can own the screen, alongside the running display and the auto summary.
// While it is up the sketch runs a stripped read/paint loop of its own: the four camera
// images and nothing else. No tire map (there is no room, and the numbers are not what you
// are looking at), no IMU gate, no session accumulation, no NBP -- none of that is measuring
// anything while the car sits still and you aim cameras at it.
static bool offsetSetupActive  = false;
static bool offsetSetupPending = false;   // picked in the menu, entered once the menu closes

static void beginOffsetSetup(){
  // No need to remember thermalMode: nothing can change it while this mode owns the screen
  // (only checkForSwipes() does, and it is not running), so applyThermalActivation() can
  // simply re-derive the quadrant flags from it on the way out.

  // All four quadrants live, whatever the running display was showing. updateDisplay() still
  // no-ops on a corner with no camera, and OffsetSetup leaves those corners out of the walk.
  for (int i = 0; i < TIRE_COUNT; i++) thermalDisplays[i]->isActive = true;

  // The left swipe is half of the nudge pair here, so it must stop meaning "open the menu".
  menuHandler.suspendMenu(true);

  display.fillScreen(ST77XX_BLACK);
  imuGateBarInvalidate();

  // Edit the ACTIVE profile -- the same slot the numeric Offsets fields target -- so the
  // images on screen are being cropped by the values under edit.
  OffsetSetup::begin(tempReader, TireProfiles::activeIndex());
  offsetSetupActive = true;

  // Paint on the very next pass instead of waiting out a read interval on a black screen.
  millisSinceLastRead = readIntervalMillis;
}

static void endOffsetSetup(){
  offsetSetupActive = false;
  menuHandler.suspendMenu(false);

  // Put the quadrant activation back the way the running display had it. No repaint: the
  // menu is about to take the whole screen, and when it closes applyMenuConfig() rebuilds
  // the display from scratch anyway.
  applyThermalActivation();
  activateTires();

  // Back to Tire Profiles -> Offsets, the screen we were launched from. MenuSystem never
  // moved, so re-opening lands on the same item with no bookkeeping.
  menuHandler.openMenu();
}

// Route the four directions into the mode. The touch library's names are NOT the screen's:
// GESTURE_LEFT is the menu's select/descend swipe, and here it slides the armed guide LEFT.
// OffsetSetup takes it from there -- including which sign that means for the byte, which
// differs between the two guides (see the sign rule in OffsetSetup.cpp).
static void checkOffsetSetupSwipes(){
  if (menuHandler.SwipedLeft())  OffsetSetup::handleSwipe(OffsetSetup::NUDGE_LEFT);
  if (menuHandler.SwipedRight()) OffsetSetup::handleSwipe(OffsetSetup::NUDGE_RIGHT);
  if (menuHandler.SwipedUp())    OffsetSetup::handleSwipe(OffsetSetup::PREV);
  if (menuHandler.SwipedDown())  OffsetSetup::handleSwipe(OffsetSetup::NEXT);

  // A confirm was answered (or there was nothing to edit): tear the mode down.
  if (!OffsetSetup::isActive()) endOffsetSetup();
}

static void doOffsetSetupMode(int time_delta){
  // Advance the blink phase first so the guides drawn below are in step with the border.
  OffsetSetup::service(display);

  millisSinceLastRead += time_delta;
  if (millisSinceLastRead < readIntervalMillis) return;
  millisSinceLastRead = 0;

  tempReader->readTemps();
  for (int i = 0; i < TIRE_COUNT; i++) thermalDisplays[i]->updateDisplay(i);

  // F3: this mode owns the screen and did not exist in March, so without its own flush
  // the buffered path would show whatever was on the glass when it was entered.
  display.drawScreen();
}

// Leave the full-screen summary and restore the running display.
static void exitSummaryAutoView(){
  summaryAutoView = false;
  display.fillScreen(ST77XX_BLACK);
  imuGateBarInvalidate();
  activateTires();
  wheels->draw(true);
  forceDrawAfterInit = 2;
}

void checkForSwipes(){
  bool sr = menuHandler.SwipedRight();
  bool su = menuHandler.SwipedUp();
  bool sd = menuHandler.SwipedDown();
  bool track = (getCurrentModeValue() == 1);

  // While the auto summary is showing, gestures page/dismiss it and nothing else --
  // the swipe must not also toggle a session or Night Mode here.
  if (summaryAutoView){
    if (su && summaryAutoPage < SessionManager::PAGE_COUNT - 1){ summaryAutoPage++; summaryAutoDirty = true; }
    else if (sd && summaryAutoPage > 0){ summaryAutoPage--; summaryAutoDirty = true; }
    else if (sr){
      exitSummaryAutoView();
      // A double-registered dismiss swipe would otherwise fall through next loop and start
      // a new session -- hold the toggle lock across the hand-off back to the running view.
      armSwipeLock();
    }
    return;
  }

  if (track){
    // Track mode: the left/right swipe toggles the SESSION (not Night Mode). Up/down
    // still switch the thermal display mode. The toggle is debounced (see SWIPE_LOCK_MS).
    if (sr && !sessionSwipeLocked()) toggleSession();
    if (su) switchThermalMode(true);
    if (sd) switchThermalMode(false);
  } else {
    // Street mode: unchanged -- the swipe toggles Night Mode and never a session.
    if (sr) ToggleNightMode();
    if (su) switchThermalMode(true);
    if (sd) switchThermalMode(false);
  }
}

// Advance the swipe-feedback timers (story 01). The red dot / black square each blink for
// FB_MS; when the end square expires the summary takes over the screen.
static void serviceFeedback(){
  if (fbState == FB_NONE) return;
  unsigned long dt = millis() - fbSetMs;
  if (fbState == FB_START){
    if (dt >= FB_MS){
      fbState = FB_NONE;              // clear the recording dot
      display.fillScreen(ST77XX_BLACK);
      imuGateBarInvalidate();
      activateTires();
      wheels->draw(true);
      forceDrawAfterInit = 2;
    }
  } else { // FB_END
    if (dt >= FB_MS){
      fbState = FB_NONE;
      summaryAutoView = true;         // present the sealed summary
      summaryAutoPage = 0;
      summaryAutoDirty = true;
    }
  }
}

// Upper-right swipe-feedback indicator geometry. The panel has rounded corners, so the
// badge is inset FB_INSET from both edges -- hard against the corner it gets clipped and
// is effectively invisible. Both shapes share the same center so start/end land in the
// same spot.
static const int FB_R      = 12;                     // recording-dot radius
static const int FB_INSET  = 14;                     // clearance from the rounded corner
static const int FB_CX     = 280 - FB_INSET - FB_R;  // 254
static const int FB_CY     = FB_INSET + FB_R;        // 26

// Blink cadence: 500 ms lit, 500 ms dark, for as long as the badge is up (FB_MS / 5 s =>
// five full cycles). A blinking badge reads as "live" at a glance where a static one can
// be mistaken for a painted-on artifact of the tire map.
static const unsigned long FB_BLINK_MS = 500;

// Paint the upper-right swipe-feedback indicator over the running display. Called every
// loop iteration -- deliberately, not just on the blink edge: the tire map repaints on the
// (slower) read cadence and would otherwise wipe the badge mid-phase, so each pass
// re-asserts whichever half of the blink is current.
static void drawSessionFeedback(){
  if (fbState == FB_NONE) return;

  const int s = FB_R * 2;                            // the stop square's footprint
  const int x = FB_CX - FB_R, y = FB_CY - FB_R;

  // "Dark" half of the current blink cycle. Each state clears exactly the footprint of the
  // shape it draws -- NOT a shared square -- so only the one badge pulses. Clearing a full
  // square under the round recording dot would flicker the tire-map corners black in step
  // with the dot, which reads as a circle AND a square blinking together.
  const bool dark = (((millis() - fbSetMs) / FB_BLINK_MS) & 1UL) != 0;

  if (fbState == FB_START){
    // Recording dot: the circle itself blinks red -> black -> red within its own footprint.
    display.fillCircle(FB_CX, FB_CY, FB_R, dark ? ST77XX_BLACK : ST77XX_RED);
  } else { // FB_END
    // Stop badge: a steady black square whose 2 px white outline pulses on/off, so it reads
    // on black at a glance without the fill flickering.
    display.fillRect(x, y, s, s, ST77XX_BLACK);
    if (!dark){
      display.drawRect(x,     y,     s,     s,     ST77XX_WHITE);
      display.drawRect(x + 1, y + 1, s - 2, s - 2, ST77XX_WHITE);
    }
  }
}

// #21 removed drawInflationIndicator() -- the top-left OVER/UNDER badge. It reported a
// single global verdict, which is exactly the thing that made the feature unhelpful: it
// could tell you something was wrong but never which corner. The per-tire segment delta
// bars now carry the latched verdict on the tire it belongs to, so the badge was strictly
// less information occluding the front-left tile.

void ToggleNightMode(){
    nightMode=!nightMode;
    int bright = (nightMode)?nightBrightness:255;
    analogWrite(LCD_BL, bright);
}

// Snap the active tire profile whenever Current Mode changes (#14). Polled every loop --
// including while the menu is open -- so walking Current Mode -> Track and then into Tire
// Profiles already shows Track's default profile. A manual profile pick afterwards sticks,
// because the mode has not changed again. The window itself is rebuilt when the menu
// closes (applyMenuConfig -> initializeSystem), as with every other setting.
static void serviceModeProfileSnap(){
  uint8_t mode = getCurrentModeValue();
  if (mode == lastModeForProfile) return;
  lastModeForProfile = mode;
  applyModeDefaultProfile();
}

bool menuWasActive = false;

// Add a 2 s grace period so USB‐Serial stays alive before heavy work
static bool firstRun = true;

void loop() {
  if (firstRun) {
    if (millis() < 2000) {
      // During first 2 seconds, do nothing to keep USB alive
      return;
    }
    firstRun = false;
  }
  int time_delta = timeDelta();
  menuHandler.loop(time_delta);
  serviceModeProfileSnap();
  serviceProfileEditSync();   // #18: Tire Profiles edit buffer follows the selector

  // "Set Offsets" was picked (#23). The action could not close the menu itself, so do it
  // here -- and only note the entry: the mode must not start until applyMenuConfig() below
  // has rebuilt tempReader, or it would be handed a pointer initializeSystem() is about to
  // delete.
  if (consumeOffsetSetupRequest()){
    menuHandler.closeMenu();
    offsetSetupPending = true;
  }

  if (!menuHandler.isMenuActive()) {
    if(menuWasActive){
      menuWasActive = false;
      applyMenuConfig();
      if (offsetSetupPending){
        offsetSetupPending = false;
        beginOffsetSetup();
      }
    }
    if (offsetSetupActive){
      // Offset setup owns the screen and all four swipe directions.
      checkOffsetSetupSwipes();                                 // may end the mode
      if (offsetSetupActive) doOffsetSetupMode(time_delta);
    } else {
      checkForSwipes();
      serviceFeedback();
      if (summaryAutoView){
        // Full-screen sealed summary owns the display; repaint only on page change.
        if (summaryAutoDirty){
          SessionManager::renderSummary(display, sessionManager.summary(), summaryAutoPage);
          summaryAutoDirty = false;
          display.drawScreen();   // F4: edge-triggered, so it must flush its own frame
        }
      } else {
        // drawSessionFeedback() and drawImuGateBar() used to be called here, after
        // doRunningMode() returned -- i.e. after its flush. Once per second the 1 Hz block
        // repaints the tire map, which wipes the badge footprint and the g bar's gutter
        // (ThreeSectionTire's clear rect is inflated by bufferPix on every side), and the
        // flush then pushed a frame with both of them missing. They were re-asserted only
        // on the following pass, after the glass had already updated -- a ~100 ms dropout
        // of the g bar and session badge, every second. They now run inside
        // doRunningMode(), immediately before the flush, so they land in the same frame.
        doRunningMode(time_delta);
      }
    }
  } else {
    menuWasActive = true;
    // Opening the menu takes over the screen: drop any running-mode summary/feedback.
    summaryAutoView = false;
    fbState = FB_NONE;
  }
}

// Cleanup objects to avoid memory leaks
static void cleanupObjects()
{
  if (wheels) {
    delete wheels;
    wheels = nullptr;
  }
  if (tempReader) {
    delete tempReader;
    tempReader = nullptr;
  }
}


static void initializeSystem()
{
  cleanupObjects();

  extern uint8_t getTemperatureScaleValue();
  extern uint8_t getNightBrightness();
  extern uint8_t getUseThermalGradient();
  extern bool getTestEnabled();
  extern uint8_t getCalcDisplayMode();
  
  extern bool getShowPixelOffsets();
  extern bool getHighFrequencyUpdates();
  extern bool getShowSegmentDeltas();  
  extern byte getLeftPixelOffset(int index);
  extern byte getRightPixelOffset(int index);
  extern byte getminInflationDeltaPct();
  extern byte getminAlignmentDeltaPct();

  extern bool getImuGateEnabled();
  extern uint8_t getLateralGateCentiG();
  extern uint8_t getGateDwellTenths();
  extern uint8_t getAlertDwellTenths();
  extern uint8_t getImuOrient();


  // No mode read here any more: the window comes from the active profile and Calculated is
  // no longer Track-gated (#16), so initializeSystem() is mode-agnostic.
  uint8_t scaleVal = getTemperatureScaleValue();   // 0=F,1=C

  nightBrightness = (int)(((float)getNightBrightness()/100.0f)*255.0f);

  highFrequencyUpdates = getHighFrequencyUpdates();
  USBSerial.println((highFrequencyUpdates)? "highFrequencyUpdates Enabled": "highFrequencyUpdates Disabled");

  enableThermalTemps = true;  // hardcoded — Test toggle is used for BufferedDisplay switching
  if (enableThermalTemps)
    THERMAL_MODES = 3;
  else
    THERMAL_MODES=4;

  // Push IMU capture-gate settings (global; centi-g and tenths-of-a-second encodings).
  // Two distinct dwells: Gate Dwl gates when capture STARTS, Dwell gates when the
  // over/under verdict LATCHES on already-captured frames (#20).
  imuGate.applyConfig(getImuGateEnabled(),
                      getLateralGateCentiG() / 100.0f,
                      (unsigned long)getGateDwellTenths() * 100UL,
                      (unsigned long)getAlertDwellTenths() * 100UL,
                      (IMUGate::Orient)getImuOrient());

  // The gate settings just changed, so the bar's zone width and colors are stale.
  imuGateBarInvalidate();

  // Single source of truth for the tire window (#14): BOTH modes read the active tire
  // profile, which bundles window + K + tau + camera crop, so switching profile swaps all
  // of them at once. The mode only chooses which profile is the default -- see
  // applyModeDefaultProfile(), which snaps the active slot at boot and on mode changes.
  const TireProfile& p = TireProfiles::active();
  float minTemp   = p.windowMin;
  float idealTemp = p.windowIdeal;
  float maxTemp   = p.windowMax;
  float calcTau   = p.tauSeconds;
  float calcK     = p.offsetK;
  USBSerial.print("Active tire profile: ");
  USBSerial.println(p.name);

    ThermalDisplay::useGradient = getUseThermalGradient();
    ThermalDisplay::showPixelOffsets = getShowPixelOffsets();
  if (scaleVal)
    ThermalDisplay::setTemperatureRangeC((int)minTemp,(int)idealTemp, (int)maxTemp);
  else
    ThermalDisplay::setTemperatureRangeF((int)minTemp,(int)idealTemp, (int)maxTemp);



  char tempUnit = (scaleVal == 0) ? 'F' : 'C';

  // Switch display implementation based on Test toggle.
  //
  // STAGED (#28): tying the buffer to Test is temporary, so the buffered and direct
  // paths can be A/B'd on the car by eye before the buffer becomes the default.
  // Phase 2 decouples them and restores Test's documented meaning.
  //
  // begin() is where the 134 KB canvas is actually allocated -- deferred out of
  // static init so it lands in PSRAM rather than internal DRAM, and so it costs
  // nothing at all when Test is off. If it fails we say so and stay on the direct
  // path rather than running with a display that silently draws nothing.
  bool useBuffered = getTestEnabled();
  if (useBuffered && !bufferedDisplay.begin()) {
    USBSerial.println("BufferedDisplay unavailable -- falling back to direct draw");
    useBuffered = false;
  }
  displayProxy.setImplementation(useBuffered ? (DisplayBase*)&bufferedDisplay : (DisplayBase*)&standardDisplay);

  // Re-assert the text defaults on whichever implementation just became active.
  // setup() set these on StandardDisplay before the canvas existed, and each
  // implementation keeps its own GFX text state, so they do not carry across.
  // Without this the degree ring (0xF8) renders as the stray dot at 0xF9 -- but
  // only on the buffered path, which is a miserable bug to chase.
  display.cp437(true);

  tempReader = new TempReader();
  tempReader->useFarenheit = (scaleVal == 0);

  // Calculated (surface->carcass) mode is available in BOTH modes (#16): the Display
  // setting alone decides, and K/tau come from the active profile either way. It used to
  // be gated on Track, which silently ignored a Calculated pick made in Street.
  tempReader->configureCalculated(getCalcDisplayMode() == 1, calcTau, calcK);

  bool fl3 = tempReader->tireSensorIsCamera[0]; //false;
  bool fr3 = tempReader->tireSensorIsCamera[1]; //false;
  bool rl3 = tempReader->tireSensorIsCamera[2]; //false;
  bool rr3 = tempReader->tireSensorIsCamera[3]; //false;

  tempReader->autoAdjustClock = true;// getShowPixelOffsets();


  wheels = new Wheels(10, ST77XX_YELLOW, ST77XX_WHITE,
                      minTemp, idealTemp, maxTemp,
                      tempUnit,
                      getShowSegmentDeltas(), getminInflationDeltaPct(), getminAlignmentDeltaPct(),
                      fl3, fr3, rl3, rr3);



  for (int i=0; i<4; i++){
    tempReader->leftPixelOffset[i] = getLeftPixelOffset(i);
    tempReader->rightPixelOffset[i] = getRightPixelOffset(i);    
  }

  ThermalDisplay::tempReader = tempReader;

      constexpr float zeros[3] = { 0.0f, 0.0f, 0.0f };
      Wheels::TireTemps fl(zeros);  // three‐value
      Wheels::TireTemps fr(zeros);// single‐value 
      Wheels::TireTemps rl(zeros);
      Wheels::TireTemps rr(zeros); 

      wheels->setTireTemps(fl, fr, rl, rr);
  //wheels->setTireTemps(0, 0, 0, 0);
  display.fillScreen(ST77XX_BLACK);
  imuGateBarInvalidate();

    activateTires();


  wheels->draw(true);
  //display.fillScreen(ST77XX_BLACK);
  forceDrawAfterInit = 2;
}

// Emit self-describing boot metadata (story 08 / #9): firmware SHA + the active config
// so a dump is interpretable long after capture. Mirrors initializeSystem()'s window/K/tau
// resolution: since #14 both modes report the active tire profile's values.
static void sendBootMetadata()
{
  extern uint8_t getTemperatureScaleValue();
  extern byte getLeftPixelOffset(int index);
  extern byte getRightPixelOffset(int index);

  bool track = (getCurrentModeValue() == 1);

  // The active profile is the mode's default profile (or a manual override), so it is the
  // window actually on screen in either mode.
  const TireProfile& p = TireProfiles::active();

  NBPProtocol::BootMetadata m;
  m.firmwareSha = FIRMWARE_GIT_SHA;
  m.modeName    = track ? "Track" : "Street";
  m.unit        = (getTemperatureScaleValue() == 0) ? 'F' : 'C';
  m.profileName = p.name;
  m.windowMin   = p.windowMin;
  m.windowIdeal = p.windowIdeal;
  m.windowMax   = p.windowMax;
  m.offsetK     = p.offsetK;
  m.tauSeconds  = p.tauSeconds;

  for (int i = 0; i < 4; i++) {
    m.leftOffset[i]  = getLeftPixelOffset(i);
    m.rightOffset[i] = getRightPixelOffset(i);
  }

  // No dedicated ambient/cabin sensor on this board; report the source honestly so a
  // reader knows the ambient field is absent, not zero-valued data.
  m.ambientSource = "none";

  nbp.sendBootMetadata(m);
}

static void activateTires(){
    if (enableThermalTemps && thermalMode == 2){
      wheels->flIsActive = true;
      wheels->frIsActive = true;
      wheels->rlIsActive = true;
      wheels->rrIsActive = true;
    }else{
      wheels->flIsActive = (!UL->isActive);
      wheels->frIsActive = (!UR->isActive);
      wheels->rlIsActive = (!LL->isActive);
      wheels->rrIsActive = (!LR->isActive);
    }
}

static void applyMenuConfig()
{
  initializeSystem();
}
