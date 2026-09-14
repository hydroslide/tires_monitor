# Surface→carcass calculated mode (+ raw mode, always-log-both)

- **Area:** design §5.5, §6 · feature
- **Depends on:** 04 (K/τ/window live in the tire profile)
- **Status:** reconciled from review notes

## User story

As a driver, I want a "calculated" display that estimates carcass temperature and
a "raw" fallback, so that the numbers match tire specs and stop flashing false
in-window spikes — while raw is still captured for later calibration.

## Context

The camera reads **surface** skin temp, which runs ~10–40 °F cooler than the
carcass a needle reads and spikes/decays far faster (§4). Published windows (e.g.
ECF 160–200 °F) are carcass-frame, so raw surface always looks too cold *and*
briefly flashes "ideal" on transient spikes. Fixed with an offset + smoothing.

## Functional behavior

- **Calculated mode:** `value ≈ EMA_τ(surface) + K`.
  - **τ (smoothing)** models the carcass's thermal inertia — kills transient
    "purple" flashes; a *better* overheat guard because real overheating is
    sustained and survives the filter. Default **τ = 15 s** (§6).
  - **K (offset)** shifts the reading toward carcass. Default **K = +20 °F** (§6),
    needle-anchored per tire.
- **Calculated everywhere (when enabled):** the calculated value is used for **all**
  temperatures, displays, and decisions across the system — window/overheat,
  balance, verdict, and summary. Only one value is shown at a time (never raw and
  calculated side by side).
- **Raw mode:** honest surface, unsmoothed — used when calculated mode is off.
- **Log BOTH, always:** the active value (calculated when enabled) is logged under
  the **original channel labels**; raw is logged as a **new, separate channel set**
  (story 08). When calculated is on, raw exists only for later diagnostics/recalibration.
- **Windows stored carcass-frame** so surface numbers are never compared against
  carcass thresholds; offset the reading up for readability.

## Menu / settings

- Display mode: **Raw / Calculated** (toggle).
- τ and K live **per tire profile** (story 04) with **hardcoded defaults**
  (τ = 15 s, K = +20 °F); there is no separate global override.

## Acceptance criteria

- [ ] Active **only in Track mode** (`currentMode == 1`); inert / hidden in Street mode.
- [ ] Calculated value = EMA_τ(surface) + K, using the active profile's τ/K.
- [ ] When calculated mode is on, calculated values feed **all** displays and decisions.
- [ ] Both are logged: calculated under the original labels + raw as a separate channel set.
- [ ] Only one value is shown at a time.
- [ ] No transient in-window flashes in calculated mode under corner-frequency spikes.

## Implementation notes

- Exact raw channel-set naming and log format (feeds story 08).
