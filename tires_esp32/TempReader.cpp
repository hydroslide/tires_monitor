#include "TempReader.h"
#include <SPI.h>
#include <Wire.h>

extern HWCDC USBSerial;

TempReader::TempReader(){
    sensorIndices = [0,1,2,3];
}

void TempReader::readTemps(){
    for (uint8_t i = 0; i < TIRE_COUNT; i++)
    {

    float temp = getTemp(i, true);
        if (isnan(temp))
        temp = 0.0f;
        USBSerial.print("Temp Read: ");
        USBSerial.println(temp);
        tireTemps[i] = temp;
    }
}

void TempReader::setup(){
    USBSerial.println("mlx.begin");
    mlx_0.begin();  
    USBSerial.println("mlx.begun");
}

float TempReader::getTemp(uint8_t index, bool farenheit){
    index = sensorIndices[index];
    USBSerial.print("Getting temp: ");
    USBSerial.println(index);
    select_I2C_bus(index);
    mlx_0.begin();
    //return 0.0f;
    if (farenheit)
        return mlx_0.readObjectTempF();
    else
        return mlx_0.readObjectTempC();
}

void TempReader::select_I2C_bus(uint8_t bus){
    Wire.beginTransmission(0x70); // TCA9548A address
    Wire.write(1 << bus);

    // send byte to select bus
    int result = Wire.endTransmission();
    if (result != 0)
    {
        String err="";
        switch(result){
            case 1:
                err="length to long for buffer";
                break;
                            case 2:
                err="address send, NACK received";
                break;
                            case 3:
                err="data send, NACK received";
                break;
                            case 4:
                err="other twi error (lost bus arbitration, bus error, ..)";
                break;
                            case 5:
                err="timeout";
                break;
        }
        USBSerial.print("I2C error: ");
        USBSerial.println(err);
    }
}