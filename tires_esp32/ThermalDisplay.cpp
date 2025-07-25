#include "ThermalDisplay.h"
#include <esp_heap_caps.h>   // for heap_caps_malloc (PSRAM allocation)
#include <Arduino.h>         // for yield()
#include "TempReader.h"

extern HWCDC USBSerial;

// Define the static 256-entry color palette (RGB565)
const uint16_t ThermalDisplay::camColors[256] = {
  0x480F, 0x400F, 0x400F, 0x400F, 0x4010, 0x3810, 0x3810, 0x3810, 0x3810, 0x3010,
  0x3010, 0x3010, 0x2810, 0x2810, 0x2810, 0x2810, 0x2010, 0x2010, 0x2010, 0x1810,
  0x1810, 0x1811, 0x1811, 0x1011, 0x1011, 0x1011, 0x0811, 0x0811, 0x0811, 0x0011,
  0x0011, 0x0011, 0x0011, 0x0011, 0x0031, 0x0031, 0x0051, 0x0072, 0x0072, 0x0092,
  0x00B2, 0x00B2, 0x00D2, 0x00F2, 0x00F2, 0x0112, 0x0132, 0x0152, 0x0152, 0x0172,
  0x0192, 0x0192, 0x01B2, 0x01D2, 0x01F3, 0x01F3, 0x0213, 0x0233, 0x0253, 0x0253,
  0x0273, 0x0293, 0x02B3, 0x02D3, 0x02D3, 0x02F3, 0x0313, 0x0333, 0x0333, 0x0353,
  0x0373, 0x0394, 0x03B4, 0x03D4, 0x03D4, 0x03F4, 0x0414, 0x0434, 0x0454, 0x0474,
  0x0474, 0x0494, 0x04B4, 0x04D4, 0x04F4, 0x0514, 0x0534, 0x0534, 0x0554, 0x0554,
  0x0574, 0x0574, 0x0573, 0x0573, 0x0573, 0x0572, 0x0572, 0x0572, 0x0571, 0x0591,
  0x0591, 0x0590, 0x0590, 0x058F, 0x058F, 0x058F, 0x058E, 0x05AE, 0x05AE, 0x05AD,
  0x05AD, 0x05AD, 0x05AC, 0x05AC, 0x05AB, 0x05CB, 0x05CB, 0x05CA, 0x05CA, 0x05CA,
  0x05C9, 0x05C9, 0x05C8, 0x05E8, 0x05E8, 0x05E7, 0x05E7, 0x05E6, 0x05E6, 0x05E6,
  0x05E5, 0x05E5, 0x0604, 0x0604, 0x0604, 0x0603, 0x0603, 0x0602, 0x0602, 0x0601,
  0x0621, 0x0621, 0x0620, 0x0620, 0x0620, 0x0620, 0x0E20, 0x0E20, 0x0E40, 0x1640,
  0x1640, 0x1E40, 0x1E40, 0x2640, 0x2640, 0x2E40, 0x2E60, 0x3660, 0x3660, 0x3E60,
  0x3E60, 0x3E60, 0x4660, 0x4660, 0x4E60, 0x4E80, 0x5680, 0x5680, 0x5E80, 0x5E80,
  0x6680, 0x6680, 0x6E80, 0x6EA0, 0x76A0, 0x76A0, 0x7EA0, 0x7EA0, 0x86A0, 0x86A0,
  0x8EA0, 0x8EC0, 0x96C0, 0x96C0, 0x9EC0, 0x9EC0, 0xA6C0, 0xAEC0, 0xAEC0, 0xB6E0,
  0xB6E0, 0xBEE0, 0xBEE0, 0xC6E0, 0xC6E0, 0xCEE0, 0xCEE0, 0xD6E0, 0xD700, 0xDF00,
  0xDEE0, 0xDEC0, 0xDEA0, 0xDE80, 0xDE80, 0xE660, 0xE640, 0xE620, 0xE600, 0xE5E0,
  0xE5C0, 0xE5A0, 0xE580, 0xE560, 0xE540, 0xE520, 0xE500, 0xE4E0, 0xE4C0, 0xE4A0,
  0xE480, 0xE460, 0xEC40, 0xEC20, 0xEC00, 0xEBE0, 0xEBC0, 0xEBA0, 0xEB80, 0xEB60,
  0xEB40, 0xEB20, 0xEB00, 0xEAE0, 0xEAC0, 0xEAA0, 0xEA80, 0xEA60, 0xEA40, 0xF220,
  0xF200, 0xF1E0, 0xF1C0, 0xF1A0, 0xF180, 0xF160, 0xF140, 0xF100, 0xF0E0, 0xF0C0,
  0xF0A0, 0xF080, 0xF060, 0xF040, 0xF020, 0xF800
};

