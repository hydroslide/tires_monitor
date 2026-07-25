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
        static constexpr int MAX_CLOCK_SPEED = 1000000; // 1MHz
        static constexpr int MIN_CLOCK_SPEED = 400000; // 600KHz
        static constexpr int MLX0_CLOCK_SPEED = 100000; // 100KHz
        static constexpr int ROWS = 24;
        static constexpr int COLS = 32;
        static constexpr int PIXEL_COUNT = ROWS * COLS;
        static constexpr int FRAME_PIXELS = COLS * ROWS;  
        float getTemp(uint8_t index, bool farenheit);
        int select_I2C_bus(uint8_t bus);
        Adafruit_MLX90614 mlx_0[TIRE_COUNT];
        Adafruit_MLX90640 mlx_a[TIRE_COUNT];
        int tireSensorClockSpeed[TIRE_COUNT];
        float celsiusToFahrenheit(float c);
        void fillTireFrame(int n);
        void checkTireSensor(uint8_t index);
        float computeMedianFloat(float* data, size_t length);
        void getSectionMedians(const float frame[PIXEL_COUNT],
                       bool useMiddleRows,
                       float medians_out[3], int leftOffset, int rightOffset);
        void flipFrameHorizontal(float frame[FRAME_PIXELS]);
        bool newTempIsInvalid(int i, int j);
        void resetTireSensor(int i);

        // Calculated-mode state (story 03).
        float calcTauSeconds = 15.0f;             // EMA time constant, seconds
        float calcOffsetWorking = 0.0f;           // K as a delta in the working unit
        float emaSectionTemps[TIRE_COUNT][3];     // smoothed raw surface per band
        bool  emaInit[TIRE_COUNT][3];             // seeded on first valid sample

    public:
        TempReader();        
        void setup();        
        float tireTemps[TIRE_COUNT];
        float tireSectionTemps[TIRE_COUNT][3];
        float lastTireSectionTemps[TIRE_COUNT][3];
        bool tireSensorIsCamera[TIRE_COUNT];
        bool autoAdjustClock;
        bool autoRecoverTire;
        int8_t tireSensorBegun[TIRE_COUNT];
        
        float frame[FRAME_PIXELS];                   // MLX90640 float-array
        int   tire_frames[TIRE_COUNT][FRAME_PIXELS]; // TIRE_COUNT × (32×24) integer arrays
        std::array<uint8_t, 4> sensorIndices;
        void readTemps();
        bool readFrame(uint8_t index);
        bool useFarenheit;
        byte leftPixelOffset[TIRE_COUNT];
        byte rightPixelOffset[TIRE_COUNT];

        // --- Calculated (surface->carcass) mode, story 03 ---
        // When calculatedMode is on, the working section temps (tireSectionTemps /
        // tireTemps) are replaced by EMA_tau(surface) + K so every downstream display
        // and decision runs on the smoothed carcass estimate. The untouched raw surface
        // medians are preserved here for a separate NBP channel set.
        float rawSectionTemps[TIRE_COUNT][3];
        bool  calculatedMode = false;
        // enabled: master switch (already ANDed with Track mode by the caller).
        // tauSeconds: EMA time constant. offsetF: K in degrees F (converted to the
        // working unit internally). tau/K are story-03 defaults; story 04 sources them
        // from the active tire profile.
        void  configureCalculated(bool enabled, float tauSeconds, float offsetF);
        // Advance the EMA and, when active, fold EMA+K into the working temps. Call once
        // per read (dtMillis = read cadence), AFTER readTemps() so the raw validity
        // filter (which keys off lastTireSectionTemps) stays on the raw signal.
        void  updateCalculated(long dtMillis);

};
#endif // TEMPREADER_H