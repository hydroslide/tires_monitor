# NBP instrumentation channels + segment colors

- **Area:** recommendations doc §3.3 · feature
- **Depends on:** 03 (log calculated + raw), 01 (summary over NBP), 02 (IMU over NBP), loosely 04/06 (verdict semantics)
- **Status:** reconciled from review notes

## User story

As an analyst and as the downstream video renderer, I want the device to emit its
verdicts, intermediate terms, per-segment colors, IMU data, and session summary over
NBP, so that dumps are self-describing and the renderer can consume colors directly
instead of re-implementing the classifier's logic.

## Context

Today NBP logs only the three raw band medians (`NBPProtocol.cpp`), so the verdict
must be re-derived offline and the renderer would have to duplicate device logic.
The driver wants to "just jam colors" at the renderer. Transport is **WiFi**, so
bandwidth is not a concern. Also needed: self-describing dumps (config + firmware
SHA) so a log is interpretable months later (§3.3).

## Functional behavior

- **Per-corner data channels:** `Delta`, `Threshold`, `Verdict` (−1/0/+1), and the
  **overall** over/under state (story 06/02).
- **Temps logged both ways (story 03):** the active value (calculated when enabled)
  under the **original channel labels**, and raw as a **new, separate channel set**.
- **Per-segment colors:** emit the segment fill color (temp/window) and the
  over/under/alignment delta color per band as **hex** values the renderer applies
  directly — no re-derivation.
- **IMU stream (story 02):** emit orientation-calibrated **accelerometer + gyro**
  data over NBP.
- **Session summary (story 01):** emit the sealed session summary over NBP so it's
  captured off-device.
- **Boot metadata (once per session):** crop offsets per corner, thresholds, active
  tire profile, ambient source/value, and the **firmware git SHA**.
- Keep raw band medians (backward compatible with existing parsing).

## Menu / settings

- Instrumentation verbosity (minimal / full) — optional.

## Acceptance criteria

- [ ] Track-only channels (verdict / colors) follow Track mode; raw median / temp logging stays on in all modes.
- [ ] Per-corner Delta/Threshold/Verdict + overall over/under channels present in the dump.
- [ ] Temps logged both ways: calculated under original labels + raw as a separate channel set.
- [ ] Per-segment colors emitted as **hex**, directly renderable.
- [ ] Orientation-calibrated accel + gyro emitted; sealed session summary emitted.
- [ ] Boot metadata (offsets, thresholds, profile, ambient, firmware SHA) emitted.
- [ ] Raw medians still present; existing `racerender.py` parsing not broken.

## Implementation notes

- Resolve the zero-as-no-data channel-set hazard here or in story 07h (avoid the corner collapsing to one channel mid-file).
