# NBP instrumentation channels + segment colors

- **Area:** recommendations doc §3.3 · feature
- **Depends on:** 03 (log raw + calculated), loosely 02/04/06 (verdict semantics)
- **Status:** draft — not filed

## User story
As an analyst and as the downstream video renderer, I want the device to emit its
verdicts, intermediate terms, and per-segment colors over NBP, so that dumps are
self-describing and the renderer can consume colors directly instead of
re-implementing the classifier's logic.

## Context
Today NBP logs only the three raw band medians (`NBPProtocol.cpp`), so the verdict
must be re-derived offline and the renderer would have to duplicate device logic.
The driver wants to "just jam colors" at the renderer rather than have it
reinterpret values. Also needed: self-describing dumps (config + firmware SHA) so
a log is interpretable months later (§3.3).

## Functional behavior
- **Per-corner data channels:** `Delta`, `Threshold`, `Verdict` (−1/0/+1), and the
  over/under state. Log **raw and calculated** temps (story 03), not one or the other.
- **Per-segment colors:** emit the segment fill color (temp/window) and the
  over/under/alignment delta color per band as values the renderer can apply
  directly — no re-derivation.
- **Boot metadata (once per session):** crop offsets per corner, thresholds,
  active tire profile, ambient source/value, and the **firmware git SHA**.
- Keep raw band medians (backward compatible with existing parsing).

## Menu / settings
- Instrumentation verbosity (minimal / full) — optional, to keep the stream lean.

## Acceptance criteria
- [ ] Track-only channels (verdict / colors) follow Track mode; raw median / temp logging stays on in all modes.
- [ ] New per-corner Delta/Threshold/Verdict channels present in the dump.
- [ ] Per-segment colors emitted and directly renderable.
- [ ] Boot metadata (offsets, thresholds, profile, ambient, firmware SHA) emitted.
- [ ] Raw medians still present; existing `racerender.py` parsing not broken.

## Open questions / tweaks to discuss
- Color encoding the renderer wants (RGB565? named states? hex?).
- Which channels are essential vs. nice-to-have (stream bandwidth over WiFi).
- Do we resolve the zero-as-no-data channel-set hazard here or in story 07h?
- Emit both raw and calculated temp channels, or a mode flag + one set?