// ─── DEFINE & INITIALIZE STATICS ──────────────────────
// You can tweak these defaults, or rely on setTemperatureRange()
int ThermalDisplay::thresholdMin     = MINTEMP;
int ThermalDisplay::thresholdIdeal   = (MINTEMP + MAXTEMP) / 2;
int ThermalDisplay::thresholdMax     = MAXTEMP;

bool ThermalDisplay::useGradient = true;
bool ThermalDisplay::showPixelOffsets = false;

uint16_t ThermalDisplay::camPalette[256];
int      ThermalDisplay::lenCold;
int      ThermalDisplay::lenWarm;
int      ThermalDisplay::lenIdeal;
int      ThermalDisplay::lenHot;

// define+zero-init the static pointer:
uint16_t* ThermalDisplay::framebuf = nullptr;
TempReader* ThermalDisplay::tempReader = nullptr;

ThermalDisplay::ThermalDisplay(Adafruit_ST7789 &displayTFT,
                               int areaX, int areaY,
                               int areaW, int areaH)
  : tft(displayTFT),
    areaX(areaX), areaY(areaY), areaW(areaW), areaH(areaH)
{
    if (!framebuf){
        // Attempt to allocate frame buffer in PSRAM
        framebuf = (uint16_t *)heap_caps_malloc(
            areaW * areaH * sizeof(uint16_t),
            MALLOC_CAP_SPIRAM
        );
        if (!framebuf) {
            // Fall back to normal heap
            framebuf = (uint16_t *)malloc(areaW * areaH * sizeof(uint16_t));
        }
        if (!framebuf) {
            // If allocation fails, halt
            while (true) { delay(1000); }
        }
    }


}



ThermalDisplay::~ThermalDisplay() {
    if (framebuf) {
        free(framebuf);
        framebuf = nullptr;
    }
}

float ThermalDisplay::fahrenheitToCelsius(float f){
    return (f - 32.0f) * 5.0f / 9.0f;
}

// ─── STATIC THRESHOLD SETTER ──────────────────────────
void ThermalDisplay::setTemperatureRangeC(int minTemp,
                                         int idealTemp,
                                         int maxTemp)
{
    thresholdMin   = constrain(minTemp,   MINTEMP,   MAXTEMP);
    thresholdIdeal = constrain(idealTemp, thresholdMin, MAXTEMP);
    thresholdMax   = constrain(maxTemp,   thresholdIdeal, MAXTEMP);
    generatePalette();
}

void ThermalDisplay::setTemperatureRangeF(int minTemp,
                                         int idealTemp,
                                         int maxTemp)
{
    int minTempC = (int)fahrenheitToCelsius(minTemp);
    int idealTempC = (int)fahrenheitToCelsius(idealTemp);
    int maxTempC = (int)fahrenheitToCelsius(maxTemp);
    setTemperatureRangeC(minTempC, idealTempC, maxTempC);
}

/*
// ─── STATIC MAPPING HELPER ────────────────────────────
uint8_t ThermalDisplay::getColorIndexForTemp(int celsius) {
    // clamp within our class‐wide bounds
    celsius = constrain(celsius, thresholdMin, thresholdMax);

    // first half of palette = [thresholdMin…thresholdIdeal]
    if (celsius <= thresholdIdeal) {
        int upper = max(thresholdIdeal, thresholdMin + 1);
        int idx = map(celsius, thresholdMin, upper, 1, 127);
        return constrain(idx, 0, 127);
    }
    // second half        = (thresholdIdeal…thresholdMax]
    else {
        int lower = min(thresholdIdeal, thresholdMax - 1);
        int idx   = map(celsius, lower, thresholdMax, 128, 254);
        return constrain(idx, 128, 255);
    }
}
    */
