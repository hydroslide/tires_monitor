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

// ... after initializing tft in setup() ...
QuadrantFactory factory(tft, /*margin=*/ 5);

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
  MenuRenderer menuRenderer(menuSystem, tft);

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

void setThermalMode(uint8_t _thermalMode){
  thermalMode = _thermalMode;
  if (enableThermalTemps){
    if (thermalMode == 0){
        UL->isActive=false;
        UR->isActive=false;
        LL->isActive=false;
        LR->isActive=false;
    }else{
        UL->isActive=true;
        UR->isActive=true;
        LL->isActive=true;
        LR->isActive=true;
    }
  }else{
    switch (thermalMode)
    {
      case 0:
        UL->isActive=false;
        UR->isActive=false;
        LL->isActive=false;
        LR->isActive=false;
        break;

      case 1:
        UL->isActive=false;
        UR->isActive=false;
        LL->isActive=true;
        LR->isActive=true;
        break;
      
      case 2:
        UL->isActive=true;
        UR->isActive=true;
        LL->isActive=false;
        LR->isActive=false;
        break;
      
      case 3:
        UL->isActive=true;
        UR->isActive=true;
        LL->isActive=true;
        LR->isActive=true;
        break;
      
      default:
        break;
    }
  }
  tft.fillScreen(ST77XX_BLACK);
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
extern byte getminInflationDeltaPct();
static int computeInterimInflationCondition()
{
  int overVotes = 0, underVotes = 0;
  float pct = getminInflationDeltaPct() / 100.0f;
  for (int t = 0; t < TIRE_COUNT; t++) {
    if (!tempReader->tireSensorIsCamera[t]) continue;
    float edge = (tempReader->tireSectionTemps[t][0] +
                  tempReader->tireSectionTemps[t][2]) / 2.0f;
    float center = tempReader->tireSectionTemps[t][1];
    if (edge <= 0.0f) continue;                  // skip unread/invalid frames
    float delta = edge - center;                 // negative => center hotter
    float minDelta = edge * pct;
    if (delta <= -minDelta) overVotes++;         // center-hot => over-inflation
    else if (delta >= minDelta) underVotes++;    // edges-hot => under-inflation
  }
  if (overVotes > underVotes) return 1;
  if (underVotes > overVotes) return -1;
  return 0;
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
    // Story 06: the inflation indicator is a Track-mode feature with its own on/off
    // toggle. When off (or in Street), feed a neutral condition so the latch never
    // triggers -- the verdict is inert / hidden, matching the acceptance criteria.
    extern bool getInflationIndicator();
    bool inflationOn = trackMode && getInflationIndicator();
    imuGate.feedCondition(inflationOn ? computeInterimInflationCondition() : 0);
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

      // Story 06: track inflation-indicator on-time over captured (straight-line) frames
      // while the indicator is enabled, so the summary can surface the verdict when it
      // was on >= 50% of the captured session. Latched alert -> signed verdict.
      if (inflationOn) {
        int av = (imuGate.alertState() == IMUGate::ALERT_OVER)  ?  1
               : (imuGate.alertState() == IMUGate::ALERT_UNDER) ? -1 : 0;
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
      if (forceDrawAfterInit>0){
          tft.fillScreen(ST77XX_BLACK);
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
      // plain edge-vs-center spread (#18 dropped the baseline correction); a center-hot
      // spread beyond Threshold votes OVER (+1), edge-hot votes UNDER (-1); Overall is
      // the latched IMU state.
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
          float d   = edge - center;
          float thr = edge * pct;
          insDelta[c]  = d;
          insThresh[c] = thr;
          if (edge > 0.0f) {                       // skip unread/invalid frames
            if (d <= -thr)      insVerdict[c] = 1;  // center-hot => over-inflation
            else if (d >= thr)  insVerdict[c] = -1; // edges-hot  => under-inflation
          }
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



    // for (int i=0; i>TempReader::TIRE_COUNT; i++){
      
    // }
    // if (tempReader->tireSensorIsCamera[0])
    //   UR->updateDisplay(tempReader->tire_frames[0]);
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

  // SPI + TFT
  hspi.begin(LCD_SCK, -1, LCD_MOSI, LCD_CS);
  //tft.setSPISpeed(80000000);
  // Slow down SPI to 40MHz (more stable than 80MHz)
  tft.setSPISpeed(40000000);
  tft.init(240, 280, SPI_MODE0);
  // Use the real CP437 charset (#17). Adafruit_GFX otherwise applies a legacy shift --
  // `if (!_cp437 && (c >= 176)) c++` -- which would silently render the degree ring
  // (0xF8) as the stray dot at 0xF9. Only affects chars >= 176; all our strings are
  // ASCII, so nothing else changes. MenuRenderer references this same tft.
  tft.cp437(true);
  tft.setRotation(3);
  tft.fillScreen(ST77XX_BLACK);

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
}

// Leave the full-screen summary and restore the running display.
static void exitSummaryAutoView(){
  summaryAutoView = false;
  tft.fillScreen(ST77XX_BLACK);
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
    else if (sr) exitSummaryAutoView();
    return;
  }

  if (track){
    // Track mode: the left/right swipe toggles the SESSION (not Night Mode). Up/down
    // still switch the thermal display mode.
    if (sr) toggleSession();
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
      tft.fillScreen(ST77XX_BLACK);
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

  const int s = FB_R * 2;                            // both shapes share this footprint
  const int x = FB_CX - FB_R, y = FB_CY - FB_R;

  // The dark half paints the footprint black rather than restoring the tire underneath.
  // That costs nothing visually: the badge occludes that patch of the front-right tire for
  // the whole FB_MS either way, and expiry repaints the screen (FB_START) or hands off to
  // the summary (FB_END), so nothing is left behind.
  if ((((millis() - fbSetMs) / FB_BLINK_MS) & 1UL) != 0){
    tft.fillRect(x, y, s, s, ST77XX_BLACK);
    return;
  }

  if (fbState == FB_START){
    tft.fillCircle(FB_CX, FB_CY, FB_R, ST77XX_RED);  // recording dot
  } else { // FB_END
    tft.fillRect(x, y, s, s, ST77XX_BLACK);          // stop square
    // 2 px outline so it reads on black at a glance
    tft.drawRect(x,     y,     s,     s,     ST77XX_WHITE);
    tft.drawRect(x + 1, y + 1, s - 2, s - 2, ST77XX_WHITE);
  }
}

// Paint the latched inflation verdict over the running display (story 06). Track-mode
// only and gated by the Inflation toggle; once the IMU gate latches OVER/UNDER on
// straight-line frames it stays visible anywhere on track until the opposite threshold
// clears it. Small badge top-left so it's glanceable without hiding the tire map.
static void drawInflationIndicator(){
  if (getCurrentModeValue() != 1) return;          // Track-mode only
  extern bool getInflationIndicator();
  if (!getInflationIndicator()) return;            // menu toggle off
  IMUGate::Alert a = imuGate.alertState();
  if (a == IMUGate::ALERT_NONE) return;            // nothing latched
  uint16_t col = (a == IMUGate::ALERT_OVER) ? ST77XX_RED : ST77XX_CYAN;
  tft.fillRect(2, 2, 52, 16, ST77XX_BLACK);
  tft.drawRect(2, 2, 52, 16, col);
  tft.setFont(nullptr);
  tft.setTextSize(1);
  tft.setTextColor(col);
  tft.setCursor(6, 6);
  tft.print((a == IMUGate::ALERT_OVER) ? F("OVER") : F("UNDER"));
}

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
  if (!menuHandler.isMenuActive()) {
    if(menuWasActive){
      menuWasActive = false;
      applyMenuConfig();
    }
    checkForSwipes();
    serviceFeedback();
    if (summaryAutoView){
      // Full-screen sealed summary owns the display; repaint only on page change.
      if (summaryAutoDirty){
        SessionManager::renderSummary(tft, sessionManager.summary(), summaryAutoPage);
        summaryAutoDirty = false;
      }
    } else {
      doRunningMode(time_delta);
      drawSessionFeedback();
      drawInflationIndicator();
      drawImuGateBar(tft);
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

  enableThermalTemps = getTestEnabled();
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

  tempReader = new TempReader();
  tempReader->autoRecoverTire = true;
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
  tft.fillScreen(ST77XX_BLACK);
  imuGateBarInvalidate();

    activateTires();


  wheels->draw(true);
  //tft.fillScreen(ST77XX_BLACK);
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
