#ifndef TEMPREADER_H
#define TEMPREADER_H

#include <array>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_MLX90614.h>
#include <Adafruit_MLX90640.h>

#define TIRE_COUNT 4

class TempReader {
    private:
        static constexpr int ROWS = 24;
        static constexpr int COLS = 32;
        static constexpr int PIXEL_COUNT = ROWS * COLS;
        static constexpr int FRAME_PIXELS = COLS * ROWS;  
        float getTemp(uint8_t index, bool farenheit);
        void select_I2C_bus(uint8_t bus);
        Adafruit_MLX90614 mlx_0[TIRE_COUNT];
        Adafruit_MLX90640 mlx_a[TIRE_COUNT];
        float celsiusToFahrenheit(float c);
        void fillTireFrame(int n);
        void checkTireSensor(uint8_t index);
        float computeMedianFloat(float* data, size_t length);
        void getSectionMedians(const float frame[PIXEL_COUNT],
                       bool useMiddleRows,
                       float medians_out[3]);
        void flipFrameHorizontal(float frame[FRAME_PIXELS]);

    public:
        TempReader();        
        void setup();        
        float tireTemps[TIRE_COUNT];
        float tireSectionTemps[TIRE_COUNT][3];
        bool tireSensorIsCamera[TIRE_COUNT];
        int8_t tireSensorBegun[TIRE_COUNT];
        
        float frame[FRAME_PIXELS];                   // MLX90640 float-array
        int   tire_frames[TIRE_COUNT][FRAME_PIXELS]; // TIRE_COUNT × (32×24) integer arrays
        std::array<uint8_t, 4> sensorIndices;
        void readTemps();
        bool readFrame(uint8_t index);
        bool useFarenheit;
       
};
#endif // TEMPREADER_H