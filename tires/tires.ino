#include <Adafruit_GFX.h>    // Core graphics library
#include <Adafruit_ST7789.h> // Hardware-specific library for ST7789
#include <Adafruit_MLX90614.h>
#include <SPI.h>
#include <Wire.h>

// Color definitions
#define BLACK 0x0000
#define BLUE 0x001F
#define RED 0xF800
#define GREEN 0x07E0
#define CYAN 0x07FF
#define MAGENTA 0xF81F
#define YELLOW 0xFFE0
#define WHITE 0xFFFF

#define TIRE_COUNT 4

#define TFT_CS 10
#define TFT_RST 8
#define TFT_DC 7

Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);

Adafruit_MLX90614 mlx_0;

// Global variables to store the time-dependent values
unsigned long lastUpdateTime = 0;
float time = 0.0; // Time variable to drive the sine wave oscillation

  // Static array to hold the temperatures of the four tires
  float tireTemps[TIRE_COUNT];


// Tire Class Definition
class Tire
{
private:
  uint16_t fillColor;

public:
  int x, y, width, height;
  uint16_t outlineColor, textColor;
  char tempUnit;
  float temperature; // Current temperature of the tire
  float lastTemp;
  bool initialized=false;
  bool crossedThreshold = true;

  Tire() {}
  // Constructor to initialize a Tire object with position, size, and colors
  Tire(int _x, int _y, int _width, int _height, uint16_t _outlineColor, uint16_t _textColor, char _tempUnit)
      : x(_x), y(_y), width(_width), height(_height), outlineColor(_outlineColor), textColor(_textColor), temperature(0), tempUnit(_tempUnit) {}

  // Method to draw the tire on the screen
  void draw()
  {
    if ((int)temperature != (int)lastTemp)
    {
      int radius = 20; // Rounded corners for the tire
      if (crossedThreshold)
        tft.fillRoundRect(x, y, width, height, radius, fillColor);
      printTemp();
      if (crossedThreshold)
        tft.drawRoundRect(x, y, width, height, radius, outlineColor);
    }
  }

  void printTemp()
  {
    // Convert temperature to integer to remove decimal places
    int tempInt = (int)temperature; // Discards the decimal portion

    // Form the string (temperature + ° + F)
    String tempString = String(tempInt) + (char)0xF7 + tempUnit; // ° symbol using 0xB0

    int16_t textWidth, textHeight;
    tft.setTextSize(4); // Set text size
    // Calculate the width of the text to center it
    tft.getTextBounds(tempString, 0, 0, NULL, NULL, &textWidth, &textHeight); // Get width of text

    // Calculate the starting x position to center the text
    int startX = x + (width - textWidth) / 2;

    // Set the cursor to the calculated position and print the temperature
    tft.setCursor(startX, y + (height / 2) - (textHeight / 2)); // Adjust the vertical position as needed
    tft.setTextColor(textColor, fillColor);                     // Set text color

    tft.println(tempString); // Print the temperature
    lastTemp = temperature;
  }

  // Method to set the temperature and update the fillColor based on thresholds
  void setTemp(float temp, float minTemp, float maxTemp, uint16_t lowColor, uint16_t normalColor, uint16_t highColor)
  {
    if (isnan(temp))
      temp = 0.0f;
    temperature = temp;

    uint16_t newColor;
    // Check if the temperature is within the normal range, below it, or above it
    if (temperature < minTemp)
    {
      newColor = lowColor; // Below normal
    }
    else if (temperature > maxTemp)
    {
      newColor = highColor; // Above normal
    }
    else
    {
      newColor = normalColor; // Normal range
    }
    if (newColor != fillColor)
    {
      fillColor = newColor;
      crossedThreshold = true;
    }
    else{
      if (initialized)
        crossedThreshold = false;
      else
        initialized=true;
    }
  }
};

// Wheels Class Definition
class Wheels
{
public:
  Tire frontLeft, frontRight, rearLeft, rearRight;
  float minTemp, maxTemp;                                           // Thresholds for the normal temperature range
  uint16_t lowTempColor, normalTempColor, highTempColor, textColor; // Colors for the different temperature states

