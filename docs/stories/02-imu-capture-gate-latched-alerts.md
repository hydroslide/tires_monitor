# IMU capture gate + debounced latched alerts

- **Area:** design §5.4 · feature
- **Depends on:** — (foundational; 01/05/06 build on it)
- **Status:** reconciled from review notes

## User story

As a driver, I want tire judgments captured only when I'm going straight, and an
alert that latches instead of flickering, so that mid-corner artifacts stop
producing false OVER/UNDER flashes.

## Context

The onboard **QMI8658C** 6-axis IMU (accel + gyro, I2C) reads the car's lateral g.
The event-triggered analysis (§3.4) showed the OVER pin is a body-roll geometry
artifact that peaks mid-corner (~−27 °F) and washes out on the straight (~−3 °F),
tracking lateral g. Gating out cornering strips ~24 of those 27 °F. This is the
mechanism-specific fix. This device's mount is far more stable than the phone that
produced the analysis logs, so the g signal needs only **minimal** smoothing.

## Functional behavior

- **Capture gate:** define "capturing" = |lateral g| below a threshold (car
  straight and steady). Accumulate the inflation / segment read **only while
  capturing**; suppress it while cornering. **Lateral accel only** (a
  hard-braking / longitudinal gate can be added later if it proves necessary).
  > **Amended by #20.** Being under the threshold is no longer sufficient on its
  > own: lateral g must stay under it for a **capture dwell** (`Gate Dwl 0.1s`,
  > default 0.5 s) before capture starts. As originally built, capture flipped on
  > the instant g crossed back under the threshold, so the tail of a corner —
  > still unwinding, load still shifting — counted as straight-line data. Setting
  > the dwell to `0` restores the original instant behaviour.
- **Debounced, latched alert:** while capturing, accumulate **overall**
  time-in-over / time-in-under (a single aggregate, **not per corner**). Once the
  overall condition exceeds the **dwell-time** threshold, raise the alert and
  **latch** it until it clears (also judged while capturing). Drives story 06's
  indicator. Replaces the per-frame flicker with a stable state.
- **Minimal smoothing** on the lateral-g signal (stable mount).
- **Orientation auto-calibration on every boot** — no manual step: learn level +
  forward axis at startup. A manual axis/sign override stays available as a fallback.
- **IMU over NBP:** emit the orientation-calibrated accelerometer **and gyro** data
  as NBP channels/events (story 08) — more reliable than the phone logger's data.
- **Auxiliary:** cornering detection to know which tires are loaded (feeds balance,
  story 05); stationary detection for the session auto-seal backstop (story 01).

## Menu / settings

- Lateral-g gate threshold — **global** setting (default ~0.3–0.4 g).
- Alert dwell time — **global** setting (default ~2–3 s).
- Capture-gate enable/disable.
- Manual orientation override (fallback; calibration is automatic on boot).
- *(#20)* Capture dwell — **global**, default 0.5 s, `0` = the original instant gate.
- *(#20)* `Show G Bar` — the on-screen lateral-g test bar. The gate had no visible
  output at all, so none of the settings above could be tuned by observation; the
  bar makes the threshold, the capture dwell and the boot calibration all legible
  while driving. See `docs/SETTINGS.md` → IMU Gate → The G Bar.

## Acceptance criteria

- [ ] Active **only in Track mode** (`currentMode == 1`); inert / hidden in Street mode.
- [ ] No inflation/segment accumulation while |lateral g| > threshold.
- [ ] An **overall** (not per-corner) time-in-over/under drives a latched alert after the dwell; it stays latched until the condition clears.
- [ ] Orientation auto-calibrates on every boot; axes map correctly to lateral/longitudinal.
- [ ] Orientation-calibrated accelerometer + gyro emitted over NBP.
- [ ] IMU read integrated without disrupting the sensor/display loop timing.

## Implementation notes

- Threshold/dwell start values (~0.3–0.4 g, ~2–3 s) are **global** (not per-profile) — tune on-car.
