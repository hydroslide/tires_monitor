#include "TempReader.h"
#include <SPI.h>
#include <Wire.h>
#include <cmath>   // for std::roundf, atanf/tanf/sqrtf (lens correction)
#include <string.h>  // memset/memcpy for the dewarp table and scratch buffer

extern HWCDC USBSerial;
#include <Arduino.h>  // for size_t, etc.



// ---------------------------------------------------------------------------
//  Fisheye lens correction (#31)
// ---------------------------------------------------------------------------
// MODEL. An equidistant ("f-theta") fisheye: r = f * theta, where r is the radius from
// the image centre in pixels and theta the angle off the optical axis. The full field of
// view spans the 32-pixel WIDTH; the 24-pixel height is a plain centre crop of the same
// projection, one focal length shared by both axes. Correcting means re-projecting to a
// rectilinear (gnomonic) image, r = f * tan(theta) -- the projection in which a straight
// line in the world is a straight line in the frame.
//
//   f_src = W / F                 source fisheye, px per radian   ((W/2) = f_src * (F/2))
//   f_dst = (W/2) / tan(F/2)      destination rectilinear, px
//
// FRAMING. A rectilinear image cannot cover the same angles at the same scale -- that is
// the whole point -- so something has to give, and what we preserve is the HORIZONTAL
// field of view. At the left and right edges the scale works out to exactly 1.0, so the
// frame still views the angles it always did. That is not an aesthetic choice: the eight
// per-corner crop offsets are a hand-aimed calibration against those very edges (there is
// an entire interactive mode for setting them), and any other framing would silently
// invalidate all eight on every profile.
//
// THE COST, and the setting that answers it. Preserving horizontal means the corrected
// top and bottom rows want scene content from beyond what the sensor captured:
//
//   Fit to View OFF -- letterbox. Those pixels are marked INVALID and painted black.
//                      No data is shown where no data exists.
//   Fit to View ON  -- pre-scale dy by `a` <= 1 so the 24 output rows cover only angles
//                      the sensor actually saw. Aspect ratio goes; STRAIGHTNESS STAYS,
//                      because an anisotropic scale is affine and affine maps carry
//                      straight lines to straight lines. Every pixel is valid.
//
// Neither mode can move a measurement, and that is the claim that lets Fit to View be a
// casual toggle rather than a recalibration. Rows 10-12 -- the only rows
// getSectionMedians() reads -- sit at |dy| <= 1.5, where the vertical pre-scale barely
// registers: toggling the mode shifts where those rows sample from by 0.012 px at the
// 110 degree default and at most 0.071 px anywhere in the 60..140 range (the shift grows
// with FOV because a bigger squeeze means a bigger change in dy). A tenth of a pixel
// column is ~0.3% of the tread width and an order of magnitude under the sensor's own
// pixel-to-pixel noise, so it cannot reach a band median, let alone a verdict.
//
// The invalid set is empty on those rows too, and lensSelfTest() proves THAT half at boot
// rather than asking for trust -- it is the one that would fail loudly and silently skew
// the shoulder bands if the tolerance rule above were ever changed.