  Wheels() {}
  // Constructor to initialize the 4 tires and set temperature thresholds and color defaults
  Wheels(int bufferPix, uint16_t outlineColor, uint16_t _textColor, float _minTemp, float _maxTemp, char tempUnit,
         uint16_t _lowTempColor = ST77XX_BLUE, uint16_t _normalTempColor = ST77XX_GREEN, uint16_t _highTempColor = ST77XX_RED)
      : textColor(_textColor),
        minTemp(_minTemp), maxTemp(_maxTemp),
        lowTempColor(_lowTempColor), normalTempColor(_normalTempColor), highTempColor(_highTempColor)
  {
    // Initialize tire dimensions and positions
    int tireWidth = (tft.width() - (bufferPix * 3)) / 2;
    int tireHeight = (tft.height() - (bufferPix * 3)) / 2;
    int tire_0_x = bufferPix;
    int tire_1_x = tire_0_x + tireWidth + bufferPix;
    int tire_0_y = bufferPix;
    int tire_1_y = tire_0_y + tireHeight + bufferPix;

    // Initialize each tire using the constructor with all the necessary parameters
    frontLeft = Tire(tire_0_x, tire_0_y, tireWidth, tireHeight, outlineColor, _textColor, tempUnit);
    frontRight = Tire(tire_1_x, tire_0_y, tireWidth, tireHeight, outlineColor, _textColor, tempUnit);
    rearLeft = Tire(tire_0_x, tire_1_y, tireWidth, tireHeight, outlineColor, _textColor, tempUnit);
    rearRight = Tire(tire_1_x, tire_1_y, tireWidth, tireHeight, outlineColor, _textColor, tempUnit);
  }

  // Method to draw all four tires
  void draw()
  {
    frontLeft.draw();
    frontRight.draw();
    rearLeft.draw();
    rearRight.draw();
  }

  // Method to set the temperature of all tires
  void setTireTemps(float frontLeftTemp, float frontRightTemp, float rearLeftTemp, float rearRightTemp)
  {
    frontLeft.setTemp(frontLeftTemp, minTemp, maxTemp, lowTempColor, normalTempColor, highTempColor);
    frontRight.setTemp(frontRightTemp, minTemp, maxTemp, lowTempColor, normalTempColor, highTempColor);
    rearLeft.setTemp(rearLeftTemp, minTemp, maxTemp, lowTempColor, normalTempColor, highTempColor);
    rearRight.setTemp(rearRightTemp, minTemp, maxTemp, lowTempColor, normalTempColor, highTempColor);
  }
};



float getTemp(uint8_t index, Adafruit_MLX90614 mlx, bool farenheit)
{
  Serial.print("Getting temp at index: ");
  Serial.println(index);
  //select_I2C_bus(index);
  return 103.0f;
  if (farenheit)
    return mlx.readObjectTempF();
  else
    return mlx.readObjectTempC();
}

float getTemp(uint8_t index, bool farenheit)
{
  Serial.print("Getting temp: ");
  Serial.println(index);
  select_I2C_bus(index);
  mlx_0.begin();
  //return 0.0f;
  if (farenheit)
    return mlx_0.readObjectTempF();
  else
    return mlx_0.readObjectTempC();
}



// Select I2C BUS
void select_I2C_bus(uint8_t bus)
{
  Wire.beginTransmission(0x70); // TCA9548A address
  Wire.write(1 << bus);

  // send byte to select bus
  if (Wire.endTransmission() != 0)
  {
    Serial.println("I2C error");
  }
}

// Global object of Wheels
Wheels *wheels;

void setup(void)
{
  Serial.begin(9600);
  Serial.print(F("Hello! ST77xx TFT Test"));

  tft.init(240, 280); // Init ST7789 280x240

  tft.setRotation(1);
  tft.fillScreen(ST77XX_BLACK);
  wheels = new Wheels(10, ST77XX_WHITE, ST77XX_YELLOW, 100.0, 180.0, 'F');


  // drawTires(WHITE, BLUE);
  wheels->setTireTemps(200, 200, 200, 200);
  wheels->draw();

  Wire.begin();
  Serial.println("mlx.begin");
  mlx_0.begin();
  
  
  Serial.println("mlx.begun");
}

void loop()
{
  Serial.println("Top of Loop");
  // Get the temperatures for all 4 tires
  // float *temps = GetTemps(70, 220, 180);
  

  for (uint8_t i = 0; i < TIRE_COUNT; i++)
  {

   float temp = getTemp(i, true);
    if (isnan(temp))
      temp = 0.0f;
    Serial.print("Temp Read: ");
    Serial.println(temp);
    tireTemps[i] = temp;
  }


  // Set the temperature for each tire based on the returned values
  wheels->setTireTemps(tireTemps[0], tireTemps[1], tireTemps[2], tireTemps[3]);
  wheels->draw();

  // Small delay to control the update rate
  delay(1000); // 100 ms delay (adjust as needed for smoothness)
}
