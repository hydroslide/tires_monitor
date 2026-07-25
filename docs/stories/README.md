# Story drafts

User stories for the tire-temp monitor redesign, derived from
[`../tire-temp-functional-design.md`](../tire-temp-functional-design.md) and
reconciled against the driver's review notes.

Filed as GitHub issues in `hydroslide/tires_monitor` (labels `todo` + `bug`/`enhancement`).

## Global constraints (apply to every story)

- **Track-mode only.** Every feature here is enabled only in the menu's **Track** mode (`currentMode == 1`, via `getCurrentModeValue()`); in **Street** mode it stays inert / hidden. Track mode is the master switch for the whole redesign. (This is the Street/Track `currentMode`, *not* the thermal `thermalMode`.) Excludes **07** (bug fixes apply in all modes) and the raw-logging half of **08**.
- **UI/UX latitude.** Redesign menu UI/UX as needed, and fix wonky / broken / buggy behavior found along the way — note each such fix in the relevant issue (or file a new one).
- **Calculated everywhere.** When calculated mode (story 03) is enabled, the calculated (offset + smoothed) value is used for **all** temperatures, displays, and decisions system-wide — window/overheat, balance, verdict, summary. Raw is logged as a *separate* diagnostic channel set and never shown. When calculated mode is off, raw is used.
- **Persist & reload all settings.** Every menu setting — including the active tire-profile selection — is saved to EEPROM and reloaded on boot (the standard settings pattern). Any new setting these stories add follows it.
- **No laps on-device.** The device has no concept of laps (that comes from the external data logger). Session stats are whole-session aggregates.

## The slate

| File | Story | Design § | Type |
|---|---|---|---|
| [01](01-session-lifecycle-summary.md) | Session lifecycle + end-of-session summary | §5.3 | feature |
| [02](02-imu-capture-gate-latched-alerts.md) | IMU capture gate + debounced latched alerts | §5.4 | feature |
| [03](03-surface-carcass-calculated-mode.md) | Surface→carcass calculated mode (+ raw mode, always-log-both) | §5.5 | feature |
| [04](04-tire-profiles.md) | Tire profiles (window + K + τ + per-corner baseline) | §5.5 | feature |
| [05](05-fr-lr-thermal-balance.md) | Front/Rear & Left/Right thermal balance | §5.2 | feature |
| [06](06-demote-gate-inflation-verdict.md) | Gate & latch the inflation verdict | §5.1 | feature |
| [07](07-correctness-bugs.md) | Correctness bugs (firmware §3.1) | §3.1 | bug |
| [08](08-nbp-instrumentation-segment-colors.md) | NBP instrumentation channels + segment colors | firmware | feature |

## Suggested order (dependencies, not mandate)

1. **07 — correctness bugs** first. Data-integrity (the C-vs-F filter bug may have
   corrupted logs) and the "byte can't store 0" bug **blocks** the profile baselines.
2. **02 — IMU capture gate**. Foundational; the gate + latched over/under drive 06.
3. **03 — calculated mode** + **04 — tire profiles** together (profiles hold K/τ/baseline).
4. **05 — balance** (uses the calculated working temp).
5. **01 — session summary** (uses window / time-in-window / balance).
6. **06 — gate & latch verdict** (folds into 02's gate + latch).
7. **08 — instrumentation** (can run in parallel; also carries IMU + summary NBP streams).

## Template

Each file: user story · context · functional behavior · menu/settings · acceptance
criteria · implementation notes. Functional level — no code.
