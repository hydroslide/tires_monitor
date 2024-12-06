#ifndef TEMPREADER_H
#define TEMPREADER_H

#include <SPI.h>
#include <Wire.h>
#include <Adafruit_MLX90614.h>

#define TIRE_COUNT 4

class TempReader {
    private:
        float getTemp(uint8_t index, bool farenheit);
        void select_I2C_bus(uint8_t bus);
        Adafruit_MLX90614 mlx_0;

    public:
        TempReader();
        void setup();        
        float tireTemps[TIRE_COUNT];
        void readTemps();
};
#endif // TEMPREADER_H