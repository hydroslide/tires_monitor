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

#define WIFI_SSID "TireTempMonitor"
#define WIFI_PASSWORD "esp32"
#define WIFI_PORT 8080

// Color definitions
#define BLACK   0x0000
#define BLUE    0x001F
#define RED     0xF800
#define GREEN   0x07E0
#define CYAN    0x07FF
#define MAGENTA 0xF81F
#define YELLOW  0xFFE0
#define WHITE   0xFFFF

HWCDC USBSerial;
SPIClass hspi(HSPI);
Adafruit_ST7789 tft = Adafruit_ST7789(&hspi, LCD_CS, LCD_DC, LCD_RST);

// WifiSerial instance
WifiSerial wifiSerial;
NBPProtocol nbp(wifiSerial);

// Existing objects
Wheels* wheels = nullptr;
TempReader* tempReader = nullptr;

// Keep track of time
unsigned long previousTime = 0;
long millisSinceLastUpdate = 0;
long updateIntervalMillis = 1000;

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

// Normal Running Mode
void doRunningMode(int time_delta)
{
  int asd=7;
  millisSinceLastUpdate += time_delta;
  if (millisSinceLastUpdate >= updateIntervalMillis)
  {
    long tempMillis = millisSinceLastUpdate;
    millisSinceLastUpdate = 0;

    // Read tire temps
    tempReader->readTemps();

    // Send them to NBP
    nbp.setTireTemps(
      tempReader->tireTemps[0],
      tempReader->tireTemps[1],
      tempReader->tireTemps[2],
      tempReader->tireTemps[3],
      (wheels->getTempUnit() == 'F')
    );

    // Update Wheels display
    wheels->setTireTemps(
      tempReader->tireTemps[0],
      tempReader->tireTemps[1],
      tempReader->tireTemps[2],
      tempReader->tireTemps[3]
    );
    wheels->draw();

    // WifiSerial
    wifiSerial.loop();
  }


}


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
  tft.setSPISpeed(80000000);
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

  // Initialize system objects
  initializeSystem();

  USBSerial.println("Bottom of ESP32 Tires Setup");
}

bool menuWasActive = false;
void loop()
{
  int time_delta = timeDelta();
  menuHandler.loop(time_delta);
  if (!menuHandler.isMenuActive()) {
    if(menuWasActive){
      menuWasActive = false;
      applyMenuConfig();
    }
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
  extern uint8_t getStreetMin();
  extern uint8_t getStreetIdeal();
  extern uint8_t getStreetMax();
  extern uint8_t getTrackMin();
  extern uint8_t getTrackIdeal();
  extern uint8_t getTrackMax();

  uint8_t modeVal = getCurrentModeValue();         // 0=Street,1=Track
  uint8_t scaleVal = getTemperatureScaleValue();   // 0=F,1=C

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

  char tempUnit = (scaleVal == 0) ? 'F' : 'C';

  wheels = new Wheels(10, ST77XX_YELLOW, ST77XX_WHITE,
                      minTemp, idealTemp, maxTemp,
                      tempUnit);

  tempReader = new TempReader();
  wheels->setTireTemps(0, 0, 0, 0);
  tft.fillScreen(ST77XX_BLACK);
  wheels->draw();
}

static void applyMenuConfig()
{
  initializeSystem();
}
