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
//#include "NBPProtocol.h"
//#include "BLESerial.h"
#include "WifiSerial.h"

#define WIFI_SSID "TireTempMonitor"
#define WIFI_PASSWORD "esp32"
#define WIFI_PORT 8080

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
SPIClass hspi(HSPI);
Adafruit_ST7789 tft = Adafruit_ST7789(&hspi, LCD_CS, LCD_DC, LCD_RST);

// Create a BluetoothSerial instance
//BLESerial bleSerial;
//NBPProtocol nbp(bleSerial);


//BluetoothSerial bluetoothSerial;
// NBPProtocol nbp(bluetoothSerial);
//NBPProtocol nbp(USBSerial);

// WifiSerial instance
WifiSerial wifiSerial;
NBPProtocol nbp(wifiSerial);

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

/*
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
    */

    if (!wifiSerial.begin(WIFI_SSID, WIFI_PASSWORD, WIFI_PORT)) {
        Serial.println("Failed to start WiFi!");
        while (1);
    }
    Serial.println("WiFi initialized!");

    nbp.sendMetadata("NAME", "Tire Temp Reader");
    nbp.sendMetadata("VERSION", "0.1");


    hspi.begin(LCD_SCK, -1, LCD_MOSI, LCD_CS);
      // 80MHz should work, but you may need lower speeds
    tft.setSPISpeed(80000000);
    // this will vary depending on your display
    tft.init(240, 280, SPI_MODE0);

    tft.setRotation(1);
    tft.fillScreen(ST77XX_BLACK);
    //wheels = new Wheels(10, ST77XX_WHITE, ST77XX_YELLOW, 100.0, 120.0, 180.0, 'F');
    wheels = new Wheels(10, ST77XX_YELLOW, ST77XX_WHITE, 75.0, 85.0, 100.0, 'F');
    tempReader = new TempReader(); // TODO: Uncomment this

    // drawTires(WHITE, BLUE);

    USBSerial.println("Before setTireTemps");
    wheels->setTireTemps(0, 0, 0, 0);
    USBSerial.println("Before draw");
    wheels->draw(true);


    USBSerial.println("Before Wire.begin()");    
    // Initialize I2C with custom pins
    Wire.setPins(IIC_SDA, IIC_SCL);
     Wire.begin();
     USBSerial.println("Bottom of ESP32 Tires Setup");

}

void loop()
{
      
    millisSinceLastUpdate += timeDelta();


    if (millisSinceLastUpdate >= updateIntervalMillis)
    {
        /*
       USBSerial.print("Top of Loop - millisSinceLastUpdate: ");
       USBSerial.print(millisSinceLastUpdate);
        USBSerial.print(" > updateIntervalMillis: ");
        USBSerial.println(updateIntervalMillis);
        */
        long tempMillis = millisSinceLastUpdate;
        millisSinceLastUpdate = 0;

        // Get the temperatures for all 4 tires
        // float *temps = GetTemps(70, 220, 180);

        tempReader->readTemps();

        nbp.setTireTemps(tempReader->tireTemps[0], tempReader->tireTemps[1], tempReader->tireTemps[2], tempReader->tireTemps[3], (wheels->getTempUnit()=='F'));

        // Set the temperature for each tire based on the returned values
        wheels->setTireTemps(tempReader->tireTemps[0], tempReader->tireTemps[1], tempReader->tireTemps[2], tempReader->tireTemps[3]);
        int newTemp = (int)(tempMillis%100);
        //wheels->setTireTemps(newTemp+1, newTemp+2, newTemp+3, newTemp+4);
        wheels->draw();

        // Call the loop function for WifiSerial
        wifiSerial.loop();
    }
          

    // Small delay to control the update rate
    delay(50);
}
