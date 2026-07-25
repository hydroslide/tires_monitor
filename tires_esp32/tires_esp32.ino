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
#include "TireProfiles.h"

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

// Calculated (surface->carcass) mode defaults, story 03. Used only as a fallback in
// Street mode / when no profile applies; in Track mode K and tau come from the active
// tire profile (story 04).
static const float CALC_TAU_DEFAULT_S    = 15.0f; // EMA smoothing time constant (s)
static const float CALC_OFFSET_DEFAULT_F = 20.0f; // carcass offset K (+degrees F)

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
extern uint8_t getCurrentModeValue();

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
  activateTires();
  if (!(enableThermalTemps && thermalMode ==2))
    wheels->draw(true);
}

// Interim over/under source for the latch (story 02). Computes the overall
// center-hot vote across the camera tires from the raw section temps. Stories
// 03/04/06 replace this with the calculated, baseline-corrected verdict.
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
    imuGate.feedCondition(computeInterimInflationCondition());
    if (!testMode && imuGate.isPresent()) {
      nbp.sendIMU(imuGate.accelG(0), imuGate.accelG(1), imuGate.accelG(2),
                  imuGate.gyroDps(0), imuGate.gyroDps(1), imuGate.gyroDps(2),
                  imuGate.lateralG());
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
  // active-slot selection and profile struct region are the authority.
  TireProfiles::begin();

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

  USBSerial.println("Bottom of ESP32 Tires Setup");
}

void checkForSwipes(){
  if (menuHandler.SwipedRight())
    ToggleNightMode();
  if (menuHandler.SwipedUp())
    switchThermalMode(true);
  if (menuHandler.SwipedDown())
    switchThermalMode(false);
}

void ToggleNightMode(){
    nightMode=!nightMode;
    int bright = (nightMode)?nightBrightness:255;
    analogWrite(LCD_BL, bright);
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
  if (!menuHandler.isMenuActive()) {
    if(menuWasActive){
      menuWasActive = false;
      applyMenuConfig();
    }
    checkForSwipes();
    doRunningMode(time_delta);
  } else {
    menuWasActive = true;
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

  extern uint8_t getCurrentModeValue();
  extern uint8_t getTemperatureScaleValue();
  extern uint8_t getNightBrightness();
  extern uint8_t getUseThermalGradient();
  extern bool getTestEnabled();
  extern uint8_t getStreetMin();
  extern uint8_t getStreetIdeal();
  extern uint8_t getStreetMax();
  extern uint8_t getTrackMin();
  extern uint8_t getTrackIdeal();
  extern uint8_t getTrackMax();
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
  extern uint8_t getAlertDwellTenths();
  extern uint8_t getImuOrient();


  uint8_t modeVal = getCurrentModeValue();         // 0=Street,1=Track
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
  imuGate.applyConfig(getImuGateEnabled(),
                      getLateralGateCentiG() / 100.0f,
                      (unsigned long)getAlertDwellTenths() * 100UL,
                      (IMUGate::Orient)getImuOrient());

  float minTemp, idealTemp, maxTemp;
  // Track-mode K/tau come from the active tire profile (story 04); default otherwise.
  float calcTau = CALC_TAU_DEFAULT_S;
  float calcK   = CALC_OFFSET_DEFAULT_F;
  if (modeVal == 0) {
    // Street: tire profiles are inert; use the plain Street window.
    minTemp   = getStreetMin();
    idealTemp = getStreetIdeal();
    maxTemp   = getStreetMax();
  } else {
    // Track: the active tire profile bundles window + K + tau together, so selecting a
    // profile swaps all of them at once.
    const TireProfile& p = TireProfiles::active();
    minTemp   = p.windowMin;
    idealTemp = p.windowIdeal;
    maxTemp   = p.windowMax;
    calcTau   = p.tauSeconds;
    calcK     = p.offsetK;
    USBSerial.print("Active tire profile: ");
    USBSerial.println(p.name);
  }

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

  // Story 03: calculated (surface->carcass) mode is a Track-mode feature. Enable only
  // when the Display setting is Calculated AND we are in Track mode; Street stays raw.
  tempReader->configureCalculated((getCalcDisplayMode() == 1) && (modeVal == 1),
                                  calcTau, calcK);

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

    activateTires();


  wheels->draw(true);
  //tft.fillScreen(ST77XX_BLACK);
  forceDrawAfterInit = 2;
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
