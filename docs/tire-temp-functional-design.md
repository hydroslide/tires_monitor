# Tire-Temp Monitor — Functional Design & Findings

**Status:** supersedes the core conclusion of [`tire-temp-algorithm-recommendations.md`](tire-temp-algorithm-recommendations.md).
**Grounded in:** offline analysis of the logged track data — Lime Rock Park June 17 & July 16 2026 (Continental **ECF** 225/40-17, the subject tire) and Thompson Speedway May 21 2026 (Continental **EC02** 205/45-17, the control) — plus a three-front literature review of motorsport tire-temp practice.
**Scope:** what the product should *do* and *show*, and why. Functional/product level, not code.

The recommendations doc hypothesized a uniform **+15 °F center-hot pedestal** pinning every corner to OVER. **The data refutes that.** There is no uniform pedestal; the cross-tread spread the firmware keys on is measuring **cornering load and body roll, not inflation pressure.** This document records what the data actually showed, reconciles the surface-vs-carcass physics, and lays out a functional redesign that points the hardware at what it is genuinely good at: warm-up window, overheating, and thermal balance.

---

## 1. Purpose

The monitor reads four MLX90640 thermal cameras (one per corner), splits each tire into Outer / Center / Inner bands, and today infers over/under-inflation from the center-vs-shoulder temperature spread. The driver reported the **left tires pinning OVER** at Lime Rock and dropped pressure in response — which made the car worse. This document explains why, using the logged data, and specifies the redesign.

Sign convention used throughout (matches `ThreeSectionTire.cpp`):

> **delta = avg(Outer, Inner) − Center.**
> **delta < 0 → center hotter → OVER-inflation. delta > 0 → shoulders hotter → UNDER-inflation.**

---

## 2. Executive summary

- **Everything new is gated to "Track" mode.** The whole redesign activates only when the menu mode is **Track** (`currentMode == 1`); in **Street** mode the new capture, gating, alerts, summary, and calculated-display behavior stay inert and their menu items hidden, so the everyday street display is unchanged.
- **The spread measures LOAD, not pressure.** Under cornering load the left-tire spread balloons to ~−18/−19 °F center-hot; on straights it sits near −3/−6 °F. The verdict tracks how hard the tire is working, not how much air is in it.
- **The OVER pin is a mid-corner body-roll geometry artifact.** As the car rolls and steers, the camera's crop bleeds onto cooler sidewall, manufacturing center-hot that scales with lateral g. It is *opposite* to real load physics (a loaded outside tire should read outer-shoulder-hot).
- **The metric is pressure-blind.** Raising hot pressure 28.5 → 31 psi — a change the driver clearly felt (mushy → good) — moved the spread by **≤0.5 °F**.
- **The device's advice was backwards.** It called the lefts OVER; the tire actually wanted *more* pressure. Acting on it (dropping to 28.5) made the car mushy/slidy.
- **Surface ≠ carcass.** The camera reads skin temperature, which runs **10–40 °F cooler** than the carcass a needle probe reads and spikes/decays far faster. Published windows (e.g. ECF 160–200 °F) are carcass-frame; comparing raw surface numbers to them makes tires look permanently cold — and may hide genuine overheating.
- **Repurpose the tool** for the two things real-time IR uniquely does — **warm-up / operating-window** and **overheat** detection — plus **front/rear & left/right thermal balance**. Demote the live inflation verdict.
- **The IMU lateral-g gate is the mechanism-specific fix.** Because the artifact is tightly coupled to lateral g, suppressing inflation reads while cornering removes ~24 of the −27 °F and leaves the honest ~−3 °F straight-line residual.

---

## 3. What the data showed

Analysis scripts live in the **`tires_data`** repo: `analyze_spread.py`, `analyze_corner_load.py`, `analyze_corner_exit.py` (parser `racerender.py`, lap slicing `isolate_lap.py`). All read the TrackAddict CSV exports, which merge the monitor's NBP tire channels with GPS, lateral g, and speed on one timebase. **RR is excluded everywhere** (unreliable hardware, per the driver).

