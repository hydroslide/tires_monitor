# Session lifecycle + end-of-session summary

- **Area:** design §5.3 · feature
- **Depends on:** 03 (calculated working temp), 04 (window from profile), 05 (balance), 02 (auto-start/seal)
- **Status:** draft — not filed

## User story

As a driver, I want to start and end a session on the device and then read a
per-corner summary, so that I can review how each tire behaved without the
cooldown lap and the drive back to the paddock spoiling the data.

## Context

Real-time IR's unique value is per-session **warm-up, overheat, and balance**.
That only works with clean session boundaries — the slow cruise back and paddock
idle must not dilute peaks/averages/time-in-window. See §5.3, §2 (research: the
temporal use cases are the ones a post-session pyrometer can't serve).

## Functional behavior

- **Start:** a menu "Start Session" action begins accumulation; optional
  motion-based auto-start (story 02).
- **End / Seal:** a menu "End Session" action the driver hits **at the start of
  the cooldown lap**. It freezes accumulation (peaks, averages, balance,
  time-in-window) at that instant. Everything after the seal is ignored.
- **Auto-seal backstop:** seal automatically on sustained low speed / stationary
  if the driver forgets; optional "trim last N seconds" so a late manual press
  still cuts the cruise.
- **Summary screen (2×2 car map, FL FR / RL RR):** per corner —
  peak temp (large), average steady temp (small), time-in-window (color or tiny
  bar), overheat flag (exceeded threshold, and roughly how long).
- **Session screen:** F/R and L/R balance (story 05); warm-up time (laps/minutes
  until all four first reached the window).
- **History:** retain the last 1–3 sessions for on-device comparison (e.g. S2 vs S3).
- Readable while stationary, from the menu, at roughly the current font size.

## Menu / settings

- Actions: Start Session, End Session, View Summary (+ history).
- Settings: auto-start on motion (on/off), auto-seal on stationary (on/off),
  trim-last-N-seconds, history depth (1–3).


## Acceptance criteria

- [ ] Active **only in Track mode** (`currentMode == 1`); inert / hidden in Street mode.
- [ ] Sealing stops accumulation immediately; post-seal frames excluded from all stats.
- [ ] Summary shows per-corner peak, avg, time-in-window, overheat flag.
- [ ] Warm-up time and F/R + L/R balance shown at session level.
- [ ] Last N sessions retained and viewable stationary.
- [ ] RR excluded from balance/averages.

## Open questions / tweaks to discuss

- Does the summary auto-display on seal, or only when opened from the menu?
- Peak/avg computed from **raw** or **calculated** temp? (Recommend calculated.)
- How many sessions to retain (EEPROM/PSRAM budget)?
- Any per-lap breakdown wanted, or session-aggregate only?