namespace {

// Fractional overshoot below which we clamp the sample into the source rather than call
// the pixel dead. Two populations need opposite treatment and a per-row rule cannot tell
// them apart: the top/bottom rows miss by 1-3 px (genuinely no data), while columns 0 and
// 31 of the MIDDLE rows miss by <= 0.25 px, which is inside the bilinear resampler's own
// error -- a rounding artifact, not missing data. Half a pixel is the line between them.
//
// This is exactly what keeps the measurement rows whole: mark those 0.25 px misses
// invalid and getSectionMedians() would take holes at columns 0 and 31, on the only rows
// it reads.
constexpr float LENS_TOL_PX = 0.5f;

constexpr int LENS_W = 32;
constexpr int LENS_H = 24;
constexpr int LENS_N = LENS_W * LENS_H;

// One resample instruction per output pixel: which 2x2 of the source to blend, and how.
// 4 bytes x 768 = 3 KB, static and shared by all four corners -- same lens, same geometry.
struct DewarpEntry {
    uint16_t idx0;   // source index of the 2x2 top-left: y0 * W + x0
    uint8_t  fx;     // x fraction, Q8 (0..255)
    uint8_t  fy;     // y fraction, Q8
};

DewarpEntry dewarpLut[LENS_N];
uint8_t     dewarpValid[(LENS_N + 7) / 8];

// What the table currently holds, so a rebuild happens on a real change and not per frame.
uint8_t lutFov   = 0;
bool    lutFit   = false;
bool    lutBuilt = false;

// Walk the output grid for a given vertical pre-scale `a`, returning the worst overshoot
// past the source rectangle in pixels. With commit=true it also writes the LUT and the
// validity bitmap; with commit=false it is the cheap probe the `a` solve bisects on.
float mapGrid(float f_src, float f_dst, float a, bool commit)
{
    const float cx = (LENS_W - 1) / 2.0f;
    const float cy = (LENS_H - 1) / 2.0f;
    float worst = 0.0f;

    if (commit) memset(dewarpValid, 0, sizeof(dewarpValid));

    for (int yo = 0; yo < LENS_H; ++yo) {
        for (int xo = 0; xo < LENS_W; ++xo) {
            const int i = yo * LENS_W + xo;

            const float dx  = xo - cx;
            const float dyr = (yo - cy) * a;          // Fit to View pre-scale; a=1 letterboxes
            const float rd  = sqrtf(dx * dx + dyr * dyr);
            // Where the fisheye put the ray this output pixel is looking along.
            // s is the source-radius / output-radius ratio; at rd -> 0 it is the limit
            // f_src/f_dst, which is also why the centre is de-magnified.
            const float s = (rd > 0.0001f) ? (f_src * atanf(rd / f_dst)) / rd
                                           : (f_src / f_dst);
            float xs = cx + dx  * s;
            float ys = cy + dyr * s;

            const float ov = fmaxf(fmaxf(-xs, xs - (LENS_W - 1)),
                                   fmaxf(-ys, ys - (LENS_H - 1)));
            if (ov > worst) worst = ov;
            if (!commit) continue;

            if (ov < LENS_TOL_PX) dewarpValid[i >> 3] |= (uint8_t)(1 << (i & 7));

            // Clamp regardless of validity. An invalid entry still needs a safe in-range
            // index so the per-frame loop can stay branch-free -- the bitmap, not the
            // value, is what decides whether anyone may USE the pixel.
            if (xs < 0.0f) xs = 0.0f;
            if (xs > LENS_W - 1) xs = LENS_W - 1;
            if (ys < 0.0f) ys = 0.0f;
            if (ys > LENS_H - 1) ys = LENS_H - 1;

            int x0 = (int)xs; if (x0 > LENS_W - 2) x0 = LENS_W - 2;
            int y0 = (int)ys; if (y0 > LENS_H - 2) y0 = LENS_H - 2;

            dewarpLut[i].idx0 = (uint16_t)(y0 * LENS_W + x0);
            dewarpLut[i].fx   = (uint8_t)((xs - x0) * 255.0f + 0.5f);
            dewarpLut[i].fy   = (uint8_t)((ys - y0) * 255.0f + 0.5f);
        }
    }
    return worst;
}

// Largest vertical pre-scale <= 1 that keeps every output pixel inside the source. Only
// meaningful for Fit to View; bisection is plenty since this runs on a settings change,
// never on the per-frame path.
float solveFitScale(float f_src, float f_dst)
{
    if (mapGrid(f_src, f_dst, 1.0f, false) < LENS_TOL_PX) return 1.0f;   // nothing to fix
    float lo = 0.1f, hi = 1.0f;
    for (int k = 0; k < 40; ++k) {
        const float mid = 0.5f * (lo + hi);
        if (mapGrid(f_src, f_dst, mid, false) < LENS_TOL_PX) lo = mid;
        else                                                 hi = mid;
    }
    return lo;
}

void lensFocals(uint8_t fovDegrees, float& f_src, float& f_dst)
{
    const float F = fovDegrees * 3.14159265358979f / 180.0f;
    f_src = (float)LENS_W / F;
    f_dst = ((float)LENS_W / 2.0f) / tanf(F / 2.0f);
}

}  // namespace

// Static lens configuration. Defaults match the compiled-in menu defaults; the menu
// re-pushes them through configureLens() at every menu close.
bool    TempReader::lensEnabled    = true;
uint8_t TempReader::lensFovDegrees = 110;
bool    TempReader::lensFitToView  = false;

namespace {

// Rebuild the table if -- and only if -- what it was built for has changed.
void ensureLensTable(uint8_t fov, bool fit)
{
    if (lutBuilt && lutFov == fov && lutFit == fit) return;
    float f_src, f_dst;
    lensFocals(fov, f_src, f_dst);
    const float a = fit ? solveFitScale(f_src, f_dst) : 1.0f;
    mapGrid(f_src, f_dst, a, true);
    lutFov   = fov;
    lutFit   = fit;
    lutBuilt = true;
}

}  // namespace

void TempReader::configureLens(bool enabled, uint8_t fovDegrees, bool fitToView)
{
    if (fovDegrees < LENS_FOV_MIN) fovDegrees = LENS_FOV_MIN;
    if (fovDegrees > LENS_FOV_MAX) fovDegrees = LENS_FOV_MAX;
    lensEnabled    = enabled;
    lensFovDegrees = fovDegrees;
    lensFitToView  = fitToView;
    // Build here rather than lazily on the first frame: the first read after a menu close
    // is already the busiest moment in the loop.
    if (enabled) ensureLensTable(lensFovDegrees, lensFitToView);
}

