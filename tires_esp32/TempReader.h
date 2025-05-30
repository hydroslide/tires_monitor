#ifndef TEMPREADER_H
#define TEMPREADER_H

#include <array>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_MLX90614.h>

#define TIRE_COUNT 4
#define BUFFER_SIZE 10

class TempReader {
    private:
        float getTemp(uint8_t index, bool farenheit);
        void select_I2C_bus(uint8_t bus);
        Adafruit_MLX90614 mlx_0;
        long readIntervalMillis = 100;
        long millisSinceLastRead = 0;
        byte bufferNdx=0;
        bool useBuffer=true;

    public:
        TempReader();
        void setup();        
        float tireTemps[TIRE_COUNT];
        float tireTempBuffer[TIRE_COUNT][BUFFER_SIZE];
        std::array<uint8_t, 4> sensorIndices;
        void readTemps(int time_delta);
        bool useFarenheit;
};
#endif // TEMPREADER_H