### 3.1 Straight-line residuals (the honest inflation read)

At the *good* pressure (31 psi), warm, straight, steady frames only:

| Corner | Straight-line delta | Reading |
|---|---|---|
| FL | ≈ −3 °F | ~neutral / very mild center-hot |
| RL | ≈ −6 °F | mild center-hot (its per-corner baseline) |
| FR | ≈ +4 °F | mild shoulder-hot (slightly under) |
| RR | ~0 | discounted (hardware) |

These are modest and mixed — **nothing here says "drop pressure."** And because surface IR over-reports center-hot (§4), the true carcass picture is even more shoulder-biased, i.e. consistent with "31 psi felt good."

### 3.2 Load balloons the spread; pressure does not move it

`analyze_corner_load.py`, split by lateral g (left tires load in right-handers, `Accel Y < −0.6`):

| Corner | Straight | Loaded @ 28.5 | Loaded @ 31 | Move for +2.5 psi |
|---|---|---|---|---|
| FL | −2.5 | −19.5 | −19.0 | **+0.5** |
| RL | −6.5 | −17.5 | −18.0 | **−0.5** |

The load effect is ~15 °F; the pressure effect is ~0.5 °F. The tool is a load gauge.

### 3.3 The firmware verdict, reproduced

`analyze_spread.py` reproduces the device rule exactly (int rounding, 10 %/15 % thresholds) over warm, moving frames:

| Corner | ECF OVER-verdict rate | Note |
|---|---|---|
| FL | ~26–27 % (up to **30 %** on one hot lap) | reads OVER |
| RL | ~28–29 % (up to **38 %** on one hot lap) | reads OVER (worst) |
| FR | OVER 0 %, UNDER 5–6 % | reads the *opposite* of the lefts |

Lime Rock is 6-of-7 right-handers, so the left tires are loaded most of the lap → the OVER verdict is on most of the lap. That is the reported pin, mechanistically explained.

### 3.4 Corner-exit event-triggered average — the artifact isolated

`analyze_corner_exit.py` smooths the (shaky-mount, noisy) lateral-g channel, detects ~75 hard right-hander exits per session, and averages the spread around each exit (t = 0 at exit):

| Phase | t vs exit | FL center-hot delta |
|---|---|---|
| Mid-corner, peak lateral g | −2.2 s | **−27 °F** (max) |
| Straightening | −0.2 s | −15 |
| Exit (g ≈ 0) | +0.2 s | −11 |
| Onto the straight | +0.8 s | −5 |
| Steady straight | +1.5 s | **−3** (baseline) |

Findings:

- The center-hot **peaks mid-corner at peak lateral g and decays monotonically through the exit** to the straight-line baseline ~1.5 s after the car is straight. **There is no exit spike** — the "shoulders shed at exit" transient hypothesis is not what drives it.
- It **tracks lateral g with ~0.5 s lag** → a body-roll/steer geometry artifact (crop bleeding onto sidewall at roll angle), which vanishes when the body returns level.
- It is **opposite to real load physics**: a loaded outside tire should read *outer-shoulder-hot*; reading strong *center*-hot instead is the tell that the measurement is geometrically compromised.
- It is **pressure-independent** (28.5 ≡ 31, both corners, both sessions).
- A small **~1 s tail** after straightening is consistent with camera latency and/or a minor real shoulder-cooling component — so the driver's physical intuition is present, just not dominant.
- **Perception reconciled:** the alert is latched on through most of the corner and releases ~0.2 s after straightening — exactly when the driver first has a moment to glance at the dash. "I only saw it briefly coming out of corners" is real; the *alert* wasn't brief, the *chance to look* was.

### 3.5 Control tire

