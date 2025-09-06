#include "TempReader.h"
#include <SPI.h>
#include <Wire.h>
#include <cmath>  // for std::roundf

extern HWCDC USBSerial;
#include <Arduino.h>  // for size_t, etc.



/**
 * @brief   Computes the median of an array of floats (in-place sort).
 * @param   data    Pointer to the array of floats.
 * @param   length  Number of elements in the array.
 * @return  The median value.
 */
 float TempReader::computeMedianFloat(float* data, size_t length) {
    if (length == 0) return 0.0f;
    // Simple O(n^2) sort (selection sort)
    for (size_t i = 0; i + 1 < length; ++i) {
        size_t minIdx = i;
        for (size_t j = i + 1; j < length; ++j) {
            if (data[j] < data[minIdx]) {
                minIdx = j;
            }
        }
        if (minIdx != i) {
            float tmp = data[i];
            data[i] = data[minIdx];
            data[minIdx] = tmp;
        }
    }
    // Compute median
    if (length & 1) {
        // odd
        return data[length / 2];
    } else {
        // even
        float a = data[(length / 2) - 1];
        float b = data[length / 2];
        return (a + b) / 2.0f;
    }
}

/**
 * @brief   Divides a 32×24 frame into 3 vertical sections and returns their medians.
 *
 * @param   frame           Pointer to a 32*24 array of floats (row-major: row*COLS + col).
 * @param   useMiddleRows   If true, only use the 3 middle rows (rows 10,11,12).
 *                         If false, use all 24 rows.
 * @param   medians_out     Caller-provided float[3] array; on return, medians_out[i]
 *                         is the median of section i (i=0:left,1:center,2:right).
 */
void TempReader::getSectionMedians(const float frame[PIXEL_COUNT],
                       bool useMiddleRows,
                       float medians_out[3], int leftOffset, int rightOffset)
{
    // Allocate a temporary buffer large enough for the largest section (11 cols × 24 rows = 264)
    float temp[COLS * ROWS];
    size_t sectionCols, rowStart, rowEnd, count;

    // Determine which rows to use
    if (useMiddleRows) {
        rowStart = (ROWS - 3) / 2;      // = (24 - 3)/2 = 10
        rowEnd   = rowStart + 3;       // = 10 + 3 = 13 (exclusive)
    } else {
        rowStart = 0;
        rowEnd   = ROWS;               // = 24 (exclusive)
    }

    int colsInRange = COLS - (leftOffset+rightOffset);

    // For each section 0,1,2:
    for (int section = 0; section < 3; ++section) {
        // Calculate column range: [startCol, endCol)
        int startCol = ((section * colsInRange) / 3)+leftOffset;         // section*32/3
        int   endCol = (((section + 1) * colsInRange) / 3)+leftOffset;   // (section+1)*32/3

        sectionCols = endCol - startCol;             // e.g. 10, 11, or 11

        // Gather all values in this section into temp[]
        count = 0;
        for (int r = rowStart; r < rowEnd; ++r) {
            int baseIdx = r * COLS + startCol;
            for (int c = 0; c < (int)sectionCols; ++c) {
                temp[count++] = frame[baseIdx + c];
            }
        }

        // Compute median on the collected values
        medians_out[section] = computeMedianFloat(temp, count);
    }
}


TempReader::TempReader() : sensorIndices{0, 7, 3, 4}{
    for (uint8_t i = 0; i < TIRE_COUNT; i++){
        resetTireSensor(i);
        for(int j=0; j<3; j++){
            tireTemps[i] = 0;
            tireSectionTemps[i][j]=0;
            lastTireSectionTemps[i][j]=0;
        }
    }
    
}

void TempReader::resetTireSensor(int i){
    tireSensorIsCamera[i]=true; 
    tireSensorClockSpeed[i] = MAX_CLOCK_SPEED;
    tireSensorBegun[i] = -5;
}