uint8_t ThermalDisplay::getColorIndexForTemp(int c) {
    // clamp entire range: [min-7 … max+7]
    int lo = thresholdMin - 7;
    int hi = thresholdMax + 7;
    c = constrain(c, lo, hi);

    // pick which segment, then local map
    if (c <= thresholdMin) {
        // cold segment
        return map(c, lo, thresholdMin, 0, lenCold - 1);
    }
    else if (c <= thresholdIdeal) {
        // warm segment
        int start = lenCold;
        return start + map(c, thresholdMin, thresholdIdeal,
                           0, lenWarm - 1);
    }
    else if (c <= thresholdMax) {
        // ideal segment
        int start = lenCold + lenWarm;
        return start + map(c, thresholdIdeal, thresholdMax,
                           0, lenIdeal - 1);
    }
    else {
        // hot segment
        int start = lenCold + lenWarm + lenIdeal;
        return start + map(c, thresholdMax, hi,
                           0, lenHot - 1);
    }
}


void ThermalDisplay::generatePalette() {
    // 1/8th for “hot” (red→white) = 32 entries
    lenHot = 256 / 8;       // = 32
    int rest = 256 - lenHot; // = 224

    // degree spans
    int spanCold = 7;                         // fixed
    int spanWarm = thresholdIdeal - thresholdMin;
    int spanIdeal= thresholdMax   - thresholdIdeal;
    int sumRest = spanCold + spanWarm + spanIdeal;

    // allocate the other three segment lengths proportionally
    lenCold  = max(1, int(round((float)spanCold  / sumRest * rest)));
    lenWarm  = max(1, int(round((float)spanWarm  / sumRest * rest)));
    // ensure we fill exactly 'rest'
    lenIdeal = rest - lenCold - lenWarm;

    // now carve into camPalette[]
    int idx = 0;

    // ── cold: violet → cyan
    for (int i = 0; i < lenCold; ++i) {
        float t = float(i) / (lenCold - 1);
        if (useGradient)
            camPalette[idx++] = interpolate565(COLOR_COLD_START, COLOR_CYAN, t);
        else
            camPalette[idx++] = COLOR_COLD;
    }
    // ── warm: cyan → yellow-orange
    for (int i = 0; i < lenWarm; ++i) {
        float t = float(i) / (lenWarm - 1);
        if (useGradient)
            camPalette[idx++] = interpolate565(COLOR_CYAN, COLOR_ORANGE, t);// COLOR_YELLOW_ORANGE, t);
        else
            camPalette[idx++] = COLOR_WARM;
    }
    // ── ideal: yellow-orange → red
    int lastI = 0;
    int halfIdeal = lenIdeal/2;
    for (int i = 0; i < halfIdeal; ++i) {
        float t = float(i) / (halfIdeal - 1);
        if (useGradient)
            camPalette[idx++] = interpolate565(COLOR_ORANGE, COLOR_PURPLE, t);//interpolate565(COLOR_YELLOW_ORANGE, COLOR_RED, t);
        else
            camPalette[idx++] = COLOR_PURPLE;//COLOR_IDEAL;
    }

    halfIdeal = lenIdeal-halfIdeal;
    for (int i =0; i < halfIdeal; ++i) {
        float t = float(i) / (halfIdeal - 1);
        if (useGradient)
            camPalette[idx++] = interpolate565(COLOR_PURPLE, COLOR_PURPLE_HOT, t);//interpolate565(COLOR_YELLOW_ORANGE, COLOR_RED, t);
        else
            camPalette[idx++] = COLOR_PURPLE;//COLOR_IDEAL;
    }


    // ── hot: red → white (always 32 entries)
    for (int i = 0; i < lenHot; ++i) {
        float t = float(i) / (lenHot - 1);
        if (useGradient)
            camPalette[idx++] = interpolate565(COLOR_PURPLE_HOT, COLOR_HOT, t); //interpolate565(COLOR_RED, COLOR_WHITE, t);
        else
            camPalette[idx++] = COLOR_HOT;
    }
}

uint16_t ThermalDisplay::interpolate565(uint16_t c1,
                                        uint16_t c2,
                                        float t)
{
    // extract 5/6/5 bits
    int r1 = (c1 >> 11) & 0x1F,  g1 = (c1 >> 5) & 0x3F,  b1 = c1 & 0x1F;
    int r2 = (c2 >> 11) & 0x1F,  g2 = (c2 >> 5) & 0x3F,  b2 = c2 & 0x1F;
    // linear interp
    int r = r1 + (r2 - r1) * t + 0.5f;
    int g = g1 + (g2 - g1) * t + 0.5f;
    int b = b1 + (b2 - b1) * t + 0.5f;
    // re-pack
    return (uint16_t)((r << 11) | (g << 5) | b);
}


