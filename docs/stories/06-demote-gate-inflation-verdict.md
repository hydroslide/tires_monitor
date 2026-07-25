# Demote / gate the inflation verdict

- **Area:** design §5.1 · chore/feature
- **Depends on:** 02 (straight-line gate), 04 (per-corner baseline)
- **Status:** draft — not filed

## User story
As a driver, I want the live inflation indicator off by default and, when enabled,
gated to straight-line frames as a between-session hint, so that it stops giving
backwards in-corner advice.

## Context
The live inflation verdict was **backwards** (called the lefts OVER; the tire
wanted more pressure) because it read a mid-corner artifact (§3.4, §5.1). The
segment-delta indicator is already a menu toggle; this story finishes the demotion.

## Functional behavior
- **Off by default.** The segment-delta indicator stays off unless deliberately enabled.
- **When enabled:** computed **only while capturing** (straight/steady, story 02),
  with the per-corner **baseline subtracted** (story 04), and presented as a
  **between-session hint** — never a live in-corner directive.
- Retain the underlying computation for diagnostics/logging.

## Menu / settings
- Inflation indicator: off by default (existing toggle).
- (If kept) hint display style — between-session summary line vs live.

## Acceptance criteria
- [ ] Indicator is off on a fresh config.
- [ ] When on, the verdict is only produced from straight-line (captured) frames.
- [ ] Baseline-corrected before classification.
- [ ] Not presented as a live "change pressure now" directive mid-corner.

## Open questions / tweaks to discuss
- Remove the live in-corner indicator entirely, or keep it gated?
- Should this fold into story 02 (gate) rather than stand alone?
- What does the "between-session hint" look like on screen / in the summary?
