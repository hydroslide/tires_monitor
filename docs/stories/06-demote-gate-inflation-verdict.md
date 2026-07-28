# Gate & latch the inflation verdict

- **Area:** design §5.1 · feature
- **Depends on:** 02 (straight-line gate + overall latched over/under), 04 (per-corner baseline)
- **Status:** reconciled from review notes — **likely folds into story 02**

## User story

As a driver, I want the inflation indicator gated to straight-line frames,
baseline-corrected, and latched so it stays visible until it clears, so that it
stops giving backwards in-corner advice while still catching my eye with a real,
stable reading anywhere on track.

## Context

The live verdict was **backwards** (called the lefts OVER; the tire wanted more
pressure) because it read a mid-corner artifact (§3.4, §5.1). Rather than remove it,
gate it to straight-line (captured) frames, subtract the per-corner baseline, and
latch it so it's stable and glanceable. Mechanically this is the presentation of
story 02's **overall** latched over/under state, so this story likely folds into 02.

## Functional behavior

- Computed **only while capturing** (straight/steady, story 02), from **calculated**
  band temps, with the per-corner **baseline subtracted** (story 04).
- **Overall** over/under state (not per corner), per story 02.
- **Presented LIVE and latched:** once triggered it stays on until the opposite
  threshold clears it — visible **anywhere on track**, not just mid-corner.
- **On-time → summary:** track the fraction of captured session time the indicator
  was active ("on-time"). If on-time is **≥ 50%** of the session, surface the verdict
  on the session summary (story 01), which may need multiple swipeable pages.
  > **#21:** on-time is now tracked per corner against a shared captured-time
  > denominator (capture is a whole-car state), and the summary names the corners —
  > e.g. `OVER FL RR` — rather than printing one global verdict.
- Retain the underlying computation for diagnostics/logging (story 08).

## Menu / settings

- ~~Inflation indicator on/off (on by default in Track mode).~~ **Removed by #21.**
  Once the latched verdict drives the on-screen segment delta bars, an off switch
  left those bars showing an alignment verdict but no inflation one — which reads as
  "this tire is fine" rather than "this check is disabled". *Camera Settings →
  Segment Deltas* is the honest visibility control. EEPROM 31 is free again.

## Acceptance criteria

- [ ] Active **only in Track mode** (`currentMode == 1`); inert / hidden in Street mode.
- [ ] The verdict is produced only from straight-line (captured) frames, from calculated temps, baseline-corrected.
- [x] ~~It is an **overall** over/under (not per corner).~~ **Reversed by #21:** it is per corner. A global verdict could say something was wrong but never which tire, which is the only thing you can act on.
- [ ] Latched: once on, it stays on until the clearing threshold — visible anywhere on track.
- [ ] On-time is tracked; if ≥ 50% of the session, the verdict appears on the summary.

## Implementation notes

- Fold into story 02 (shared gate + latch) rather than a separate subsystem.
- On-time summary threshold (50% to start) and the multi-page summary layout.