bool TempReader::lensPixelValid(int idx)
{
    if (!lensEnabled) return true;                 // raw frame: every pixel is real
    if (idx < 0 || idx >= LENS_N) return true;
    ensureLensTable(lensFovDegrees, lensFitToView);
    return (dewarpValid[idx >> 3] >> (idx & 7)) & 1;
}

void TempReader::dewarpFrame(float f[FRAME_PIXELS])
{
    if (!lensEnabled) return;
    ensureLensTable(lensFovDegrees, lensFitToView);

    // Not an in-place transform -- an output pixel reads source pixels that later output
    // pixels still need. Bilinear, not nearest: at 32x24 a nearest-neighbour resample
    // would quantise the grooves into staircases and throw away most of the benefit.
    static float scratch[LENS_N];
    for (int i = 0; i < LENS_N; ++i) {
        const DewarpEntry& e = dewarpLut[i];
        const float fx = e.fx * (1.0f / 255.0f);
        const float fy = e.fy * (1.0f / 255.0f);
        const float p00 = f[e.idx0],            p01 = f[e.idx0 + 1];
        const float p10 = f[e.idx0 + LENS_W],   p11 = f[e.idx0 + LENS_W + 1];
        const float top = p00 + fx * (p01 - p00);
        const float bot = p10 + fx * (p11 - p10);
        scratch[i] = top + fy * (bot - top);
    }
    memcpy(f, scratch, sizeof(scratch));
}

bool TempReader::lensSelfTest()
{
    // The claim: getSectionMedians()'s rows (10, 11, 12 -- see the useMiddleRows branch)
    // never contain an invalid pixel, at any field of view the menu can reach, in either
    // fit mode. If that ever stops holding, the medians start silently sampling fewer
    // columns on one side and the inflation/camber deltas skew with it.
    const uint8_t savedFov   = lensFovDegrees;
    const bool    savedFit   = lensFitToView;
    const bool    savedOn    = lensEnabled;
    lensEnabled = true;

    int worstFov = -1, worstRow = -1, worstCol = -1;
    for (uint8_t fov = LENS_FOV_MIN; fov <= LENS_FOV_MAX && worstFov < 0; ++fov) {
        for (int fit = 0; fit < 2 && worstFov < 0; ++fit) {
            ensureLensTable(fov, fit != 0);
            for (int r = 10; r <= 12 && worstFov < 0; ++r) {
                for (int c = 0; c < LENS_W; ++c) {
                    const int i = r * LENS_W + c;
                    if (!((dewarpValid[i >> 3] >> (i & 7)) & 1)) {
                        worstFov = fov; worstRow = r; worstCol = c;
                        break;
                    }
                }
            }
        }
    }

    lensEnabled = savedOn;
    ensureLensTable(savedFov, savedFit);
    lensFovDegrees = savedFov;
    lensFitToView  = savedFit;

    if (worstFov < 0) {
        USBSerial.println("Lens self-test PASS: measurement rows 10-12 valid across 60-140 deg, both fit modes");
        return true;
    }
    USBSerial.print("Lens self-test FAIL: invalid measurement pixel at fov=");
    USBSerial.print(worstFov);
    USBSerial.print(" row=");
    USBSerial.print(worstRow);
    USBSerial.print(" col=");
    USBSerial.println(worstCol);
    return false;
}

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

    // Distribute the remainder of an uneven column count symmetrically so the two
    // shoulder bands (left/right) stay equal width and only the center absorbs the
    // odd column. Integer division alone made the left band systematically narrowest
    // (e.g. 6/7/7 for 20 cols), which biased the alignment (O-I) metric.
    int base = colsInRange / 3;
    int rem  = colsInRange % 3;
    int bandWidths[3] = { base, base, base };
    if (rem == 1) {
        bandWidths[1] += 1;             // give the odd column to the center band
    } else if (rem == 2) {
        bandWidths[0] += 1;             // keep the two shoulders symmetric
        bandWidths[2] += 1;
    }

    // For each section 0,1,2:
    int startCol = leftOffset;
    for (int section = 0; section < 3; ++section) {
        sectionCols = bandWidths[section];

        // Gather all values in this section into temp[]
        //
        // Pixels the lens correction has no source data for are DROPPED rather than
        // averaged in (#31): they carry a clamped edge value that is not a reading, and
        // letting one vote would drag the median toward whatever happens to sit at the
        // frame's edge. The middle-rows path never actually loses one (lensSelfTest()
        // proves rows 10-12 stay whole across the whole FOV range), so in practice this
        // costs nothing -- but the full-frame path below it would, and a silent hole is
        // exactly the kind of thing that is invisible until a verdict is wrong.
        count = 0;
        for (int r = rowStart; r < rowEnd; ++r) {
            int baseIdx = r * COLS + startCol;
            for (int c = 0; c < (int)sectionCols; ++c) {
                const int idx = baseIdx + c;
                if (!lensPixelValid(idx)) continue;
                temp[count++] = frame[idx];
            }
        }

        // Compute median on the collected values
        medians_out[section] = computeMedianFloat(temp, count);

        startCol += sectionCols;
    }
}