void ThermalDisplay::setTempIndex(int _tempIndex){                                        
        tempIndex = _tempIndex;
    }

void ThermalDisplay::updateDisplay(){
   updateDisplay(tempIndex);
}

void ThermalDisplay::updateDisplay(int _tempIndex){
     if (isActive && tempReader->tireSensorIsCamera[_tempIndex])
        updateDisplay(tempReader->tire_frames[_tempIndex], _tempIndex);
}

void ThermalDisplay::updateDisplay(const int temps[CAMERA_WIDTH * CAMERA_HEIGHT], int _tempIndex)
{
    // Build the areaW×areaH RGB565 buffer from 32×24 temps[]
    for (int camY = 0; camY < CAMERA_HEIGHT; camY++) {
        for (int camX = 0; camX < CAMERA_WIDTH; camX++) {
            int idxFlat = camY * CAMERA_WIDTH + camX;

            int raw = temps[idxFlat];
            // clamp using our static min/max
            //raw = constrain(raw, thresholdMin, thresholdMax);

            uint8_t ci      = getColorIndexForTemp(raw);
            uint16_t color  = camPalette[ci];//camColors[ci];

            /*
            int tempVal = temps[idxFlat];

            int celsius = tempVal;

            // Clamp to [MINTEMP, MAXTEMP]
            celsius = min(celsius, MAXTEMP);
            celsius = max(celsius, MINTEMP);

            // **replace** the old full‐range mapping:
            //   uint8_t colorIndex = map(celsius, MINTEMP, MAXTEMP, 0, 255);
            //   colorIndex = constrain(colorIndex, 0, 255);
            //   uint16_t color = camColors[colorIndex];
            //
            // **with** our bucketed helper:
            uint8_t colorIndex = getColorIndexForTemp(celsius);
            uint16_t color     = camColors[colorIndex];
            */

            // Compute scaled block in areaW×areaH
            int xStart = (camX     * areaW) / CAMERA_WIDTH;
            int xEnd   = ((camX+1) * areaW) / CAMERA_WIDTH;
            int yStart = (camY     * areaH) / CAMERA_HEIGHT;
            int yEnd   = ((camY+1) * areaH) / CAMERA_HEIGHT;

            for (int yy = yStart; yy < yEnd; yy++) {
                for (int xx = xStart; xx < xEnd; xx++) {
                    framebuf[yy * areaW + xx] = color;
                }
                // Yield to avoid watchdog reset
                //yield();
            }
        }
    }

    // Push the entire buffer to ST7789 at (areaX, areaY)
    tft.startWrite();
    tft.setAddrWindow(areaX, areaY, areaW, areaH);
    tft.writePixels(framebuf, areaW * areaH);
    tft.endWrite();

    if (showPixelOffsets)
        drawPixelOffsets(_tempIndex);
}

void ThermalDisplay::drawPixelOffsets(int _tempIndex){
    byte leftOffset = tempReader->leftPixelOffset[_tempIndex];
    byte rightOffset = tempReader->rightPixelOffset[_tempIndex];

     USBSerial.print(_tempIndex);
     USBSerial.print(": leftOffset: ");
     USBSerial.println(leftOffset);


    if(leftOffset >0){
        int leftX = (((leftOffset * areaW) / CAMERA_WIDTH)-1)+areaX;   
        USBSerial.print(_tempIndex);
        USBSerial.print(": leftX: ");
        USBSerial.println(leftX);     
        tft.drawFastVLine(leftX, areaY, areaH, OFFSET_LINE_COLOR);
    }

        USBSerial.print(_tempIndex);
        USBSerial.print(": rightOffset: ");
        USBSerial.println(rightOffset);
    if (rightOffset >0){
        int rightX = ((areaW- ((rightOffset * areaW) / CAMERA_WIDTH))+1)+areaX;
        USBSerial.print(_tempIndex);
        USBSerial.print(": rightX: ");
        USBSerial.println(rightX);
        tft.drawFastVLine(rightX, areaY, areaH, OFFSET_LINE_COLOR);
    }

}
