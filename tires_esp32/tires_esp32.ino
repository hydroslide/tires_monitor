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

HWCDC USBSerial;
SPIClass hspi(HSPI);
Adafruit_ST7789 tft = Adafruit_ST7789(&hspi, LCD_CS, LCD_DC, LCD_RST);

// WifiSerial instance
WifiSerial wifiSerial;
NBPProtocol nbp(wifiSerial);

// Existing objects
Wheels* wheels = nullptr;
TempReader* tempReader = nullptr;

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

// Normal Running Mode
void doRunningMode(int time_delta)
{
  millisSinceLastUpdate += time_delta;
  millisSinceLastRead += time_delta;
  if (millisSinceLastRead >= readIntervalMillis)
  {
    millisSinceLastRead = 0;

    // Read tire temps
    tempReader->readTemps();

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

      if (!testMode)
        nbp.setAllTireTemps(fl, fr, rl, rr, (wheels->getTempUnit() == 'F')); 

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
  EEPROM.begin(50);

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
  
  extern bool getShowPixelOffsets();
  extern bool getHighFrequencyUpdates();
  extern bool getShowSegmentDeltas();  
  extern byte getLeftPixelOffset(int index);
  extern byte getRightPixelOffset(int index);
  extern byte getminInflationDeltaPct();
  extern byte getminAlignmentDeltaPct();


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

  float minTemp, idealTemp, maxTemp;
  if (modeVal == 0) {
    // Street
    minTemp   = getStreetMin();
    idealTemp = getStreetIdeal();
    maxTemp   = getStreetMax();
  } else {
    // Track
    minTemp   = getTrackMin();
    idealTemp = getTrackIdeal();
    maxTemp   = getTrackMax();
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
