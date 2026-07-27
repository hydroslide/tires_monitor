# Tire profiles (window + K + τ + per-corner baseline)

> **Status note (#18):** the **per-corner baseline was removed** after implementation, and
> the profile gained **per-corner camera crop offsets** (#15) in its place. #14 also made
> the profile the single source of truth for the window in *both* modes (not Track-only),
> and stopped persisting the active selection. The baseline rationale below is retained as
> the historical record — see "Amendments" in
> [`../tire-temp-functional-design.md`](../tire-temp-functional-design.md) for why it was
> dropped. Treat the acceptance criteria here as superseded where they mention baselines.

- **Area:** design §5.5 · feature
- **Depends on:** 07 (the "byte can't store 0" EEPROM bug blocks signed baseline storage)
- **Status:** reconciled from review notes

## User story

As a driver, I want selectable tire profiles that bundle window thresholds, the
surface→carcass offset K, the smoothing τ, and per-corner inflation baselines, so
that switching tires swaps all the calibration at once instead of re-tuning by hand.

## Context

ECF and EC02 read differently because they *are* different tires with different
pedestals and windows (§3.5, §5.5). All the tire-specific calibration belongs in
one selectable bundle. The per-corner baseline is the straight-line spread at the
known-good pressure (e.g. FL −3, RL −6, FR +4 at 31 psi) that defines each corner's
"zero" for the inflation indicator (story 06).

## Functional behavior

- A **profile** holds:
  - window thresholds (cold / warm / ideal / overheat), stored carcass-frame;
  - offset **K** (default +20 °F);
  - smoothing **τ** (default 15 s);
  - per-corner **inflation baseline** (signed °F, one per corner).
  - (The IMU gate threshold / dwell are **global**, story 02 — *not* in the profile.)
- **3 profile slots.** Select / edit / name each.
- Seed profiles: **ECF** (window ~160–200 carcass, K +20, τ 15, baselines
  FL −3 / RL −6 / FR +4) and **EC02**.
- **Persist to EEPROM, including the active-profile selection** so the same profile
  reloads on boot — the standard settings-persistence pattern (see global
  constraints; it applies to *all* settings, not just profiles).
- **Naming UI (small screen):** swipe up/down to cycle a letter, swipe **left** to lock
  it in and move to the next slot to the right (locking past the last slot saves);
  swipe **right** to step back a slot, and backing out past the first slot cancels.
  Left/right follow the rest of the menu, where left confirms/descends and right goes back.

## Menu / settings

- Select active profile.
- Edit profile fields (window bounds, K, τ, per-corner baseline).
- Create / name / delete profile.

## Acceptance criteria

- [ ] Active **only in Track mode** (`currentMode == 1`); inert / hidden in Street mode.
- [ ] Selecting a profile swaps window, K, τ, and baselines together.
- [ ] **3 slots**; profiles *and the active selection* persist across power cycles (reload on boot).
- [ ] Signed per-corner baselines store and load correctly (needs story 07e).
- [ ] The baseline is applied to the inflation indicator (story 06).

## Implementation notes

- Baseline encoding — signed byte vs offset-encoded (blocked on story 07e).
- Small-screen name-entry UX (swipe up/down letters, swipe left to lock).
