# IMU capture gate + debounced latched alerts

- **Area:** design §5.4 · feature
- **Depends on:** — (foundational; 01/05/06 build on it)
- **Status:** draft — not filed

## User story
As a driver, I want tire judgments captured only when I'm going straight, and
alerts that latch instead of flickering, so that mid-corner artifacts stop
producing false OVER/UNDER flashes.

## Context
The onboard **QMI8658C** 6-axis IMU (accel + gyro, I2C) reads the car's lateral g.
The event-triggered analysis (§3.4) showed the OVER pin is a body-roll geometry
artifact that peaks mid-corner (~−27 °F) and washes out on the straight (~−3 °F),
tracking lateral g. Gating out cornering strips ~24 of those 27 °F. This is the
mechanism-specific fix, not a heuristic.

## Functional behavior
- **Capture gate:** define "capturing" = |lateral g| below a threshold (car
  straight and steady, not hard braking). Accumulate the inflation / segment read
  **only while capturing**; suppress it while cornering.
- **Debounced, latched alerts:** while capturing, accumulate *time-in-over* /
  *time-in-under* per corner. Once a corner exceeds a **dwell-time** threshold,
  raise the alert and **latch** it until the condition clears (also judged while
  capturing). Replaces the per-frame flicker with a stable state.
- **Auxiliary uses:** cornering/braking detection to know which tires are loaded
  (feeds balance, story 05); motion-based session auto-start; stationary auto-seal
  (story 01).
- **Orientation calibration:** one-time — learn how the box is mounted (sit still =
  level; drive straight + brake = forward axis), or pick axis/sign in the menu.
  Coarse is fine; we only need straight-vs-cornering.

## Menu / settings
- Lateral-g gate threshold (default ~0.3–0.4 g — TBD).
- Alert dwell time (default ~2–3 s — TBD).
- Orientation-calibration action; manual axis/sign override.
- Capture-gate enable/disable.

## Acceptance criteria
- [ ] Active **only in Track mode** (`currentMode == 1`); inert / hidden in Street mode.
- [ ] No inflation/segment accumulation while |lateral g| > threshold.
- [ ] Alert fires only after the dwell time in-condition, and latches until clear.
- [ ] Orientation calibration correctly maps the box's axes to lateral/longitudinal.
- [ ] IMU read integrated without disrupting the sensor/display loop timing.

## Open questions / tweaks to discuss
- Threshold and dwell defaults — start values, and whether they're per-profile.
- Gate on hard braking (longitudinal g) too, or lateral only?
- Any use for the gyro (steady cornering vs transitions), or lateral accel enough?
- Smoothing on the (shaky-mount) g signal — how much.
