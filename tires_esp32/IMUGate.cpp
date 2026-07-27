#include "IMUGate.h"

// QMI8658C register map (subset we use).
static const uint8_t QMI_ADDR      = 0x6B;
static const uint8_t REG_WHO_AM_I  = 0x00;  // -> 0x05
static const uint8_t REG_CTRL1     = 0x02;  // serial IF: address auto-increment
static const uint8_t REG_CTRL2     = 0x03;  // accel full-scale + ODR
static const uint8_t REG_CTRL3     = 0x04;  // gyro full-scale + ODR
static const uint8_t REG_CTRL5     = 0x06;  // low-pass filters
static const uint8_t REG_CTRL7     = 0x08;  // sensor enable
static const uint8_t REG_RESET     = 0x60;  // soft reset
static const uint8_t REG_TEMP_L    = 0x33;  // die temperature (LSB)
static const uint8_t REG_AX_L      = 0x35;  // accel X..Z then gyro X..Z, LE 16-bit

static const uint8_t WHO_AM_I_VAL  = 0x05;

// CTRL2: accel FS = +/-4g (0b001<<4), ODR ~125 Hz (0b0110). CTRL3: gyro FS =
// +/-256 dps (0b100<<4), ODR ~125 Hz. Sensitivities follow from those choices.
static const uint8_t CTRL2_VAL     = 0x16;  // +/-4g, 125 Hz
static const uint8_t CTRL3_VAL     = 0x46;  // +/-256 dps, 125 Hz
static const uint8_t CTRL7_VAL     = 0x03;  // enable accel + gyro
static const float   ACC_LSB_PER_G = 8192.0f;   // +/-4g
static const float   GYR_LSB_PER_DPS = 128.0f;  // +/-256 dps

// Boot calibration: average this many samples while stationary.
static const int   CAL_SAMPLES = 64;
// Minimal smoothing on the lateral-g signal (stable dash mount): light EMA.
static const float LAT_EMA_ALPHA = 0.35f;

IMUGate::IMUGate()
: bus(nullptr), present(false), enabled(true), trackActive(false),
  orient(ORIENT_AUTO), thresholdG(0.35f), gateDwellMs(500), dwellMs(2500),
  dieTempC(0.0f), verticalAxis(2), lateralAxis(1),
  latG(0.0f), latInit(false), zoneMs(0), capturing(true),
  overAccumMs(0), underAccumMs(0), alert(ALERT_NONE), pendingCond(0)
{
  for (int i = 0; i < 3; i++) { accG[i] = 0.0f; gyrDps[i] = 0.0f; restBias[i] = 0.0f; }
}

bool IMUGate::writeReg(uint8_t reg, uint8_t val) {
  bus->beginTransmission(QMI_ADDR);
  bus->write(reg);
  bus->write(val);
  return bus->endTransmission() == 0;
}

uint8_t IMUGate::readReg(uint8_t reg) {
  uint8_t v = 0;
  readBytes(reg, &v, 1);
  return v;
}

bool IMUGate::readBytes(uint8_t reg, uint8_t* buf, uint8_t len) {
  bus->beginTransmission(QMI_ADDR);
  bus->write(reg);
  if (bus->endTransmission(false) != 0) return false;
  uint8_t got = bus->requestFrom((int)QMI_ADDR, (int)len);
  if (got != len) return false;
  for (uint8_t i = 0; i < len; i++) buf[i] = bus->read();
  return true;
}

bool IMUGate::begin(TwoWire &wire) {
  bus = &wire;
  present = false;

  if (readReg(REG_WHO_AM_I) != WHO_AM_I_VAL) {
    // One soft reset + retry in case the part came up mid-transaction.
    writeReg(REG_RESET, 0xB0);
    delay(15);
    if (readReg(REG_WHO_AM_I) != WHO_AM_I_VAL) {
      return false;
    }
  }

  writeReg(REG_CTRL1, 0x40);   // address auto-increment for burst reads
  writeReg(REG_CTRL2, CTRL2_VAL);
  writeReg(REG_CTRL3, CTRL3_VAL);
  writeReg(REG_CTRL5, 0x00);   // internal LPFs off -- we smooth ourselves
  writeReg(REG_CTRL7, CTRL7_VAL);
  delay(10);

  present = true;
  recalibrate();
  return true;
}

void IMUGate::applyConfig(bool en, float thr, unsigned long gateDwell,
                          unsigned long dwell, Orient o) {
  enabled = en;
  thresholdG = (thr > 0.01f) ? thr : 0.35f;
  // No floor on the capture dwell: 0 is a legitimate setting that reproduces the pre-#20
  // instant gate, which is what makes it A/B-able against a dwelled gate on the car.
  gateDwellMs = gateDwell;
  dwellMs = dwell;
  if (o != orient) {
    orient = o;
    resolveLateralAxis();
  } else {
    orient = o;
  }
}

int IMUGate::dominantAxis(const float v[3]) const {
  int idx = 0;
  float best = fabsf(v[0]);
  for (int i = 1; i < 3; i++) {
    if (fabsf(v[i]) > best) { best = fabsf(v[i]); idx = i; }
  }
  return idx;
}