On the EC02 at Thompson the left-side bias largely disappears (FL −3.5 → −1.5, RL −8 → −2.5; OVER-rate 26–28 % → 2–5 %). Consistent with a tire-specific component, **but confounded by the different track** (Thompson has a different corner balance), so most of the gap is likely load/track rather than tire. Re-run the load split on a like-for-like track before crediting a purely tire-specific pedestal.

---

## 4. Physics: surface vs carcass, and the two-regime rule

Three independent literature threads converge on why a surface camera disagrees with the classic pit-lane needle method.

**Surface ≠ bulk, and grip lives in the bulk.** A needle probe reads compound temperature 3–5 mm down — the hysteretic heat that correlates with grip. IR reads only the skin, which "fluctuates rapidly with dynamic loading" and is *"badly correlated with grip,"* while the core "changes at a much slower rate" ([MegaRide](https://www.megaride.eu/press/tire-thermal-model/); [Izze Racing white paper](https://www.izzeracing.com/ewExternalFiles/Izze_Racing_White_Paper_Tire_Temperature.pdf)). Corner-by-corner spikes are a surface phenomenon the carcass never sees.

**IR reads cooler, edges worst.** Surface IR reads **10–40 °F cooler** than a probe, and *"the edges cool fastest"* ([Speedway Motors](https://www.speedwaymotors.com/the-toolbox/pyrometers-probe-vs-infrared/29996); [Prisma Electronics](https://prismaelectronics.com/blogs/tire-lab/how-to-measure-tire-temperature-in-motorsport-infrared-pyrometer-vs-needle-probe)). This is largely a *timing/cooling* effect: read instantly and statically the two agree to ~0.3 °C, but the shoulders shed heat to the airstream in the seconds after the contact patch — a **built-in center-hot bias** independent of load.

**Roll/steer crop-bleed.** A chassis-mounted sensor cannot see the contact patch; it sees tread that has rotated up and out. Under steer/roll the outer channels *"bleed into the sidewall … when steering angle is applied"* ([Texense](https://www.gomuchfaster.com/products/irn4c-f1-200-c-ir-tire-temp-sensor)), and curved shoulders viewed obliquely read low because directional emissivity falls past ~30–40° off-normal ([Optris](https://optris.com/us/knowledge-library/emissivity/)). This is the mechanism our corner-exit data fingerprinted.

**The two-regime rule (the practical upshot):**

| Regime | Tire state | What the center-vs-shoulder delta means | Trust it? |
|---|---|---|---|
| **Straight / flat / steady** | square, even load | inflation (center-hot = over, both-edges-hot = under) | **Yes** — this is the inflation regime |
| **Mid-corner / rolled** | on the outer edge | camber signal + roll/geometry artifact | **No** — do not read inflation here |

The classic Inner/Center/Outer matrix is confirmed valid ([JOES Racing](https://www.joesracing.com/tires-temperatures/): *"the middle temp is your pressure gauge and the inner-to-outer delta is your camber gauge"*; [949racing/Supermiata](https://949racing.com/supermiata/tech-info/supermiata-using-a-tire-pyrometer/); [Northstar Motorsports](https://northstarmotorsports.com/pages/tech-tips-understanding-tire-temperatures); [Autosport Labs](https://www.autosportlabs.com/using_tire_temperatures_for_better_grip_and_faster_lap_times/)) — **but it is a needle/bulk, tire-read-flat methodology.** Applied to surface IR mid-corner it inverts, which is exactly how the driver got misled.

---

## 5. Functional design

### 5.0 Global constraints (apply to every feature below)

**Track-mode gating.** Every feature in this section is enabled **only in the menu's "Track" mode** (`currentMode == 1`, read via `getCurrentModeValue()`; the other mode is "Street"). In Street mode the new behavior is inert and its menu items hidden or disabled, so the everyday street display is unchanged. **Track mode is the single master switch for this redesign.** (This is the Street/Track `currentMode` — distinct from the thermal-display `thermalMode`, which is a separate setting.)

**UI/UX latitude.** The hand-built menu, touch handling, and display have rough edges. The implementer has explicit latitude to redesign menu UI/UX where it serves these features and to fix wonky, broken, or buggy behavior found along the way — record any such fix in the relevant issue (or file a new one) so it stays tracked.

### 5.1 (a) Inflation verdict — demoted

- **Off by default.** The segment-delta indicator is already a menu toggle; keep it off.
- If used at all: **straight-line-gated** (only computed when the IMU says the car isn't cornering, §5.4), presented as a **between-session hint**, never a live in-corner directive.
- The capability is retained for data/diagnostics, not for telling the driver to change pressure in the moment.

### 5.2 (b) Front/Rear & Left/Right balance

Balance compares each tire's **working temperature** (the smoothed/calculated value of §5.5, not raw spiky surface) across two pairings:

| Pairing | Compare | Interpretation |
|---|---|---|
| **Front / Rear** | avg front vs avg rear | fronts hotter → understeer; rears hotter → oversteer |
| **Left / Right** | avg left vs avg right | expected lopsided at a directional track (Lime Rock loads lefts); flags the *unexpected* (corner-weight, brake bias, one-side abuse) |

- **Show the pair, not just the delta** — absolute levels matter (fronts +15 °F with everything in-window ≠ fronts +15 °F with cold rears).
- **Exclude RR** from the averages so its bad hardware doesn't poison the numbers.
- Primarily a summary/between-session readout; an optional coarse live front/rear bias arrow is a nice-to-have.

### 5.3 (c) Session-end summary

A per-corner recap, menu-accessible, readable while stationary, roughly current font size.

- **Layout:** a 2×2 car map (FL FR / RL RR). Each cell: **peak temp** (large) + **average steady temp** (small) + **time-in-window** (color or tiny bar) + **overheat flag** (exceeded threshold, and roughly how long).
- **Session-level screen:** F/R and L/R balance (§5.2); **warm-up time** (laps/minutes until all four first reached the window).
- **Session lifecycle:**
  - **Start** — menu "Start Session" action (reliable); optional motion auto-start.
  - **End / Seal** — menu action the driver hits **at the start of the cooldown lap**; it freezes accumulation (peaks, averages, balance, time-in-window) at that instant so the cruise back and paddock idle don't dilute the stats or add a fake cooldown tail.
  - **Auto-seal backstop** — seal on sustained low speed / stationary if the driver forgets; optional "trim last N seconds" so a late press still cuts the cruise.
  - **History** — keep the last 1–3 sessions for on-device comparison (e.g. S2 vs S3).

### 5.4 (d) IMU (QMI8658C, 6-axis, I2C)

The board carries an onboard QMI8658C accelerometer + gyro. Dash-mounted, it reads the car's motion — the lateral-g signal we need.

- **Capture gate (the core use):** accumulate the inflation read **only when |lateral g| is below a threshold** (straight and steady, not hard braking). Cornering frames are suppressed — that is where the artifact lives. Empirically this strips ~24 of the −27 °F and leaves the honest ~−3 °F baseline.
- **Debounced, latched alerts:** during capture, accumulate *time-in-over* / *time-in-under* per corner; once a corner exceeds a **dwell-time** threshold, raise the alert and **latch** it until the condition clears (also judged during capture). Replaces the flickery per-frame verdict with a stable, physically valid one.
- **Auxiliary uses:** cornering/braking detection to know which tires are loaded (feeds balance); motion-based session auto-start; stationary auto-seal.
- **One-time orientation calibration:** learn how the box is mounted (sit still = level; drive straight + brake = forward axis), or pick axis/sign in the menu. Coarse is fine — we only need straight-vs-cornering.

### 5.5 (e) Surface → carcass "calculated" mode

The window mismatch (surface reads ~10–40 °F below the carcass-frame spec) is fixed the way the driver proposed.

- **Offset K vs window-shift — equivalent for the decision.** Adding K to the reading or subtracting K from every threshold yields the same in/out-of-window call. **Offset the reading up** for readability (the displayed number becomes comparable to published specs and the driver's mental model). **Store windows in one reference frame (carcass)** so surface numbers are never compared against carcass thresholds — the trap the tool is in today.
- **Smoothing τ = carcass thermal-inertia model.** The carcass is not just hotter, it is *slower*. A low-pass with time constant τ makes the displayed value rise and fall gradually like the carcass, killing the transient "purple" spikes that falsely read as in-window. This is a **better** overheat guard, not a worse one: genuine overheating is *sustained* and survives the filter, while a brief surface spike that doesn't move the smoothed value isn't cooking the carcass.
- **Raw vs Calculated modes (menu):** Raw = honest surface, unsmoothed (bench/debug/data). Calculated = offset + smoothed carcass estimate (driving). **Always log raw** (or raw + the active K/τ) even in calculated mode, so calibration can be re-derived offline — never discard the raw signal.
- **Calibration:** per-profile defaults + an **immediate needle cross-check** to anchor K (probe a tire center the instant you come in, diff against what the camera showed) + an **offline τ fit** from the logs. K drifts with tire/wear/speed/ambient, so it is a profile value, never one magic number.
- **Tire profiles** bundle the tire-specific values: window thresholds, offset K, smoothing τ, and (since #15) the **per-corner camera crop offsets**. Selecting a profile (ECF, EC02, …) swaps all of them at once. This is *why* ECF and EC02 read differently — they are different tires with different windows. *(The per-corner inflation baseline was also bundled here originally; it was removed in #18 — see "Amendments" at the end of this document.)*

---

## 6. Calculated τ and K (from the data)

**Smoothing time-constant τ = 15 s (range 10–20 s).** In "calculated mode" the raw surface center-temp is EMA-low-passed before display so the number behaves like the slower carcass. τ is chosen to kill corner-frequency ripple while preserving the warm-up/cooldown trend. Sweeping τ over the July-S3 log (FL/RL/FR center band, warm & moving, 0.1 s resample) and evaluating a first-order low-pass's analytic gain at a ~10 s corner cycle:

| τ | corner transient kept | warm-up lag |
|---|---|---|
| 5 s | 30 % | 5 s |
| 10 s | 16 % | 10 s |
| **15 s** | **11 %** | **15 s** |
| 20 s | 8 % | 20 s |
| 30 s | 5 % | 30 s |

τ = 15 s knocks the corner ripple to ~11 % of amplitude (no more transient "purple" flashes) while lagging a 60–120 s warm-up by only ~15 s. Below 10 s leaves too much ripple; above 20 s adds real warm-up lag for little gain. Store τ per tire profile so it can be tuned. (Intrinsic surface-cooldown τ couldn't be fit directly — these logs end while still rolling out, so there's no clean cooldown tail, and Intake Air Temp is engine-heat-soaked and unusable as the ambient asymptote.)

**Offset K = +20 °F (default, must be calibrated).** `displayed_carcass_estimate ≈ EMA₁₅(surface) + K`. K is a **literature default only** (surface reads ~10–40 °F below a needle probe; +20 is the midpoint). There is **no carcass ground truth in the logs**, so K must be anchored per tire by an immediate needle-pyrometer cross-check (probe the center hot, within seconds of pit-in; the camera-vs-probe gap at that instant is K). Seed each tire profile at +20 and refine on the first needle session; expect K to drift with tire, wear, speed, and ambient.

Computed by `tires_data/src/fit_tau.py`.

---

## 7. Next Steps

This design will be turned into GitHub stories **interactively, feature by feature**:

1. We walk the backlog (§8) one item at a time.
2. For each, a story body is drafted (problem, behavior, settings, acceptance).
3. **We pause on each story so the driver can tweak details before it is filed** — there are specific tweaks intended per feature, so nothing is filed without a review beat.
4. Stories are created only after that review.

Preconditions and mechanics:

- **`gh` is authenticated** (account `hydroslide`); the interactive story pass can run whenever the driver is ready.
- Stories will be filed as **`tires_monitor` GitHub issues** (firmware repo). GitHub Issues is not yet configured as this repo's tracker — the first pass should set that up (labels/states) before filing.
- The **`tires_data` analysis repo is published** at [`hydroslide/tires_data`](https://github.com/hydroslide/tires_data) (private) — flip to public or rename anytime.
- Story authoring happens in its own pass; the backlog below is the slate, not the filed stories.

---

## 8. Stories / Backlog (to be filled)

Story bodies are written during the interactive Next-Steps pass (§7); this is the candidate slate:

| # | Candidate story | Area |
|---|---|---|
| 1 | Session lifecycle — start / seal-during-cooldown / summary + history | §5.3 |
| 2 | IMU capture gate + debounced latched over/under alerts | §5.4 |
| 3 | Surface→carcass calculated mode (offset K + smoothing τ) + raw mode + always-log-raw | §5.5 |
| 4 | Tire profiles (window thresholds + K + τ + per-corner baseline) | §5.5 |
| 5 | Front/Rear & Left/Right thermal balance readout | §5.2 |
| 6 | Demote/gate the inflation verdict (straight-line-only, between-session) | §5.1 |
| 7 | Correctness bugs — C-vs-F validity-filter comparison, uninitialized `tj` loop var, `initialize()` from base ctor, `anySectionColorChanged` missing return, byte-can't-store-0 EEPROM, integer-division band skew, `draw()` early return, zero-as-no-data in NBP | firmware §3.1 |
| 8 | NBP instrumentation channels + segment colors for the downstream video renderer | firmware |

---

## 9. References & provenance

**Repositories**
- `tires_monitor` — firmware (this repo; branch `pressure-fix`). Classifier in `tires_esp32/ThreeSectionTire.cpp`; band extraction in `tires_esp32/TempReader.cpp`; NBP encoder in `tires_esp32/NBPProtocol.cpp`.
- `tires_data` — offline analysis (separate repo, local). Parser `src/racerender.py`; analyses `src/analyze_spread.py`, `src/analyze_corner_load.py`, `src/analyze_corner_exit.py`; lap slicing `src/isolate_lap.py`; field dictionary `FIELDS.md`; session inventory `data/manifest.yaml`.

**Data set**
- Lime Rock 2026-06-17 (ECF, 4 sessions) and 2026-07-16 (ECF, 3 sessions: S1 mixed/off-transit ~28.5–32, S2 hot 28.5 "mushy/under", S3 hot 31 "good"); Thompson 2026-05-21 (EC02 control). Source CSVs are TrackAddict exports from `~/Documents/RaceRender 3/`.

**Research sources**
- MegaRide tire thermal model — https://www.megaride.eu/press/tire-thermal-model/
- Izze Racing white paper (surface vs core) — https://www.izzeracing.com/ewExternalFiles/Izze_Racing_White_Paper_Tire_Temperature.pdf
- Prisma Electronics (IR vs needle) — https://prismaelectronics.com/blogs/tire-lab/how-to-measure-tire-temperature-in-motorsport-infrared-pyrometer-vs-needle-probe
- Speedway Motors (probe vs infrared) — https://www.speedwaymotors.com/the-toolbox/pyrometers-probe-vs-infrared/29996
- Texense IRN4C-F1 (sidewall bleed under steer) — https://www.gomuchfaster.com/products/irn4c-f1-200-c-ir-tire-temp-sensor
- Optris (directional emissivity) — https://optris.com/us/knowledge-library/emissivity/
- JOES Racing (pressure vs camber gauges) — https://www.joesracing.com/tires-temperatures/
- 949racing / Supermiata (pyrometer use) — https://949racing.com/supermiata/tech-info/supermiata-using-a-tire-pyrometer/
- Northstar Motorsports (temp interpretation) — https://northstarmotorsports.com/pages/tech-tips-understanding-tire-temperatures
- Autosport Labs (using tire temps) — https://www.autosportlabs.com/using_tire_temperatures_for_better_grip_and_faster_lap_times/
- Izze Racing / AiM live systems & display practice; GRM & Miata forums for hot-pressure targets (see conversation research log).

---

## Amendments (post-implementation)

This document records the original design and the data analysis behind it. The findings
above still stand; the items below are where **shipped behavior deliberately diverges**
from what the design proposed. Read these before treating any statement above as current.

### #14 — the tire profile is the single source of truth for the window, in both modes

The design assumed Street and Track each carried their own Min/Ideal/Max. They no longer
do. Each *mode* now names a **default profile** (Street → EC02, Track → ECF) and the active
profile supplies the window, K and τ. The active selection is transient: it is resolved
from the current mode's default at boot and re-snapped on every mode change, and is never
persisted. Seed windows were retuned at the same time: **ECF 120/160/200, EC02 110/140/170**.

### #16 — Calculated display is available in both modes

The design (and the stories' global constraint) treated calculated mode as Track-only.
Since both modes now resolve K and τ from the active profile, the calc path has everything
it needs in Street too, and the mode gate was an artificial restriction. The **Display**
(Raw/Calculated) setting appears under both Street Settings and Track Settings and is one
shared global value, not a per-mode setting.

Still genuinely Track-only: the inflation indicator, the session/balance swipe features,
and boot-metadata mode labelling.

### #18 — the per-corner inflation baseline was removed

§3.1's measured straight-line residuals (FL ≈ −3, RL ≈ −6, FR ≈ +4, RR ~0 at 31 psi) are
**retained as data** — the measurement was sound. What was wrong was promoting them to a
per-profile calibration constant:

- **They are a track fingerprint, not a property of the car.** FL heaviest, RL next, FR
  opposite sign is Lime Rock's load pattern (clockwise; left tyres loaded, FR barely
  worked). Static camber would push FL and FR the *same* direction — these push opposite
  ways. And while §5.4's geometry artifact decays ~1.5 s after the body levels, *heat* does
  not: surface thermal constants run seconds to tens of seconds and Lime Rock's straights
  are short, so "warm, straight, steady" frames there still carry heat from the corner just
  exited. The values do not transfer to another track.
- **The artifact they appeared to correct is already gone on a straight.** Removing it is
  the capture gate's entire job (§5.4); the baseline added nothing on top.
- **There was no way to compute them** — no capture routine, 12 hand-entered numbers
  (4 corners × 3 profiles) derived from one session at one track.
- **The one genuinely static component has a better home.** A camera aimed slightly off, so
  its "center" band is not really tyre center, is real and track-independent — but that is
  an error measured in **pixels**, and #15 gave every profile per-corner crop offsets.
  Correcting it with a degrees-F offset fixes the wrong variable: wrong unit, and it shifts
  only the edge−center *delta* while leaving every displayed band temperature still wrong.

The inflation verdict now uses a plain `edge − center` spread against the Inflation Delta %
threshold, still gated to straight-line frames. **If residual cornering heat later proves to
bias the verdict, the fix is a straight-line dwell** — require N seconds continuously
straight before a frame counts — which is track-independent and needs one global number
rather than twelve per-profile ones, and extends the existing latch dwell.

### #18 — Tire Profiles menu: no `Load` / `Save Prof`

The edit buffer now follows the `Profile` selector automatically, so the manual `Load` step
is gone. It had been papering over a real bug: `MenuValueBinding` has no change-callback, so
changing the selector left the buffer showing the *previous* slot's values, and saving then
wrote them into the newly selected slot. Root **Save Settings** already commits the edit
buffer and writes all three slots, so the profile-only save was a strict subset of it.
`Reset` remains. Note that switching profiles now discards uncommitted edits — normal
select-record→edit→save semantics, and strictly better than silently corrupting a slot.
