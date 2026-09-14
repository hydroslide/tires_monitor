# Session lifecycle + end-of-session summary

- **Area:** design §5.3 · feature
- **Depends on:** 03 (calculated working temp), 04 (window from profile), 05 (balance), 02 (stationary auto-seal backstop), 08 (summary over NBP)
- **Status:** reconciled from review notes

## User story

As a driver, I want to start and end a session with a simple gesture and then read
a per-corner summary, so that I can review how each tire behaved without the
cooldown lap and the drive back to the paddock spoiling the data.

## Context

Real-time IR's unique value is per-session **warm-up, overheat, and balance**.
That only works with clean session boundaries — the slow cruise back and paddock
idle must not dilute peaks/averages/time-in-window. A menu start/stop is awkward to
hit while driving, so session control is a gesture.

## Functional behavior

- **Start / end by swipe (Track mode).** Commandeer the existing left/right swipe
  gesture. In **Track** mode the swipe toggles the **session** (and does *not*
  toggle Night Mode); in **Street** mode the swipe still toggles Night Mode and
  never a session. Swipe with no session running → **start**; swipe with a session
  running → **end**.
- **Swipe feedback:** on **start**, a small red "recording" dot in the upper-right
  corner for 5 s; on **end**, a small black "stop" square in the same spot for 5 s,
  then the session summary is shown. The dot pulses within its own round footprint
  and the square shows a pulsing outline over a steady black fill, so each reads as a
  single blinking badge (not two shapes at once).
- **Toggle debounce (1 s):** the touch layer often reports one physical swipe twice,
  which would start a session and immediately end it (or bleed a summary-dismiss swipe
  into starting a new one). The session-toggle swipe is locked out for 1 s after any
  start/end and after dismissing the summary, so the duplicate registration is dropped.
  Up/down (thermal-mode switching) is unaffected.
- **Seal on end:** ending freezes accumulation (peaks, averages, balance,
  time-in-window) at that instant; everything after is ignored. Swipe-end at the
  start of the cooldown lap so the cruise back isn't included.
- **Auto-seal backstop:** optionally seal on sustained low speed / stationary
  (story 02) if the driver forgets to swipe.
- **Summary screen (2×2 car map, FL FR / RL RR):** per corner — peak temp (large),
  average steady temp (small), time-in-window (color or bar), overheat flag
  (exceeded threshold, and roughly how long). All temps are **calculated** values.
- **Session-level screen(s):** F/R and L/R balance (story 05); warm-up time (time
  until all four first reached the window); and — if the inflation on-time is high
  enough (story 06) — the inflation verdict. The summary may span **multiple
  swipeable pages**.
- **Persist + recall:** the last summary is written to **EEPROM** and **overwrites**
  the previous one; the menu's **View Summary** recalls it any time. The summary is
  also **emitted over NBP** (story 08) so it's captured off-device.
- Whole-session **aggregates only** — the device has no concept of laps.
- Readable while stationary, at roughly the current font size.

## Menu / settings

- Action: **View Summary** (recall the last summary from EEPROM).
- Setting: auto-seal on stationary (on/off).

## Acceptance criteria

- [ ] Active **only in Track mode** (`currentMode == 1`); inert / hidden in Street mode.
- [ ] Left/right swipe starts/ends a session in Track mode and does **not** toggle Night Mode; in Street mode the swipe toggles Night Mode and never a session.
- [ ] Start shows the red dot for 5 s; end shows the black square for 5 s, then the summary.
- [ ] Sealing stops accumulation immediately; post-seal frames excluded from all stats.
- [ ] Summary shows per-corner peak, avg, time-in-window, overheat flag (calculated temps).
- [ ] Warm-up time and F/R + L/R balance shown at session level.
- [ ] Last summary persisted to EEPROM (overwriting the previous), recallable via View Summary, and emitted over NBP.

## Implementation notes

- EEPROM budget for the summary blob (per-corner peak/avg/time-in-window/overheat + balance + warm-up).
- Multi-page summary layout and how pages are swiped through (shares the swipe gesture).
