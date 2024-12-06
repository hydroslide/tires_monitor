#include <Adafruit_GFX.h>    // Core graphics library
#include <Adafruit_ST7789.h> // Hardware-specific library for ST7789
#include <SPI.h>
#include <Wire.h>
#include <Arduino.h>
#include "Arduino_GFX_Library.h"
#include "pin_config.h"
#include "HWCDC.h"

#include "Tire.h"
#include "Wheels.h"
#include "TempReader.h"
//#include <SoftwareSerial.h>
#include "NBPProtocol.h"

// Color definitions
#define BLACK 0x0000
#define BLUE 0x001F
#define RED 0xF800
#define GREEN 0x07E0
#define CYAN 0x07FF
#define MAGENTA 0xF81F
#define YELLOW 0xFFE0
#define WHITE 0xFFFF
//#define PURPLE 0xE01F//0xD01F

#define TFT_CS 10
#define TFT_RST 8
#define TFT_DC 7

Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);

// HC-05 Bluetooth module connected to pins 10 (RX) and 11 (TX)
//SoftwareSerial bluetoothSerial(3, 2);
//NBPProtocol nbp(bluetoothSerial);
NBPProtocol nbp(Serial);

long millisSinceLastUpdate = 0;
long updateIntervalMillis = 1000;

// Global object of Wheels
Wheels *wheels;

TempReader *tempReader;

unsigned long previousTime;
long timeDelta()
{
  unsigned long currentTime = millis();
  long delta = (long)(currentTime - previousTime);
  previousTime = currentTime;
  return delta;
}

void setup(void)
{
    Serial.begin(9600);

    //bluetoothSerial.begin(9600);
    nbp.sendMetadata("NAME", "Tire Temp Reader");
    nbp.sendMetadata("VERSION", "0.1");

    tft.init(240, 280); // Init ST7789 280x240

    tft.setRotation(1);
    tft.fillScreen(ST77XX_BLACK);
    //wheels = new Wheels(10, ST77XX_WHITE, ST77XX_YELLOW, 100.0, 120.0, 180.0, 'F');
        wheels = new Wheels(10, ST77XX_WHITE, ST77XX_YELLOW, 75.0, 85.0, 100.0, 'F');
    tempReader = new TempReader();

    // drawTires(WHITE, BLUE);
    //wheels->setTireTemps(200, 200, 200, 200);
    wheels->setTireTemps(0, 0, 0, 0);
    wheels->draw(true);

    Wire.begin();
}

void loop()
{
    millisSinceLastUpdate += timeDelta();

    if (millisSinceLastUpdate >= updateIntervalMillis)
    {
       Serial.print("Top of Loop - millisSinceLastUpdate: ");
       Serial.print(millisSinceLastUpdate);
        Serial.print(" > updateIntervalMillis: ");
        Serial.println(updateIntervalMillis);
        millisSinceLastUpdate = 0;

       
        // Get the temperatures for all 4 tires
        // float *temps = GetTemps(70, 220, 180);

        tempReader->readTemps();

        nbp.setTireTemps(tempReader->tireTemps[0], tempReader->tireTemps[1], tempReader->tireTemps[2], tempReader->tireTemps[3], (wheels->getTempUnit()=='F'));

        // Set the temperature for each tire based on the returned values
        wheels->setTireTemps(tempReader->tireTemps[0], tempReader->tireTemps[1], tempReader->tireTemps[2], tempReader->tireTemps[3]);
        wheels->draw();
    }

    // Small delay to control the update rate
    delay(50);
}
