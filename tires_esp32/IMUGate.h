#ifndef IMU_GATE_H
#define IMU_GATE_H

#include <Arduino.h>
#include <Wire.h>

// IMUGate -- on-board QMI8658C (accel + gyro, I2C 0x6B) capture gate and latched
// over/under alert state machine for the tire-temp monitor (design 5.4, story 02).
//
// The core job: read the car's lateral g and decide whether we are "capturing"
// (car straight and steady) so inflation/segment reads are only accumulated on the
// straight, where the center-hot artifact is absent. It also runs a debounced,
// LATCHED over/under alert driven by an externally-supplied per-frame condition.
//
// Self-contained register-level driver on purpose: the pinned build profile ships
// no IMU library, so we talk to the QMI8658C directly over Wire to keep compiling.
class IMUGate {
public:
  // Lateral axis mapping. AUTO learns "level" (which axis is vertical) at boot and
  // maps lateral to a horizontal axis; the explicit values are a manual fallback.
  enum Orient { ORIENT_AUTO = 0, ORIENT_X = 1, ORIENT_Y = 2, ORIENT_Z = 3 };

  // Latched alert state. OVER = center-hot (over-inflation) sustained past the dwell.
  enum Alert { ALERT_NONE = 0, ALERT_OVER = 1, ALERT_UNDER = 2 };

  IMUGate();

  // Initialize the QMI8658C and run a one-shot orientation auto-calibration
  // (learn the at-rest gravity vector; the car should be stationary at boot).
  // Returns true if the chip answered on I2C.
  bool begin(TwoWire &wire = Wire);

  // Push menu-configured settings. thresholdG in g, both dwells in milliseconds.
  // gateDwellMs = how long lateral g must stay inside the zone before we capture (0 =
  // instant). dwellMs = how long an over/under condition must persist on captured frames
  // before the alert latches. They are independent knobs; see IMUGate.cpp update().
  void applyConfig(bool enabled, float thresholdG, unsigned long gateDwellMs,
                   unsigned long dwellMs, Orient orient);

  // Re-run the at-rest orientation calibration (car must be stationary/level).
  void recalibrate();

  // Sample the IMU and advance the gate + latch. dtMillis is the loop delta;
  // trackMode gates the whole feature -- in Street mode the gate never suppresses
  // and the latch is held clear, so upstream behavior is unchanged.
  void update(long dtMillis, bool trackMode);

  // Per captured-frame condition for the latch: +1 = over, -1 = under, 0 = ok.
  // Ignored while cornering (not capturing) or outside Track mode.
  void feedCondition(int cond);

  bool  isPresent()   const { return present; }
  bool  isEnabled()   const { return enabled; }
  bool  isCapturing() const { return capturing; }   // true => accumulate reads
  bool  isCornering() const { return present && enabled && trackActive && !capturing; }
  float lateralG()    const { return latG; }        // smoothed, gravity-removed
  Alert alertState()  const { return alert; }

  // Gate internals, exposed for the on-screen test bar (#20). inZone() is the raw
  // threshold test; isCapturing() is inZone() AND the capture dwell already served, so
  // the two disagree exactly during the dwell window -- which is the state the bar paints
  // yellow. Note isCapturing() is forced true when the feature is inert (Street mode,
  // gate disabled, no IMU), where inZone() still reports the honest lateral-g answer.
  bool  inZone()      const { return present && (fabsf(latG) < thresholdG); }
  float thresholdGate() const { return thresholdG; }    // g
  unsigned long zoneDwellMs() const { return zoneMs; }  // time held in zone, ms
  unsigned long gateDwell()   const { return gateDwellMs; }

  // Orientation-calibrated latest sample (accel in g, gyro in deg/s).
  float accelG(int axis) const { return (axis >= 0 && axis < 3) ? accG[axis] : 0.0f; }
  float gyroDps(int axis) const { return (axis >= 0 && axis < 3) ? gyrDps[axis] : 0.0f; }
  float tempC() const { return dieTempC; }

private:
  TwoWire* bus;
  bool present;
  bool enabled;
  bool trackActive;

  Orient orient;
  float  thresholdG;          // |lateral g| below this => in the capture zone
  unsigned long gateDwellMs;  // time held in the zone before capture starts (0 = instant)
  unsigned long dwellMs;      // sustained condition before the alert latches

  // Latest calibrated sample.
  float accG[3];          // accel, g, at-rest bias removed
  float gyrDps[3];        // gyro, deg/s
  float dieTempC;

  // At-rest calibration.
  float restBias[3];      // averaged accel (g) captured while stationary
  int   verticalAxis;     // axis carrying gravity at rest
  int   lateralAxis;      // horizontal axis used for the lateral-g gate

  // Gate + latch running state.
  float latG;             // EMA-smoothed lateral g (minimal smoothing)
  bool  latInit;
  unsigned long zoneMs;   // continuous time held inside the zone, ms (0 on any exit)
  bool  capturing;
  unsigned long overAccumMs;   // overall time-in-over while capturing
  unsigned long underAccumMs;  // overall time-in-under while capturing
  Alert alert;
  int   pendingCond;      // last fed per-frame condition (+1/0/-1)

  // --- low-level QMI8658C helpers ---
  bool    writeReg(uint8_t reg, uint8_t val);
  uint8_t readReg(uint8_t reg);
  bool    readBytes(uint8_t reg, uint8_t* buf, uint8_t len);
  bool    readSample();   // fill accG/gyrDps/dieTempC from the chip
  int     dominantAxis(const float v[3]) const;
  void    resolveLateralAxis();
};

#endif // IMU_GATE_H
