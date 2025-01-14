// Adafruit_FT6206.h

#ifndef ADAFRUIT_FT6206_H
#define ADAFRUIT_FT6206_H

#include <Arduino.h>
#include <Wire.h>
#include "Arduino_DriveBus_Library.h"

class Adafruit_FT6206 {
public:
    Adafruit_FT6206();

    // Initializes the touch device
    bool begin();

    // Reads touch point data
    bool touched();
    void getPoint(int32_t &x, int32_t &y);

private:
    std::unique_ptr<Arduino_CST816x> touchDevice;
    std::shared_ptr<Arduino_IIC> iicBus;
    static void touchInterrupt();
    static bool interruptFlag;
};

#endif // ADAFRUIT_FT6206_H