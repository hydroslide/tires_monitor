// Adafruit_FT6206.cpp

#include "Adafruit_FT6206.h"

bool Adafruit_FT6206::interruptFlag = false;

Adafruit_FT6206::Adafruit_FT6206()
    : iicBus(std::make_shared<Arduino_HWIIC>(IIC_SDA, IIC_SCL, &Wire)),
      touchDevice(new Arduino_CST816x(iicBus, CST816T_DEVICE_ADDRESS, TP_RST, TP_INT, touchInterrupt)) {}

void Adafruit_FT6206::touchInterrupt() {
    interruptFlag = true;
}

bool Adafruit_FT6206::begin() {
    Wire.begin(IIC_SDA, IIC_SCL);

    if (!touchDevice->begin()) {
        Serial.println("CST816T initialization failed");
        return false;
    }

    Serial.println("CST816T initialized successfully");

    touchDevice->IIC_Write_Device_State(
        touchDevice->Arduino_IIC_Touch::Device::TOUCH_DEVICE_INTERRUPT_MODE,
        touchDevice->Arduino_IIC_Touch::Device_Mode::TOUCH_DEVICE_INTERRUPT_PERIODIC);

    return true;
}

bool Adafruit_FT6206::touched() {
    return interruptFlag;
}

void Adafruit_FT6206::getPoint(int32_t &x, int32_t &y) {
    x = touchDevice->IIC_Read_Device_Value(touchDevice->Arduino_IIC_Touch::Value_Information::TOUCH_COORDINATE_X);
    y = touchDevice->IIC_Read_Device_Value(touchDevice->Arduino_IIC_Touch::Value_Information::TOUCH_COORDINATE_Y);
    interruptFlag = false;
}
