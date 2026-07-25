# Front/Rear & Left/Right thermal balance

- **Area:** design §5.2 · feature
- **Depends on:** 03 (calculated working temp)
- **Status:** reconciled from review notes

## User story

As a driver/crew, I want front/rear and left/right temperature balance, so that I
can read the car's understeer/oversteer thermal bias and catch one-side problems.

## Context

Balance is the thermal fingerprint of handling (§5.2). It's the most
engineer-valued balance metric and maps to what the driver feels. Uses the
calculated working temp, not raw spiky surface.

## Functional behavior

- **Front/Rear** = avg front vs avg rear: **fronts hotter → understeer; rears
  hotter → oversteer.**
- **Left/Right** = avg left vs avg right: **expected lopsided** at a directional
  track (Lime Rock loads the lefts); flags the *unexpected* (corner-weight, brake
  bias, one-side abuse).
- **Working temp = whole-tire average** of the **calculated** value per corner.
- **All four corners are included** (RR is *not* excluded — it works well enough for
  the balance readout).
- **Show the pair, not just the delta** — absolute levels matter (fronts +15 °F with
  everything in-window ≠ fronts +15 °F with cold rears).
- **Summary only** — appears on the session summary; no live bias arrow.
- No bias threshold — report the numbers; small deltas are fine to show.

## Menu / settings

- Show/hide balance on the summary.

## Acceptance criteria

- [ ] Active **only in Track mode** (`currentMode == 1`); inert / hidden in Street mode.
- [ ] F/R and L/R computed from the calculated whole-tire average, all four corners.
- [ ] Each shown as a pair plus the delta, with a plain-language bias hint.
- [ ] Appears on the session summary (not a live readout).

## Implementation notes

- None outstanding — behavior settled in review.
