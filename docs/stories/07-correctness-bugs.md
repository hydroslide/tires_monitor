# Correctness bugs (firmware §3.1)

- **Area:** recommendations doc §3.1 · bug
- **Depends on:** — (do first; unblocks calibration + data integrity)
- **Status:** draft — not filed

## Summary

> **Not Track-gated** — correctness fixes apply in all modes (Street and Track).
A cluster of correctness bugs verified in the firmware while reading it. Two
matter for the data/calibration work and should lead; the rest are cleanups.
Decide during review whether to file as one tracking issue with a checklist or
split the load-bearing ones out.

## The bugs

**(a) Fahrenheit compared against Celsius in the validity filter.** `TempReader.cpp`
`readTemps()` converts each band C→F and validates it in the same pass, so the
cross-band "rescue" in `newTempIsInvalid()` compares an already-°F band against
sibling bands still in °C (~85-unit gap at operating temp). Result: asymmetric
band rejection, worst during warm-up; a rejected band latches its previous value.
**May have corrupted existing logs.** Fix: convert all three bands, *then* validate.

**(b) Uninitialized loop variable.** `TempReader.cpp:146` — `for (int tj; tj<3; tj++)`
reads stack garbage; a negative start indexes out of bounds. Fix: `int tj = 0`.

**(c) `ThreeSectionTire::initialize()` never runs.** Invoked from the base `Tire`
constructor (`Tire.cpp:12`) where the vtable is still `Tire`'s, so the override
doesn't execute and `lastDeltaColors[]` / `currentDeltaColors[]` stay uninitialized.
Cosmetic; call it from the derived ctor or first `draw()`.

**(d) `anySectionColorChanged()` has no return on the fall-through path**
(`ThreeSectionTire.cpp`). Harmless today (uncalled). Delete or fix.

**(e) A byte setting can never be stored as 0.** `MenuSystem.cpp:227` skips loading
any `VALUE_BYTE` whose stored value is 0, so `minInflationDeltaPct` can't be zeroed
and **signed per-corner baselines can't be stored** — this **blocks story 04**.
Needs a "written" sentinel or offset encoding.

**(f) Uneven bands from integer division.** `TempReader.cpp:76-77` — the left band
is systematically narrowest (6/7/7 for a 20-col crop). Irrelevant to inflation
(both shoulders averaged) but biases the alignment (O−I) metric. Distribute the
remainder symmetrically.

**(g) `draw()` returns early from inside its own loop** (`ThreeSectionTire.cpp`).
One out-of-range band aborts the whole tire's redraw + delta bar for that frame
without updating `lastTemps`; a single glitch frame stalls the display.

**(h) Zero overloaded as "no data" in the NBP encoder.** `NBPProtocol.cpp:43` —
`if (fl.values[1] == 0)` collapses a corner to a single channel, so a legitimate
0 (or a startup-rejected band) silently changes the channel set mid-file — a
parsing hazard for the dumps.

## Acceptance criteria
- [ ] (a) All bands converted before any validation; band rejection symmetric.
- [ ] (b) `tj` initialized.
- [ ] (e) A byte setting can store 0 / signed baselines can be persisted.
- [ ] (c,d,f,g,h) addressed or explicitly deferred with rationale.
- [ ] Existing log parsing unaffected (or the change documented for `tires_data`).

## Open questions / tweaks to discuss
- One tracking issue with a checklist, or split (a)/(b)/(e) as their own?
- Which are must-fix-now vs. later? (a), (b), (e) are the load-bearing ones.
- Does (a) warrant re-checking already-collected logs for the latch signature?
