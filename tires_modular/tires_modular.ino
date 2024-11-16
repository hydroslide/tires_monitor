#include <Adafruit_GFX.h>    // Core graphics library
#include <Adafruit_ST7789.h> // Hardware-specific library for ST7789
//#include <Adafruit_MLX90614.h>
#include <SPI.h>
#include <Wire.h>

#include "Tire.h"
#include "Wheels.h"
#include "TempReader.h"



// Color definitions
#define BLACK 0x0000
#define BLUE 0x001F
#define RED 0xF800
#define GREEN 0x07E0
#define CYAN 0x07FF
#define MAGENTA 0xF81F
#define YELLOW 0xFFE0
#define WHITE 0xFFFF

#define TFT_CS 10
#define TFT_RST 8
#define TFT_DC 7

Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);



// Global object of Wheels
Wheels *wheels;

TempReader* tempReader;

void setup(void)
{
  Serial.begin(9600);
  Serial.print(F("Hello! ST77xx TFT Test"));

  tft.init(240, 280); // Init ST7789 280x240

  tft.setRotation(1);
  tft.fillScreen(ST77XX_BLACK);
  wheels = new Wheels(10, ST77XX_WHITE, ST77XX_YELLOW, 100.0, 180.0, 'F');
  tempReader = new TempReader();


  // drawTires(WHITE, BLUE);
  wheels->setTireTemps(200, 200, 200, 200);
  wheels->draw();

  Wire.begin();

}

void loop()
{
  Serial.println("Top of Loop");
  // Get the temperatures for all 4 tires
  // float *temps = GetTemps(70, 220, 180);
  

  tempReader->readTemps();


  // Set the temperature for each tire based on the returned values
  wheels->setTireTemps(tempReader->tireTemps[0], tempReader->tireTemps[1], tempReader->tireTemps[2], tempReader->tireTemps[3]);
  wheels->draw();

  // Small delay to control the update rate
  delay(1000); // 100 ms delay (adjust as needed for smoothness)
}