void IMUGate::resolveLateralAxis() {
  if (orient == ORIENT_X)      { lateralAxis = 0; return; }
  if (orient == ORIENT_Y)      { lateralAxis = 1; return; }
  if (orient == ORIENT_Z)      { lateralAxis = 2; return; }
  // AUTO: pick a horizontal axis (not the gravity axis). Prefer Y, then X, then Z
  // so a level dash mount (gravity on Z) maps lateral -> Y by default.
  for (int cand = 1; cand >= 0; cand--) {         // try Y(1) then X(0)
    if (cand != verticalAxis) { lateralAxis = cand; return; }
  }
  lateralAxis = (verticalAxis == 2) ? 0 : 2;
}

void IMUGate::recalibrate() {
  if (!present) return;
  float sum[3] = {0.0f, 0.0f, 0.0f};
  int taken = 0;
  for (int n = 0; n < CAL_SAMPLES; n++) {
    if (readSample()) {
      for (int i = 0; i < 3; i++) sum[i] += accG[i];
      taken++;
    }
    delay(4);
  }
  if (taken > 0) {
    for (int i = 0; i < 3; i++) restBias[i] = sum[i] / (float)taken;
  }
  verticalAxis = dominantAxis(restBias);
  resolveLateralAxis();
  latG = 0.0f;
  latInit = false;
}

bool IMUGate::readSample() {
  if (!present) return false;
  uint8_t raw[12];
  if (!readBytes(REG_AX_L, raw, 12)) return false;
  for (int i = 0; i < 3; i++) {
    int16_t a = (int16_t)((raw[i * 2 + 1] << 8) | raw[i * 2]);
    accG[i] = (float)a / ACC_LSB_PER_G;
  }
  for (int i = 0; i < 3; i++) {
    int16_t g = (int16_t)((raw[6 + i * 2 + 1] << 8) | raw[6 + i * 2]);
    gyrDps[i] = (float)g / GYR_LSB_PER_DPS;
  }
  uint8_t t[2];
  if (readBytes(REG_TEMP_L, t, 2)) {
    int16_t traw = (int16_t)((t[1] << 8) | t[0]);
    dieTempC = (float)traw / 256.0f;
  }
  return true;
}

void IMUGate::update(long dtMillis, bool trackMode) {
  trackActive = trackMode;

  if (present) readSample();
  if (dtMillis < 0) dtMillis = 0;

  // Lateral g = horizontal-axis accel with the at-rest tilt/offset removed, then
  // lightly EMA-smoothed (stable mount => minimal smoothing).
  //
  // #20 moved this ABOVE the inert early-return. It used to run only in Track mode, so
  // latG was dead in Street and the g-bar had nothing to show -- but Street is exactly
  // where you want to eyeball calibration and mount noise before a session. Computing it
  // unconditionally changes no gate behavior: the inert branch still forces capturing.
  if (present) {
    float rawLat = accG[lateralAxis] - restBias[lateralAxis];
    if (!latInit) { latG = rawLat; latInit = true; }
    else          { latG = latG + LAT_EMA_ALPHA * (rawLat - latG); }
  }

  // Feature inert outside Track mode or when disabled: never suppress, hold clear.
  // latInit is deliberately NOT cleared here any more -- re-seeding the EMA on every
  // Street-mode tick would peg the bar to the raw signal and defeat the smoothing.
  if (!present || !enabled || !trackMode) {
    capturing = true;
    zoneMs = 0;
    overAccumMs = 0;
    underAccumMs = 0;
    alert = ALERT_NONE;
    pendingCond = 0;
    return;
  }

  // Capture requires the car to be in the zone AND to have HELD it for the capture dwell
  // (#20). Before this, capture flipped on the instant lateral g dropped below the
  // threshold, which let the tail of a corner -- still unwinding, load still shifting --
  // count as straight-line data. Leaving the zone resets the timer outright; there is no
  // partial credit for a brief dip below threshold mid-corner.
  bool zoneNow = (fabsf(latG) < thresholdG);
  zoneMs = zoneNow ? (zoneMs + (unsigned long)dtMillis) : 0;
  capturing = zoneNow && (zoneMs >= gateDwellMs);

  // Overall (not per-corner) time-in-over/under, accumulated only while capturing.
  // Cornering frames freeze the accumulators and hold the latch untouched.
  if (capturing) {
    if (pendingCond > 0) {
      overAccumMs += (unsigned long)dtMillis;
      underAccumMs = 0;
    } else if (pendingCond < 0) {
      underAccumMs += (unsigned long)dtMillis;
      overAccumMs = 0;
    } else {
      overAccumMs = 0;
      underAccumMs = 0;
    }

    // Debounced hysteresis latch: a condition sustained past the dwell latches the
    // alert; it stays latched (visible anywhere, even through neutral frames) until
    // the OPPOSITE condition sustains past the dwell and flips it.
    if (overAccumMs >= dwellMs) {
      alert = ALERT_OVER;
    } else if (underAccumMs >= dwellMs) {
      alert = ALERT_UNDER;
    }
  }
}

void IMUGate::feedCondition(int cond) {
  // Latest per-frame verdict; update() applies it (only while capturing in Track).
  pendingCond = (cond > 0) ? 1 : (cond < 0 ? -1 : 0);
}
