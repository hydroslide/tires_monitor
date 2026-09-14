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
        // Re-project the raw fisheye frame to a rectilinear one, in place (#31). Called
        // from readFrame() straight after the flip, so every consumer downstream inherits
        // the correction without knowing it happened. No-op when correction is off.
        void dewarpFrame(float f[FRAME_PIXELS]);
        bool newTempIsInvalid(int i, int j);
        void resetTireSensor(int i);

        // Lens-correction config (#31). Static because initializeSystem() re-news the
        // TempReader on every menu close and the setting must not die with the instance --
        // nor must the 768-entry lookup table it drives get rebuilt that often.
        static bool    lensEnabled;
        static uint8_t lensFovDegrees;
        static bool    lensFitToView;

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

        // --- Fisheye lens correction (#31) ---
        //
        // The wide-angle MLX90640 is a ~110 degree lens, so straight lines in the world --
        // a tire's circumferential grooves -- bow outward in the raw frame. That is not
        // only a looks problem: getSectionMedians() slices the frame into three
        // FIXED-WIDTH column bands, and fixed columns are equal slices of TIRE only if the
        // projection is linear across the width. It is not, so the outer/inner comparison
        // behind the camber and inflation verdicts has been reading bands that do not match
        // the physical thirds of the tread.
        //
        // The frame is therefore re-projected the moment it lands, inside readFrame(),
        // before the medians, before fillTireFrame(), before the image. One insertion
        // point; everything downstream inherits it.
        //
        // Config and lookup table are STATIC on purpose: initializeSystem() deletes and
        // re-news the TempReader every single time the menu closes, and rebuilding a
        // 768-entry atan/sqrt map on each of those would be pure waste. The table rebuilds
        // only when the field of view or the fit mode actually changes.
        //
        // fovDegrees is the field of view across the 32-pixel WIDTH. Range is enforced
        // here as well as in the menu -- the map degenerates as it approaches 180 (the
        // rectilinear focal length goes to zero), so it must never be reachable.
        static void configureLens(bool enabled, uint8_t fovDegrees, bool fitToView);

        // False for a pixel the corrected frame has no source data for: with Fit to View
        // off, the top and bottom rows want scene content from beyond the sensor's
        // vertical extent, and there is nothing there to sample. Always true when
        // correction is off or Fit to View is on.
        //
        // An invalid pixel must never be presented as a temperature. getSectionMedians()
        // drops it from the sample set; ThermalDisplay paints it black rather than running
        // it through the palette (every int maps to a colour, so a fabricated value would
        // read as a real -- and possibly alarming -- reading). Callers outside those two
        // must check this before trusting frame[]/tire_frames[].
        static bool lensPixelValid(int idx);

        // Boot-time proof of the property the whole design rests on: no invalid pixel ever
        // lands in rows 10-12, the only rows getSectionMedians() reads, at ANY field of
        // view in range and in either fit mode. Logs a PASS/FAIL line. Cheap enough to run
        // unconditionally (81 degrees x 2 modes x 768 pixels of pure float maths, once).
        static bool lensSelfTest();

        static constexpr uint8_t LENS_FOV_MIN = 60;
        static constexpr uint8_t LENS_FOV_MAX = 140;

};
#endif // TEMPREADER_H