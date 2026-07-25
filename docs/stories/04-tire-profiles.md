# Tire profiles (window + K + τ + per-corner baseline)

- **Area:** design §5.5 · feature
- **Depends on:** 07 (the "byte can't store 0" EEPROM bug blocks signed baseline storage)
- **Status:** draft — not filed

## User story
As a driver, I want selectable tire profiles that bundle window thresholds, the
surface→carcass offset K, the smoothing τ, and per-corner inflation baselines, so
that switching tires swaps all the calibration at once instead of re-tuning by hand.

## Context
ECF and EC02 read differently because they *are* different tires with different
pedestals and windows (§3.5, §5.5). All the tire-specific calibration belongs in
one selectable bundle. The per-corner baseline is the straight-line spread at the
known-good pressure (e.g. FL −3, RL −6, FR +4 at 31 psi) that defines each corner's
"zero" for the demoted inflation hint.

## Functional behavior
- A **profile** holds:
  - window thresholds (cold / warm / ideal / overheat), stored carcass-frame;
  - offset **K** (default +20 °F);
  - smoothing **τ** (default 15 s);
  - per-corner **inflation baseline** (signed °F, one per corner).
- **Select** a profile from the menu; all values swap together.
- **Edit** any field per profile; **create/name** profiles.
- Seed profiles: **ECF** (window ~160–200 carcass, K +20, τ 15, baselines
  FL −3 / RL −6 / FR +4, RR n/a) and **EC02**.
- Persist to EEPROM.

## Menu / settings
- Select active profile.
- Edit profile fields (window bounds, K, τ, per-corner baseline).
- Create / name / delete profile.

## Acceptance criteria
- [ ] Active **only in Track mode** (`currentMode == 1`); inert / hidden in Street mode.
- [ ] Selecting a profile swaps window, K, τ, and baselines together.
- [ ] Profiles persist across power cycles.
- [ ] Signed per-corner baselines store and load correctly (needs story 07e).
- [ ] The baseline is applied to the straight-line inflation hint (story 06).

## Open questions / tweaks to discuss
- How many profile slots (EEPROM budget)?
- Baseline encoding — signed byte / offset-encoded (blocked on story 07e)?
- Should profile carry the IMU gate threshold / dwell (story 02) too?
- Naming/UI for profiles on a small screen.