bool TempReader::newTempIsInvalid(int i, int j){
 //return (lastTireSectionTemps[i][j] !=0 && abs(tireSectionTemps[i][j]-lastTireSectionTemps[i][j]) > 50) || (lastTireSectionTemps[i][j] ==0 &&  (tireSectionTemps[i][j] >=200 || tireSectionTemps[i][j] <0));

  // Read current/last using your existing indices i,j
  const float curr = tireSectionTemps[i][j];
  const float last = lastTireSectionTemps[i][j];

  // --- Tunables (keep simple) ---
  const float ABS_MIN   = -10.0f;   // physically impossible low
  const float ABS_MAX   = 270.0f;   // physically impossible high
  const float START_OK_MIN = 20.0f; // during startup, only reject crazy values
  const float START_OK_MAX = 190.0f;
  const float MAX_STEP  = 30.0f;    // max believable jump per frame

  // 1) Absolute sanity
  if (!isfinite(curr) || curr < ABS_MIN || curr > ABS_MAX) return true;

  // 2) If last is uninitialized (your code uses 0 as "none"),
  //    don't be picky—just reject only extreme startup garbage.
  if (last == 0.0f) {
    return !(curr >= START_OK_MIN && curr <= START_OK_MAX);
  }

  // 3) If the *last* looked bogus but the new value looks mid-range sane,
  //    allow an immediate re-sync (prevents getting stuck using a bad last).
  if ((last < START_OK_MIN || last > START_OK_MAX) &&
      (curr >= START_OK_MIN && curr <= START_OK_MAX)) {
    return false; // accept to recover immediately
  }

  // 4) Normal step check vs last accepted
  if (fabsf(curr - last) > MAX_STEP){
    for (int tj; tj<3; tj++){
        if (tj!=j){
            float tjCurr = tireSectionTemps[i][tj];
            if (tjCurr!=0.0 && fabsf(curr - tjCurr) <= MAX_STEP)
                return false;
        }
    }
        return true;
  } 

  // Passed all guards → valid
  return false;


}

void TempReader::readTemps(){
    for (uint8_t i = 0; i < TIRE_COUNT; i++)
    {
        int busIndex = sensorIndices[i];
        int result = select_I2C_bus(busIndex);  
        if (result ==0){
            checkTireSensor(i);
            if (tireSensorIsCamera[i]){
                if(readFrame(i)){
                    fillTireFrame(i);
                    getSectionMedians(frame, true, tireSectionTemps[i], leftPixelOffset[i], rightPixelOffset[i]);
                    for(int j=0; j<3; j++){
                        float valueF = tireSectionTemps[i][j] * 9.0f / 5.0f + 32.0f;
                        if (useFarenheit)
                        tireSectionTemps[i][j] = valueF;
                        if (newTempIsInvalid(i,j))
                            tireSectionTemps[i][j] = lastTireSectionTemps[i][j];
                        else
                            lastTireSectionTemps[i][j] = tireSectionTemps[i][j];
                        //  USBSerial.print("|");
                        //  USBSerial.print(valueF);
                        //     USBSerial.print("F");
                    }    
                    // USBSerial.println("|");            
                }
            }else{
                float temp = 0.0;
                if (tireSensorBegun[i]==1){
                    temp = getTemp(i, useFarenheit);                   
                    USBSerial.print(i);
                    USBSerial.print(": Temp Read: ");
                    USBSerial.println(temp);                
                    tireTemps[i] = temp;
                    tireSectionTemps[i][0] = temp;
                    tireSectionTemps[i][1] = 0.0;
                    tireSectionTemps[i][2] = 0.0;
                }
            }
        }
    }
}

void TempReader::fillTireFrame(int n) {
        if (n < 0 || n >= TIRE_COUNT) {
            // Out‐of‐range index – bail or handle error as you choose
            return;
        }
        for (int i = 0; i < FRAME_PIXELS; ++i) {
            float valueC = frame[i];
            // if (isFahrenheit) {
            //     // Convert Celsius → Fahrenheit: F = C * 9/5 + 32
            //     float valueF = valueC * 9.0f / 5.0f + 32.0f;
            //     // Round to nearest integer (you can also use static_cast<int>(valueF) if truncation is acceptable)
            //     tire_frames[n][i] = static_cast<int>(std::roundf(valueF));
            // } else {
                // Directly truncate/round the Celsius float to int
                // tire_frames[n][i] = static_cast<int>(std::roundf(valueC));
                tire_frames[n][i] = (int)valueC;
            // }
        }
    }