TempReader::TempReader() : sensorIndices{0, 7, 3, 4}{
    for (uint8_t i = 0; i < TIRE_COUNT; i++){
        resetTireSensor(i);
        for(int j=0; j<3; j++){
            tireTemps[i] = 0;
            tireSectionTemps[i][j]=0;
            lastTireSectionTemps[i][j]=0;
            rawSectionTemps[i][j]=0;
            emaSectionTemps[i][j]=0;
            emaInit[i][j]=false;
        }
    }

}

// Calculated (surface->carcass) mode config, story 03. K is authored in degrees F;
// convert the offset delta to the working unit (a difference, so no +32 term).
void TempReader::configureCalculated(bool enabled, float tauSeconds, float offsetF){
    calculatedMode = enabled;
    calcTauSeconds = tauSeconds;
    calcOffsetWorking = useFarenheit ? offsetF : (offsetF * 5.0f / 9.0f);
}

// Advance the per-band EMA of the raw surface medians and, when calculated mode is
// active, fold EMA + K into the working temps (tireSectionTemps / tireTemps). Raw is
// stashed in rawSectionTemps for the separate diagnostic channel set. A value of 0 is
// the pipeline's "unread/invalid" sentinel, so those bands are skipped (no EMA, no
// offset) to avoid seeding the filter with a fake +K reading during warm-up.
void TempReader::updateCalculated(long dtMillis){
    float dt = dtMillis / 1000.0f;
    if (dt < 0.0f) dt = 0.0f;
    // First-order low-pass: alpha = dt / (tau + dt).
    float alpha = (calcTauSeconds > 0.0f) ? (dt / (calcTauSeconds + dt)) : 1.0f;
    for (int i = 0; i < TIRE_COUNT; i++){
        int bands = tireSensorIsCamera[i] ? 3 : 1; // point sensors carry band 0 only
        for (int j = 0; j < 3; j++){
            float raw = tireSectionTemps[i][j];
            rawSectionTemps[i][j] = raw;
            bool active = (j < bands) && (raw > 0.0f);
            if (active){
                if (!emaInit[i][j]){ emaSectionTemps[i][j] = raw; emaInit[i][j] = true; }
                else emaSectionTemps[i][j] += alpha * (raw - emaSectionTemps[i][j]);
                if (calculatedMode)
                    tireSectionTemps[i][j] = emaSectionTemps[i][j] + calcOffsetWorking;
            }
        }
        // Keep the single-value mirror consistent for non-camera tiles, which read
        // tireTemps[] directly rather than tireSectionTemps[][0].
        if (calculatedMode && !tireSensorIsCamera[i] && rawSectionTemps[i][0] > 0.0f)
            tireTemps[i] = tireSectionTemps[i][0];
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
    for (int tj = 0; tj<3; tj++){
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
                    // Convert ALL bands to the working unit FIRST, then validate. The
                    // validity filter's cross-band "rescue" compares a band against its
                    // siblings, so every band must already be in the same unit before any
                    // validation runs — otherwise an already-°F band was compared against
                    // sibling bands still in °C (~85-unit gap at operating temp), causing
                    // asymmetric band rejection and latched stale values during warm-up.
                    if (useFarenheit){
                        for(int j=0; j<3; j++)
                            tireSectionTemps[i][j] = tireSectionTemps[i][j] * 9.0f / 5.0f + 32.0f;
                    }
                    for(int j=0; j<3; j++){
                        if (newTempIsInvalid(i,j))
                            tireSectionTemps[i][j] = lastTireSectionTemps[i][j];
                        else
                            lastTireSectionTemps[i][j] = tireSectionTemps[i][j];
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
        // Geometry correction lands HERE, at the point of capture, so the section medians,
        // tire_frames[], the thermal image, NBP and balance all derive from a rectilinear
        // frame without any of them knowing (#31). After the flip, not before: the two
        // commute (both are symmetric about the same centre) but the frame should be in
        // display orientation whenever it is remapped, so what you aim at is what is
        // measured.
        dewarpFrame(frame);
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