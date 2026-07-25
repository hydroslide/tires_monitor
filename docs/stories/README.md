# Story drafts

Draft user stories for the tire-temp monitor redesign, derived from
[`../tire-temp-functional-design.md`](../tire-temp-functional-design.md).

**These are drafts, not filed issues.** The plan (design doc §7): walk them one
at a time, tweak each, *then* create the real GitHub issues in `hydroslide/tires_monitor`.
Nothing is filed until reviewed. GitHub Issues isn't configured as this repo's
tracker yet — the first filing pass should set that up (labels/states) first.

## Global constraints (apply to every story)

- **Track-mode only.** Every feature here is enabled only in the menu's **Track** mode (`currentMode == 1`, via `getCurrentModeValue()`); in **Street** mode it stays inert / hidden. Track mode is the master switch for the whole redesign. (This is the Street/Track `currentMode`, *not* the thermal `thermalMode`.) Excludes **07** (bug fixes apply in all modes) and the raw-logging half of **08**.
- **UI/UX latitude.** Redesign menu UI/UX as needed, and fix wonky / broken / buggy behavior found along the way — note each such fix in the relevant issue (or file a new one).

## The slate

| File | Story | Design § | Type |
|---|---|---|---|
| [01](01-session-lifecycle-summary.md) | Session lifecycle + end-of-session summary | §5.3 | feature |
| [02](02-imu-capture-gate-latched-alerts.md) | IMU capture gate + debounced latched alerts | §5.4 | feature |
| [03](03-surface-carcass-calculated-mode.md) | Surface→carcass calculated mode (+ raw mode, always-log-raw) | §5.5 | feature |
| [04](04-tire-profiles.md) | Tire profiles (window + K + τ + per-corner baseline) | §5.5 | feature |
| [05](05-fr-lr-thermal-balance.md) | Front/Rear & Left/Right thermal balance | §5.2 | feature |
| [06](06-demote-gate-inflation-verdict.md) | Demote/gate the inflation verdict | §5.1 | chore/feature |
| [07](07-correctness-bugs.md) | Correctness bugs (firmware §3.1) | §3.1 | bug |
| [08](08-nbp-instrumentation-segment-colors.md) | NBP instrumentation channels + segment colors | firmware | feature |

## Suggested order (dependencies, not mandate)

1. **07 — correctness bugs** first. Data-integrity (the C-vs-F filter bug may have
   corrupted logs) and the "byte can't store 0" bug **blocks** calibration/baseline work.
2. **02 — IMU capture gate**. Foundational; several stories gate on lateral g.
3. **03 — calculated mode** + **04 — tire profiles** together (profiles hold K/τ/baseline).
4. **05 — balance** (uses the calculated working temp).
5. **01 — session summary** (uses window / time-in-window / balance).
6. **06 — demote verdict** (partly done; finishes the gating).
7. **08 — instrumentation** (can run in parallel anytime).

## Template

Each file: user story · context · functional behavior · menu/settings · acceptance
criteria · **open questions / tweaks to discuss** (the review beat). Functional
level — no code.
