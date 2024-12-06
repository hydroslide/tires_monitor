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
#include "NBPProtocol.h"
#include "BLESerial.h"



#define SDA_PIN 21
#define SCL_PIN 22

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

HWCDC USBSerial;

Adafruit_ST7789 tft = Adafruit_ST7789(LCD_CS, LCD_DC, LCD_MOSI, LCD_SCK, LCD_RST); //Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);

// Create a BluetoothSerial instance
BLESerial bleSerial;
NBPProtocol nbp(bleSerial);
//BluetoothSerial bluetoothSerial;
// NBPProtocol nbp(bluetoothSerial);
//NBPProtocol nbp(USBSerial);

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
    USBSerial.begin(9600);

     USBSerial.println("Top of ESP32 Tires Setup");

    pinMode(LCD_BL, OUTPUT);
    digitalWrite(LCD_BL, HIGH);

    // Start Bluetooth with the desired device name
         if (!bleSerial.begin("Tire Temp Monito")) {
        USBSerial.println("Failed to start BLE!");
        while (1);
    }
    Serial.println("BLE initialized!");
    // if (!bluetoothSerial.begin("Tire Temp Monitor")) {
    //     USBSerial.println("Failed to initialize Bluetooth. Check configuration.");
    //     while (1); // Stop further execution if Bluetooth initialization fails
    // }
    USBSerial.println("Bluetooth started. Device is ready to pair.");
    
    nbp.sendMetadata("NAME", "Tire Temp Reader");
    nbp.sendMetadata("VERSION", "0.1");

    tft.init(240, 280); // Init ST7789 280x240

    tft.setRotation(1);
    tft.fillScreen(ST77XX_BLACK);
    //wheels = new Wheels(10, ST77XX_WHITE, ST77XX_YELLOW, 100.0, 120.0, 180.0, 'F');
    wheels = new Wheels(10, ST77XX_YELLOW, ST77XX_WHITE, 75.0, 85.0, 100.0, 'F');
    //tempReader = new TempReader(); // TODO: Uncomment this

    // drawTires(WHITE, BLUE);

        USBSerial.println("Before setTireTemps");
    wheels->setTireTemps(0, 0, 0, 0);
    USBSerial.println("Before draw");
    wheels->draw(true);


    USBSerial.println("Before Wire.begin()");    
    // Initialize I2C with custom pins
    Wire.begin(SDA_PIN, SCL_PIN);
     USBSerial.println("Bottom of ESP32 Tires Setup");

}

void loop()
{
      
    millisSinceLastUpdate += timeDelta();


    if (millisSinceLastUpdate >= updateIntervalMillis)
    {
       USBSerial.print("Top of Loop - millisSinceLastUpdate: ");
       USBSerial.print(millisSinceLastUpdate);
        USBSerial.print(" > updateIntervalMillis: ");
        USBSerial.println(updateIntervalMillis);
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