void TempReader::checkTireSensor(uint8_t index){
    int cameraClockSpeed = (autoAdjustClock)? tireSensorClockSpeed[index] : MAX_CLOCK_SPEED;
    if (tireSensorBegun[index]<1){
            USBSerial.print("Attempt Adafruit MLX90640 Camera Begin: ");
            USBSerial.println(index);
            
            Wire.setClock(cameraClockSpeed); 
            tireSensorIsCamera[index]=true;
            if (!mlx_a[index].begin(MLX90640_I2CADDR_DEFAULT, &Wire)) {

                USBSerial.print("MLX90640 not found at index: ");
                USBSerial.println(index);

                if (autoAdjustClock){
                    if (tireSensorBegun[index]<0)
                        tireSensorBegun[index]++;
                    else{
                        if (cameraClockSpeed > MIN_CLOCK_SPEED){
                            cameraClockSpeed-=MLX0_CLOCK_SPEED;
                            USBSerial.print("Reducing Clock Speed to: ");
                            USBSerial.println(cameraClockSpeed);
                            tireSensorClockSpeed[index] = cameraClockSpeed;
                            tireSensorBegun[index] = -1;
                        }
                        else
                            tireSensorBegun[index] = 2;                   
                    }
                }

                USBSerial.print("Attempt MLX_0 Begin: ");
                USBSerial.println(index);
                Wire.setClock(MLX0_CLOCK_SPEED);
                mlx_0[index].begin();
                int tempTemp = (int)getTemp(index,true);
                if (tempTemp != 0){
                    tireSensorBegun[index]=1;
                    tireSensorIsCamera[index]=false;
                    USBSerial.print("MLX_0 Begun: ");
                    USBSerial.print(index);
                    USBSerial.print(" with Temp: ");
                    USBSerial.println(tempTemp);
                }
            }else{            
                mlx_a[index].setMode(MLX90640_CHESS);
                mlx_a[index].setResolution(MLX90640_ADC_18BIT);
                mlx_a[index].setRefreshRate(MLX90640_16_HZ);
                USBSerial.print("Adafruit MLX90640 Camera Begun: ");
                USBSerial.println(index);
                tireSensorBegun[index]=1;
                tireSensorIsCamera[index]=true;
            }
    }else{
        if(tireSensorIsCamera[index]==false){
            Wire.setClock(MLX0_CLOCK_SPEED);
            mlx_0[index].begin();
        }
        else
            Wire.setClock(cameraClockSpeed); 
        if (autoRecoverTire && tireSensorBegun[index] > 1){
            tireSensorBegun[index]++;
            if (tireSensorBegun[index] >=60)
                resetTireSensor(index);
        }
    }
}

void TempReader::setup(){
    // USBSerial.println("mlx.begin");
    // mlx_0.begin();  
    // USBSerial.println("mlx.begun");




}

float TempReader::celsiusToFahrenheit(float c) { 
    return c * 9.0f / 5.0f + 32.0f;
     }

bool TempReader::readFrame(uint8_t index){

    if (mlx_a[index].getFrame(frame) != 0) {
        USBSerial.println("Failed to read MLX frame");            
        return false;
    } else{
        //USBSerial.println("Succeeded to read MLX frame");  
        flipFrameHorizontal(frame);
    }
    return true;
}

void TempReader::flipFrameHorizontal(float frame[FRAME_PIXELS]) {
  for (int r = 0; r < ROWS; ++r) {
    int base = r * COLS;
    // swap columns c <-> (COLS-1-c)
    for (int c = 0; c < COLS/2; ++c) {
      int i1 = base + c;
      int i2 = base + (COLS - 1 - c);
      std::swap(frame[i1], frame[i2]);
    }
  }
}

float TempReader::getTemp(uint8_t index, bool farenheit){
    float temp;
    if (farenheit)
        temp= mlx_0[index].readObjectTempF();
    else
        temp= mlx_0[index].readObjectTempC(); 
    if (isnan(temp))
        temp = 0.0f;   
    return temp;
}

int TempReader::select_I2C_bus(uint8_t bus){
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
    delayMicroseconds(200);        // give the mux time to settle 
    return result;
}