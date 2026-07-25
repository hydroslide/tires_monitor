# Front/Rear & Left/Right thermal balance

- **Area:** design §5.2 · feature
- **Depends on:** 03 (calculated working temp)
- **Status:** draft — not filed

## User story
As a driver/crew, I want front/rear and left/right temperature balance, so that I
can read the car's understeer/oversteer thermal bias and catch one-side problems.

## Context
Balance is the thermal fingerprint of handling (§5.2). It's the most
engineer-valued balance metric and maps to what the driver feels. Uses the
smoothed/calculated working temp, not raw spiky surface.

## Functional behavior
- **Front/Rear** = avg front vs avg rear: **fronts hotter → understeer; rears
  hotter → oversteer.**
- **Left/Right** = avg left vs avg right: **expected lopsided** at a directional
  track (Lime Rock loads the lefts); flags the *unexpected* (corner-weight, brake
  bias, one-side abuse).
- **Show the pair, not just the delta** — absolute levels matter (fronts +15 °F
  with everything in-window ≠ fronts +15 °F with cold rears).
- **Exclude RR** from the averages (unreliable hardware) — use the three good corners.
- Primarily a summary / between-session readout; an optional coarse **live**
  front/rear bias arrow is a nice-to-have.

## Menu / settings
- Show/hide balance on the summary.
- Live front/rear bias arrow: on/off.

## Acceptance criteria
- [ ] F/R and L/R computed from the calculated working temp.
- [ ] Each shown as a pair plus the delta, with a plain-language bias hint.
- [ ] RR excluded from the averages.

## Open questions / tweaks to discuss
- Working temp = center band or whole-tire average?
- Live bias arrow, or summary-only?
- With RR excluded, how is the "right" side represented (single tire FR)?
- Any thresholds before a bias is called (avoid over-reading small deltas)?
