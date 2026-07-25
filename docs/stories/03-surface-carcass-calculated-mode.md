# Surface→carcass calculated mode (+ raw mode, always-log-raw)

- **Area:** design §5.5, §6 · feature
- **Depends on:** 04 (K/τ/window live in the tire profile)
- **Status:** draft — not filed

## User story
As a driver, I want a "calculated" display that estimates carcass temperature and
a "raw" mode I can fall back to, so that the numbers match tire specs and stop
flashing false in-window spikes — while the raw signal is always preserved for
later calibration.

## Context
The camera reads **surface** skin temp, which runs ~10–40 °F cooler than the
carcass a needle reads and spikes/decays far faster (§4). Published windows (e.g.
ECF 160–200 °F) are carcass-frame, so raw surface always looks too cold *and*
briefly flashes "ideal" on transient spikes. Fixed with an offset + smoothing.

## Functional behavior
- **Calculated mode:** `display ≈ EMA_τ(surface) + K`.
  - **τ (smoothing)** models the carcass's thermal inertia — kills transient
    "purple" flashes; a *better* overheat guard because real overheating is
    sustained and survives the filter. Default **τ = 15 s** (§6).
  - **K (offset)** shifts the reading toward carcass. Default **K = +20 °F** seed
    (§6), to be needle-anchored per tire.
- **Raw mode:** honest surface, unsmoothed — for bench/debug/data.
- **Menu toggle** between raw and calculated.
- **Always log raw** (or raw + the active K/τ) even in calculated mode, so
  calibration can be re-derived offline. Never discard the raw signal.
- **Windows stored in one reference frame (carcass)** so surface numbers are never
  compared against carcass thresholds (today's trap). Offset the *reading* up for
  readability; equivalently the thresholds could shift down — same decision.

## Menu / settings
- Display mode: Raw / Calculated.
- τ and K are **per tire profile** (story 04), surfaced for edit there; defaults
  τ = 15 s, K = +20 °F.

## Acceptance criteria
- [ ] Active **only in Track mode** (`currentMode == 1`); inert / hidden in Street mode.
- [ ] Calculated value = EMA_τ(surface) + K, using the active profile's τ/K.
- [ ] Raw mode shows unsmoothed surface.
- [ ] Raw is always logged regardless of display mode.
- [ ] No transient in-window flashes in calculated mode under corner-frequency spikes.
- [ ] Overheat/window logic keys off the calculated value in calculated mode.

## Open questions / tweaks to discuss
- Are K/τ strictly per-profile, or is there a global override?
- Log format for raw + params (feeds story 08 instrumentation).
- Does the summary (story 01) report raw peaks, calculated, or both?
- Should the driver see both numbers at once (raw small, calculated large)?
