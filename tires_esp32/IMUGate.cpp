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
// Leak rate for a neutral inflation reading, as a fraction of elapsed time (#21). Below 1
// on purpose: neutral is weaker evidence than a contrary reading, so it should walk a
// verdict back more slowly than the opposite condition would flip it.
static const float DECAY_K = 0.75f;

IMUGate::IMUGate()
: bus(nullptr), present(false), enabled(true), trackActive(false),
  orient(ORIENT_AUTO), thresholdG(0.35f), gateDwellMs(500), dwellMs(2500),
  dieTempC(0.0f), verticalAxis(2), lateralAxis(1),
  latG(0.0f), latInit(false), zoneMs(0), capturing(true)
{
  for (int i = 0; i < 3; i++) { accG[i] = 0.0f; gyrDps[i] = 0.0f; restBias[i] = 0.0f; }
  for (int t = 0; t < TIRE_SLOTS; t++) { inflScoreMs[t] = 0; tireCond[t] = 0; }
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
    for (int t = 0; t < TIRE_SLOTS; t++) { inflScoreMs[t] = 0; tireCond[t] = 0; }
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

  // Per-tire leaky evidence accumulator, advanced only on captured frames. Cornering
  // frames freeze every score, so a corner neither builds nor decays a verdict.
  //
  // #21 replaced a single global over/under pair with one SIGNED score per tire. The old
  // shape collapsed four corners to one majority vote before the timer ever ran, so a
  // single genuinely over-inflated tire was cancelled by its neighbours' neutral votes and
  // the verdict could never say WHICH corner was wrong -- the thing you actually want.
  if (capturing) {
    const long cap = (long)dwellMs * 2;   // clamp: 2x dwell of headroom each way
    for (int t = 0; t < TIRE_SLOTS; t++) {
      long s = inflScoreMs[t];
      if (tireCond[t] > 0) {
        s += dtMillis;                    // evidence for OVER
      } else if (tireCond[t] < 0) {
        s -= dtMillis;                    // evidence for UNDER, full rate -- an opposite
                                          // reading is real evidence, not just absence
      } else {
        // Neutral is weaker evidence than a contrary reading, so it only leaks the score
        // back toward zero at DECAY_K. Without a leak the verdict could never unlatch
        // within a session; without the asymmetry, neutral would flip it as fast as the
        // opposite condition and the latch would chatter.
        long d = (long)((float)dtMillis * DECAY_K);
        if (d < 1) d = 1;                 // always make progress, even at tiny dt
        if      (s > 0) { s -= d; if (s < 0) s = 0; }
        else if (s < 0) { s += d; if (s > 0) s = 0; }
      }
      if (s >  cap) s =  cap;
      if (s < -cap) s = -cap;
      inflScoreMs[t] = s;
    }
  }
}

IMUGate::Alert IMUGate::alertState(int tire) const {
  // The latch is a pure function of the score -- no separate latched flag to keep in sync.
  // Hysteresis comes from the 2x cap: a saturated tire has to spend ~1.3x the dwell in
  // neutral before it falls back under the line, so it cannot chatter at the boundary.
  if (tire < 0 || tire >= TIRE_SLOTS) return ALERT_NONE;
  long s = inflScoreMs[tire];
  if (s >=  (long)dwellMs) return ALERT_OVER;
  if (s <= -(long)dwellMs) return ALERT_UNDER;
  return ALERT_NONE;
}

IMUGate::Alert IMUGate::alertState() const {
  // Rolled-up verdict for the consumers that still want one number (NBP "Overall").
  // Majority of the per-tire latches; a tie is NONE.
  int over = 0, under = 0;
  for (int t = 0; t < TIRE_SLOTS; t++) {
    Alert a = alertState(t);
    if      (a == ALERT_OVER)  over++;
    else if (a == ALERT_UNDER) under++;
  }
  if (over > under)  return ALERT_OVER;
  if (under > over)  return ALERT_UNDER;
  return ALERT_NONE;
}

void IMUGate::feedCondition(int tire, int cond) {
  // Latest per-frame verdict for one corner; update() applies it (only while capturing).
  if (tire < 0 || tire >= TIRE_SLOTS) return;
  tireCond[tire] = (cond > 0) ? 1 : (cond < 0 ? -1 : 0);
}
