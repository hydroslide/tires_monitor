# Tire-Temp Algorithm — Coding & Testing Recommendations

Follow-on to [`tire-temp-pressure-handoff.md`](tire-temp-pressure-handoff.md). That doc framed the problem and ranked hypotheses before anyone had read the firmware. This one is written **after** reading `TempReader.cpp`, `ThreeSectionTire.cpp`, `NBPProtocol.cpp`, `TireMenu.cpp`, and `MenuSystem.cpp`, and it **supersedes the hypothesis ranking in handoff §4**.

Scope: what to change in code, and what to test — offline, on the bench, and at the track. No code has been changed yet.

---

## 1. Corrected framing

The handoff's mental model was "the metric doesn't respond to pressure." The firmware says something sharper.

The classifier ([`ThreeSectionTire.cpp:139-151`](../tires_esp32/ThreeSectionTire.cpp#L139-L151)):

```cpp
float avgEdge = (sectionTemps[outer] + sectionTemps[inner]) / 2.0f;
float delta   = avgEdge - sectionTemps[center];          // +ve = shoulders hot = under
float minInflationDelta = avgEdge * (minInflationDeltaPct / 100.0f);
```

`minInflationDeltaPct` defaults to **10** ([`TireMenu.cpp:52`](../tires_esp32/TireMenu.cpp#L52)), and `avgEdge` is an **absolute °F** temperature. On a 150 °F tire the bar to declare over-inflation is therefore **15 °F of center-hot spread**.

It trips. Persistently. Across a 3 psi swing.

That is not a null result — it is a positive measurement. **The frames genuinely contain ≥15 °F of center-hot spread at every pressure tested.** The failure mode is not "too insensitive to see the signal," it is:

| Component | Magnitude (est.) |
|---|---|
| Structural / thermal pedestal (pressure-independent) | ~15–20 °F center-hot |
| Pressure-driven modulation (28 → 31 psi) | ~3–6 °F |
| Classifier threshold | ~15 °F |

The pedestal alone clears the bar. The pressure term modulates the total but never pulls it back under. The indicator pins to OVER and the real signal is invisible — not absent.

**This is handoff §4 hypothesis 3 (missing calibration baseline), promoted to primary.** The tool is measuring an absolute where it should measure a deviation from a per-tire zero.

---

## 2. Hypothesis ledger (revised)

### Withdrawn — errors in the earlier read of the code

- **"The crop isn't happening / offsets default to 0."** Wrong. [`TempReader.cpp:71-77`](../tires_esp32/TempReader.cpp#L71-L77) walks `leftOffset → COLS-rightOffset` and splits *that* range into thirds. Everything sampled is inside the margins. The source defaults of 0 are irrelevant because EEPROM supplies real per-corner values.
- **"The 3-row strip may drift onto the shoulder."** Wrong, and rotated 90°. Sections split by **column** (left/center/right = inner/center/outer); `useMiddleRows` takes rows 10–12, a **horizontal** strip spanning the tread. Vertical aim error moves the strip *circumferentially*, which is thermally near-uniform. Middle rows are the correct choice.

### Demoted

- **Equal-pixel vs. equal-arc-length thirds (was handoff's top suspect).** `getSectionMedians` returns a **median**, not a mean ([`TempReader.cpp:91`](../tires_esp32/TempReader.cpp#L91)). A 7-column band × 3 rows is ~21 samples; even if the outermost 1–2 columns sit on the roll-off, that is 3–6 of 21 samples in the tail and the median does not move. Arc-length weighting only matters for a mean. **Not worth building unless the bench test in §5.1 says otherwise.**
- **Emissivity at viewing angle (handoff §4.2).** At any sane mount distance the incidence angle across a 225 mm tread is ~20–30°, well inside rubber's flat ε region (falloff starts ~60°). The genuinely oblique pixels at the extreme edge are exactly the minority the median discards. Effectively dead for a roughly-normal mount; only revisit if the camera looks along the tire at a steep angle.

### Live, ranked

1. **Structural crown.** 225/40-17 on a 17x8 is at the narrow end of the tire's rim range. Tread crowns; center carries disproportionate contact pressure at *every* inflation pressure. Pressure-independent by construction.
2. **Lateral shoulder cooling** — *not in the original hypothesis list, and it belongs there.* The shoulder tread abuts the sidewall and bead, which run far cooler and conduct heat laterally out of the tread. The shoulder is also the most air-exposed part of the tread, and by the time it rotates to the sensor it has cooled more than the center. Produces a persistent center-hot **surface** profile largely independent of inflation, scaling with airflow and inversely with ambient. A cool evening session at Lime Rock in a 2400 lb car maximizes it.
3. **Threshold anchor and asymmetry.** Real defects (§3.2), but second-order — they explain ~10% distortions, not a pinned indicator.

Hypotheses 1 and 2 are **not separable from the thermal data alone** — both produce a symmetric, constant, pressure-insensitive center-hot pedestal. Separating them requires the chalk / needle-pyrometer ground truth in §5.3. Fortunately the *fix* is the same for both: subtract a per-tire baseline.

---

## 3. Code recommendations

### 3.1 Correctness bugs — fix regardless of the algorithm outcome

**(a) Fahrenheit compared against Celsius in the validity filter.** [`TempReader.cpp:173-184`](../tires_esp32/TempReader.cpp#L173-L184) converts C→F **in place, band by band**, then validates each band immediately:

```cpp
for (int j = 0; j < 3; j++) {
    float valueF = tireSectionTemps[i][j] * 9.0f / 5.0f + 32.0f;
    if (useFarenheit) tireSectionTemps[i][j] = valueF;
    if (newTempIsInvalid(i, j)) ...
}
```

The cross-band rescue inside `newTempIsInvalid` ([`TempReader.cpp:146-152`](../tires_esp32/TempReader.cpp#L146-L152)) compares band `j` (already °F) against sibling bands that have **not yet been converted** (still °C). At 65 °C / 150 °F that comparison is `|150 − 65| = 85 > MAX_STEP(30)`, so the rescue never fires for bands 0 and 1 but works normally for band 2. Result: **asymmetric rejection rates across the three bands**, worst during warm-up when temps are changing fastest, and a rejected band latches its previous value.

Fix: convert all three bands, *then* validate all three.

**(b) Uninitialized loop variable — undefined behavior.** Same function, [`TempReader.cpp:146`](../tires_esp32/TempReader.cpp#L146):

```cpp
for (int tj; tj<3; tj++){          // tj never initialized
```

Reads stack garbage; if it starts negative, `tireSectionTemps[i][tj]` indexes out of bounds. Should be `int tj = 0`.

**(c) `ThreeSectionTire::initialize()` never runs.** It is invoked from the **base** `Tire` constructor ([`Tire.cpp:12`](../tires_esp32/Tire.cpp#L12)), where the vtable is still `Tire`'s, so the empty `Tire::initialize()` executes instead of the override. `lastDeltaColors[]` and `currentDeltaColors[]` stay uninitialized. Cosmetic (does not touch classification math), but call it from the derived constructor or from the first `draw()`.

**(d) `anySectionColorChanged()` has no `return` on the fall-through path** ([`ThreeSectionTire.cpp:73-78`](../tires_esp32/ThreeSectionTire.cpp#L73-L78)). Currently harmless because nothing calls it. Delete it or fix it.

**(e) A byte setting can never be stored as 0.** [`MenuSystem.cpp:227`](../tires_esp32/MenuSystem.cpp#L227) skips loading any `VALUE_BYTE` whose stored value is 0:

```cpp
if (EEPROM.read(addr) > 0 && EEPROM.read(addr) < 255) { ... }
```

So `minInflationDeltaPct` cannot be zeroed to log raw unthresholded deltas — which is exactly what you want during calibration. Needs a separate "written" sentinel or a magic-byte scheme.

**(f) Uneven bands from integer division.** [`TempReader.cpp:76-77`](../tires_esp32/TempReader.cpp#L76-L77): for a 20-column crop the bands come out **6 / 7 / 7**; for 22, **7 / 7 / 8**. The left band is systematically narrowest. Irrelevant to the inflation metric (both shoulders are averaged) but it **biases the alignment metric**, which compares outer against inner directly. Distribute the remainder symmetrically.

**(g) `draw()` returns early from inside its own loop.** [`ThreeSectionTire.cpp:95-96`](../tires_esp32/ThreeSectionTire.cpp#L95-L96): one out-of-range band aborts the whole tire's redraw *and* the delta bar for that frame, without updating `lastTemps`. Cosmetic, but it means a single glitch frame stalls the display.

**(h) Zero is overloaded as "no data" in the NBP encoder.** [`NBPProtocol.cpp:43`](../tires_esp32/NBPProtocol.cpp#L43): `if (fl.values[1] == 0)` decides between logging one channel or three. A center band that legitimately reads 0 — or that was rejected while `lastTireSectionTemps` was still 0 during startup — silently collapses the tire to a **single** `"Front Left Tire"` channel instead of `O/C/I`. This is both a bug and a **parsing hazard for the dumps**: the channel set changes mid-file. See §4.0(e).

### 3.2 Algorithm changes

**(a) Introduce a per-tire baseline. This is the primary fix.**

Keep the existing sign convention to avoid a regression (handoff §3 explicitly warns off sign-chasing):

```cpp
float delta = avgEdge - sectionTemps[center];        // unchanged: +ve = shoulders hot = under
float corrected = delta - baselineDelta[tireIndex];  // NEW
// classify `corrected`, not `delta`
```

`baselineDelta` is the tire's natural spread with correct pressure — **negative** for a crowned tire (shoulders cooler than center). Determined empirically per tire-and-wheel combination from §4.

*Implementation constraint:* `MenuSystem` only supports unsigned `VALUE_BYTE` and cannot store 0 (§3.1e). A signed baseline needs either a new `VALUE_SBYTE` type, or an offset encoding — e.g. store `baseline + 20` over the range 1…41 to represent −19…+21 °F. Four EEPROM addresses needed, one per corner.

**(b) Re-anchor the threshold.** Two independent problems with `avgEdge * pct/100`:

*Anchor quantity.* Percent-of-absolute-°F does scale in the direction you want (hotter tire → wider spread range → bigger absolute delta required), but the curve does not pass through the origin. At zero rise above ambient no work is being done and the expected spread is zero, yet `10% × 70 °F` still demands a 7 °F threshold:

| Tire temp | Rise over 70 °F ambient | Threshold | As % of rise |
|---|---|---|---|
| 100 °F | 30 °F | 10.0 °F | 33% |
| 150 °F | 80 °F | 15.0 °F | 19% |
| 180 °F | 110 °F | 18.0 °F | 16% |

Twice as coarse cold as hot. `pct × (avgTemp − ambient)` preserves the intent and removes the offset.

*Anchor band.* The scale factor is the **shoulder** mean, not the mean of all three bands. Hold shoulder temp fixed and the two branches are symmetric; hold the *tire's overall* temperature fixed — the physically meaningful comparison — and they are not. With `M` = midpoint of center and shoulder-mean and `s` = spread:

- Over-inflation trips when `s ≥ 0.1·(M − s/2)` → `s ≥ 0.0952·M`
- Under-inflation trips when `s ≥ 0.1·(M + s/2)` → `s ≥ 0.1053·M`

At M = 150 °F: over trips at 14.3 °F, under at 15.8 °F. **Under-inflation needs ~10.5% more spread to be called than over-inflation does.** A center-hot tire has cold shoulders → lower bar; a shoulder-hot tire has hot shoulders → higher bar. Anchoring to the mean of all three bands fixes it for free.

Note this is a ~10% distortion. It is a genuine defect pointed in the same direction as the complaint, but it is **not** the cause of a pinned indicator. Do not expect fixing it to solve the problem.

*Recommendation:* once a baseline is subtracted the residual is small, and a **fixed °F threshold on the corrected deviation** (start ~5 °F, tune from §4) is simpler and more defensible than a percentage of anything. Let the data in §4.0(b) decide whether the pedestal is constant in °F or proportional to rise-above-ambient; if proportional, scale the *baseline*, not the threshold.

**(c) Gate classification on operating temperature.** Suppress the verdict entirely (new neutral/grey state) below a menu-settable minimum average tire temp. This kills nonsense during warm-up *and* directly implements handoff §5.5 — if the tires never reach operating temp, the honest output is "no call," not a pressure recommendation. A light car on a cool evening on a 200TW tire is a real candidate for this.

**(d) Source an ambient reference.** Needed for (b) and possibly (c). Options in order of preference:
1. The MLX90640 reports its own die/ambient temperature `Ta`. Check whether the Adafruit wrapper exposes it; if not, call `MLX90640_GetTa()` on the underlying Melexis driver. Free, per-sensor, no new hardware.
2. Median of the frame region *outside* the crop margins — but this is wheel-well/bodywork/sky, not air temp. Noisy.
3. A menu-entered ambient set at the event. Crude but adequate.

### 3.3 Instrumentation — do this before the next event

**(a) Log the verdict and the intermediate terms.** Right now [`NBPProtocol.cpp:37-73`](../tires_esp32/NBPProtocol.cpp#L37-L73) logs raw per-band medians and nothing else. The verdict is a pure function of those three numbers, so it *can* be recomputed offline — but add explicit channels anyway so the dump is self-describing and firmware/analysis drift is detectable:
- `<Corner> Delta` — the computed `delta`
- `<Corner> Thresh` — the computed threshold
- `<Corner> Verdict` — −1 / 0 / +1

**(b) Log configuration once per session.** Emit crop offsets per corner, `minInflationDeltaPct`, `minAlignmentDeltaPct`, temp scale, mode, and baseline values as NBP metadata at boot. Without this, a dump cannot be interpreted six months later — and the cross-tire comparison in §4.1 *depends* on knowing whether the offsets were re-set between tires.

**(c) Log band spread, not just the median.** Add per-band min and max, or a rough IQR. A shoulder band contaminated by background shows up immediately as a wide spread with a cold tail, which is the decisive check for §4.0(d).

**(d) Log ambient.** Whichever source (d) above resolves to.

**(e) Stamp firmware git SHA** into NBP metadata at boot.

---

## 4. Testing — offline, on existing data

**Do all of this before touching hardware.** Everything below runs against dumps already in hand. The raw per-band medians are logged unsmoothed, so the classifier can be re-derived entirely offline.

Available: session 2 (28 psi hot), session 3 (31 psi hot), plus — newly available — **runs from a different day on a different tire where the problem did not occur.** That last set is the most valuable data in the pile; see §4.1.

### 4.0 Baseline characterization

**(a) Build the time series.** Parse NBP (`*NBP1,UPDATEALL,<t>` … `#`, channel lines `"Front Left Tire C","degF":152.00`). Per corner, per frame:

```
spread = C − (I + O)/2          # +ve = center hot = "over" per current logic
avgAll = (I + C + O)/3
```

Note the timestamp is `millis()/1000` and **resets on reboot** — it is session-relative, not wall-clock. Align to other logger channels accordingly.

**(b) Characterize the pedestal — the central question.** Per session, per corner: median, IQR, and full distribution of `spread`. Then:
- Is the session-median `spread` meaningfully different between 28 psi and 31 psi? **Quantify it.** If it moves 3–6 °F, the signal exists and only the pedestal is in the way — baseline subtraction fixes the tool.
- If it moves <1 °F, the pressure signal is genuinely buried and the metric drops to trend-logging (handoff §5.4's downgrade branch).
- Regress `spread` against `avgAll` **within** a session. If the pedestal is proportional to how hard the tire is working, `spread` rises with `avgAll` and the baseline should be modeled as `k × (avgAll − ambient)` rather than a constant °F. This decides §3.2(b) directly.

**(c) Establish the noise floor.** Frame-to-frame standard deviation of `spread`. If per-frame noise exceeds the pressure-driven difference from (b), determine the averaging window (or lap/corner count) needed to resolve it, and make the firmware average over that window before rendering a verdict. A twitchy per-frame indicator is worse than a slow correct one.

**(d) Audit the crop from the data.** Check each shoulder band's **absolute** temperature against the center band and against ambient. A shoulder sitting near ambient rather than near tire temp means the margin is a column or two too wide and background is diluting the band. The median resists this but does not defeat it once contamination exceeds half the band. This is a cheap, decisive check that requires no hardware.

**(e) Forensics on the validity filter.** Detect runs of *identical consecutive values* per band. The sticky-last behavior plus the C/F bug in §3.1(a) predicts a specific, falsifiable signature: **bands 0 and 1 should latch more often than band 2.** If that asymmetry is visible in the dumps, §3.1(a) is confirmed in the field. Also count frames where a corner collapsed to a single channel (§3.1h).

**(f) Segment by driving state, if the logger has speed or lateral-g.** A straight-line steady-state reading is cooling-dominated; the pressure signal is strongest in **sustained cornering**, where the loaded shoulder is doing real work. Session-mean spread may wash out exactly the moments that carry information. If the dump has the time resolution the handoff §6 asks for, compare cornering-phase spread against straight-line spread. This may recover signal that session means destroy.

**(g) Recompute the verdict under each candidate rule** and plot verdict-vs-time for both sessions:
1. Current rule (`pct × avgEdge`)
2. Mean-anchored (`pct × avgAll`)
3. Rise-normalized (`pct × (avgAll − ambient)`)
4. Baseline-subtracted with a fixed °F threshold
5. Baseline-subtracted **and** rise-normalized

**Success criterion: does any variant separate session 2 from session 3?** If none does, no threshold tuning will save the metric and the honest move is the downgrade in §6.

### 4.1 The cross-tire control — highest-value analysis available

Runs on a different tire, on a different day, where the problem did not occur. Same car, same camera, same firmware family. This is close to a natural experiment and it discriminates between hypotheses that the ECF data alone cannot.

**The prediction, and it is falsifiable.** Per handoff §1 the previous tire was an EC02 **205/45-17** on the same **17x8** wheel. A 205 sits at or above the *top* of its recommended rim range on an 8" wheel — tread pulled flat, or even shoulder-biased. A 225 sits at the *narrow* end — tread crowned. So:

> **If the structural-crown hypothesis is right, the EC02 runs should show a pedestal near zero or slightly negative, while the ECF runs show +15–20 °F.**

If that holds, it is near-confirmation, and the fix is a per-tire baseline that you re-derive whenever the tire or wheel changes.

**Run the full §4.0 battery on the control set, then compare:**

| Control-set result | Interpretation | Consequence |
|---|---|---|
| Pedestal ≈ 0 | Pedestal is **tire-specific** → structural crown | Per-tire baseline is correct and sufficient. Tool works after §3.2(a). |
| Pedestal positive but under threshold | Pedestal is **common-mode** (sensor/geometry/cooling), ECF merely has more | Baseline must scale with conditions → rise-normalized form, §3.2(b) |
| Pedestal ≈ same as ECF, but never tripped | Not a pedestal difference at all — a **threshold** difference | Re-examine: were temps lower that day, making `pct × avgEdge` a larger bar? |

**Confounds to check before trusting the comparison** — list them explicitly in the analysis output:
- Ambient and track temperature (drives the shoulder-cooling term hardest)
- Average tire temperature per session (a cooler day inflates the percentage threshold)
- **Were the crop offsets re-set when the tire changed?** Section width changed from 205 to 225, so the tread occupies a different number of pixels. If stale offsets were reused, band boundaries sit differently on the tread. Note the direction: offsets tuned for the *narrower* 205 and left in place would crop **into** the 225's tread, moving shoulder bands inboard and **reducing** measured spread — that masks the problem rather than causing it, so it does not explain the observation, but rule it out anyway. The dangerous direction is offsets too *small*, which admits background into the shoulders.
- Firmware version — check `git log` between the two dates for changes to `TempReader` / `ThreeSectionTire`
- Session length and pace (8th career event; pace has presumably risen)
- Which corners are being compared — compare like for like

### 4.2 Data hygiene

Everything above should be reproducible from a script, not a spreadsheet. See §6.

---

## 5. Testing — hardware

### 5.1 Bench: the uniform-target test *(do this first — it is the cleanest test in the whole plan)*

Point the camera at a **large, uniform-temperature, matte surface** filling the frame — a flat sheet warmed evenly, or a pan of warm water, ideally at ~150 °F to sit in the operating range. Run the normal pipeline and record the three band medians.

**A uniform target has zero true spread. Any nonzero band spread here is pure instrument-plus-algorithm bias**, with no tire physics involved.

| Result | Means |
|---|---|
| Spread < ~2 °F | Algorithm and optics are clean. The 15 °F pedestal is real tire physics. Band-geometry hypotheses are dead — do not build arc-length correction. |
| Spread 5–15 °F center-hot | There **is** a built-in instrument bias. Revisit lens/vignetting and band definition; the median-robustness argument in §2 is wrong somewhere. |

This single test decides whether §2's demotion of the band-geometry hypothesis was correct. It costs an hour in a garage.

### 5.2 Bench: warm-tire static cross-check

Drive briefly to warm the tires, park, and immediately read one tire with **both** the camera and a **needle pyrometer** (the standard motorsport tool — probe into the rubber at inner / center / outer). A handheld IR gun is an acceptable fallback but shares the camera's surface-reading and emissivity assumptions, so it is a weaker check.

This separates two things the camera cannot separate on its own:

- Needle shows center-hot too → **the tire really is center-hot**; the camera is honest and the pedestal is physical (crown and/or cooling)
- Needle shows flat, camera shows center-hot → **the camera or algorithm is generating the pedestal**; go back to §5.1

Note that the needle reads *bulk* rubber temperature and the camera reads *surface skin*. That difference is itself informative: a large camera-vs-needle discrepancy concentrated at the shoulders is direct evidence for the **lateral cooling** hypothesis (§2, live #2), because cooling affects skin far more than bulk.

### 5.3 Bench: crop verification, all four corners

With `showPixelOffsets` enabled, confirm the margin lines land on the actual tread edges for each corner. **Record the offsets per corner and put them in the event manifest** (§6). Re-do this any time tires, wheels, or camera mounts change — and log it, per §3.3(b).

### 5.4 Track: ground truth

Unchanged from handoff §7, and still the only check downstream of no assumptions:

**Chalk the outer shoulders.** One lap out, come in, inspect. Wear past the shoulder onto sidewall = too low; wear only in the center third = too high; even = there.

Run it alongside a **deliberate pressure sweep — 29 / 31 / 33 hot, one session each** — and at each pit-in capture, per corner: chalk photo, needle pyrometer I/C/O immediately on entry, and the camera dump. Record ambient and track temp per session.

Three sweep points plus the two existing sessions gives five points against which to fit the baseline and validate that the corrected metric tracks pressure. **This is the session that makes the tool trustworthy.** Do §4 and §5.1–5.2 before it so the sweep is spent confirming a hypothesis rather than forming one.

Independent of all debugging, handoff §8 stands: **run 31 psi hot.**

---

## 6. Decision table

| Finding | Conclusion | Action |
|---|---|---|
| Uniform-target spread < 2 °F **and** control tire pedestal ≈ 0 | Pedestal is real, tire-specific crown | Ship §3.2(a) per-tire baseline. Tool works. Re-baseline on every tire/wheel change. |
| Uniform-target spread < 2 °F **and** control tire pedestal also large | Pedestal is common-mode physics (lateral cooling) | Baseline must be condition-scaled — rise-normalized, §3.2(b) |
| Uniform-target spread large | Instrument/algorithm bias exists | Reopen band geometry and optics; §2's demotion was wrong |
| Session `spread` differs 3–6 °F between 28 and 31 psi | Pressure signal is present and recoverable | Baseline + fixed °F threshold, ~5 °F to start |
| Session `spread` differs <1 °F under **every** rule in §4.0(g) | Signal is buried | **Downgrade honestly:** keep the display as a trend log, remove the inflation recommendation. Do not ship a number that does not measure what it claims. |
| Needle flat, camera center-hot | Camera-side artifact | §5.1, then optics |
| Bands 0/1 latch more than band 2 in the dumps | §3.1(a) C/F bug confirmed in the field | Fix and re-baseline — prior data may be partly corrupted |

---

## 7. Data & analysis structure (handoff §9)

Recommend, starting minimal:

```
data/
  2025-06-XX-limerock-tnia/
    manifest.yaml
    session2-28psi.nbp
    session3-31psi.nbp
  <control-event>/
    manifest.yaml
    *.nbp
analysis/
  parse_nbp.py        # NBP → tidy DataFrame
  pedestal.py         # §4.0 battery
  compare_tires.py    # §4.1
```

`manifest.yaml` per event — this is what makes the data interpretable later, and the cross-tire comparison is impossible without it:

```yaml
date: 2025-06-XX
track: Lime Rock Park
event: SCCA TNiA (evening)
ambient_f: 68
track_temp_f: null
car: ND Miata, ~2400 lb
tire: {model: Continental ECF, size: 225/40-17, wheel: 17x8}
sessions:
  - {file: session2-28psi.nbp, hot_psi: 28, cold_psi: 23.5, feel: "clearly under"}
  - {file: session3-31psi.nbp, hot_psi: 31, cold_psi: 23.5, feel: "good"}
crop_offsets: {fl: [L,R], fr: [L,R], rl: [L,R], rr: [L,R]}
firmware_sha: <git sha>
notes: |
  ...
```

Gitignore the bulk `.nbp` files if they are large; **always track the manifests**. Once §3.3(b) lands, the firmware emits most of the manifest itself and this becomes bookkeeping rather than transcription.

---

## 8. Suggested order of work

1. §3.1(a) and (b) — the C/F bug and the uninitialized `tj`. Real bugs, small diffs, and (a) may have corrupted existing data.
2. §4.0 + §4.1 offline — **no hardware needed, highest information yield.** The cross-tire control is the most valuable dataset available.
3. §5.1 uniform-target bench test — one hour, decides the band-geometry question outright.
4. §3.3 instrumentation — before the next event, so the next dump is self-describing.
5. §3.2 algorithm changes — informed by 2 and 3, not ahead of them.
6. §5.2 / §5.3 bench verification.
7. §5.4 track sweep with chalk and needle — confirmation, not discovery.

**The concept is sound.** Cross-tread thermal spread is a legitimate contact-pressure proxy. The metric's zero is in the wrong place, and there is a plausible route to putting it right.

---

## 9. Open questions for the driver

- Camera mount geometry — distance to tread and viewing angle. Kills or resurrects the emissivity hypothesis (§2) definitively.
- Approximately where in the wheel's rotation does each camera look — how far past contact-patch exit? Drives the magnitude of the lateral-cooling term (§2, live #2).
- Were the crop offsets re-set when the tires changed from EC02 205 to ECF 225? Load-bearing for §4.1.
- Ambient and track temperature for **both** events, if recorded.
- Does the logger carry speed or lateral-g on the same time base as the NBP channels? Determines whether §4.0(f) corner segmentation is possible.
- Handoff §3 lists session 3 as reporting over-inflation "believed, needs confirming." Confirm from the dump.
