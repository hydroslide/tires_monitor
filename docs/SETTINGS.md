# Settings reference

Every menu setting on the device: what it is, where it lives, its default and range, **what
it's for**, and how to tune it. If you can't remember why a setting exists, this is the file
to read — the design rationale behind the tire-temp maths lives in
[`tire-temp-functional-design.md`](tire-temp-functional-design.md).

Kept current as part of finishing a feature (see `CLAUDE.md`) — if you change a setting in
firmware, change the row here in the same commit.

---

## How settings are stored

**Edits take effect immediately; `Save Config` makes them survive a reboot.** Every setting
is a live variable the menu writes straight into. The root-level **`Save Config`** item is
what writes them to EEPROM. Change something, back out of the menu, and it applies right
away — but power-cycle without saving and it reverts.

`Save Config` writes **everything at once**: all the menu settings *and* all three tire
profiles. You can edit profile A, switch to B, edit B, then save once and both persist.

Two independent "have these ever been written" magic bytes guard the EEPROM. **When either
is bumped in firmware, that whole group resets to defaults on the next boot** — this is
deliberate, and it's why a flash sometimes wipes your settings:

| Group | Magic | Bumped when |
|---|---|---|
| Menu settings | `SETTINGS_MAGIC`, currently `0xA8` | a setting moves address or changes meaning, **or a new setting claims an address no earlier firmware wrote** |
| Tire profiles | `PROFILE_MAGIC`, currently `0x5D` | the `TireProfile` struct layout changes |

> `SETTINGS_MAGIC` went `0xA7` → `0xA8` in #20, which added *Gate Dwl 0.1s* and *Show G Bar*
> at EEPROM 32 and 33. Those addresses were never written by earlier firmware, and the
> loader copies stored bytes in **without range-clamping** — so a stale `0xFF` at 32 would
> have loaded as a 25.5 s capture dwell. The bump forces a clean re-seed instead. **Expect
> every menu setting to reset once on the first boot after that flash.**

**The active tire profile is deliberately not saved.** On boot it's set to the current
mode's *Default Profile*. See [Tire Profiles](#tire-profiles).

---

## Root menu

| Setting | Default | Range | What it's for |
|---|---|---|---|
| **Current Mode** | `Street` | Street / Track | Master mode switch. Selects which *Default Profile* is adopted, and enables the Track-only features (straight-line capture gating and the per-tire inflation verdict, session recording, balance readout). Changing it immediately re-snaps the active tire profile. |
| **Temp Scale** | `F` | F / C | Display units. Profile windows and Carcass Offset are authored in °F and converted for display, so switching scale doesn't require re-tuning a profile. |
| **Night Brightness** | `25` | 0–100 % | Screen brightness **while night mode is active** (swipe-toggled) — it does not affect normal daytime brightness. Percent of full backlight. Low enough not to blind you at night, high enough to still read. |
| **Test** | `On` | On / Off | Enables the raw **thermal-camera image** views. With it on, the swipe-cycled display has 3 states (off, or all four camera images live); with it off there are 4 states that cycle none → lower pair → upper pair → all four. Primarily a bench/diagnostic aid for checking a camera is aimed and reading sensibly. |

Plus the submenus below, and **`Save Config`** (see [How settings are stored](#how-settings-are-stored)).

---

## Street Settings / Track Settings

Both modes have the same two settings; Track has five more.

| Setting | Menu | Default | Range | What it's for |
|---|---|---|---|---|
| **Default Profile** | both | Street → `EC02`, Track → `ECF` | any profile slot | The tire profile this mode adopts. Selecting a mode snaps the active profile to its default — this is how switching Street↔Track swaps the whole temperature window in one move. A manual pick in Tire Profiles overrides it until the next mode change or reboot. |
| **Display** | both | `Raw` | Raw / Calculated | `Raw` shows the honest measured **surface** temperature. `Calculated` shows the estimated **carcass** temperature — the surface reading smoothed by *Carcass Lag* and offset by *Carcass Offset*. Use Calculated for driving (it's what the window thresholds are authored against); Raw for bench work and diagnosing the sensors. **One shared setting shown in both menus** — changing it in Street changes it in Track. |
| **Show Balance** | Track | `On` | On / Off | Whether the `Balance` readout below will open. Off makes the item report that it's hidden instead. |
| **Balance** | Track | — | action | Opens the front/rear and left/right thermal balance readout. |
| **View Summary** | Track | — | action | Re-opens the last sealed session summary. |
| **Auto-Seal** | Track | `Off` | On / Off | Automatically ends (seals) a running session after a sustained stationary period — a backstop for forgetting to swipe to end. Defaults off because it detects stillness from the IMU only (there's no speed input), so it's best-effort and can fire while you're sitting in the paddock. |

> **Note:** in Raw mode the displayed number is a *surface* temperature but the profile
> window is authored in the *carcass* frame, so tires will read roughly one Carcass Offset
> (~20 °F) colder than the window expects. That's inherent to Raw, not a bug.

---

## Camera Settings

Device-wide display behavior. (The per-corner crop offset *values* live on the tire profile
— see [Tire Profiles → Offsets](#offsets).)

| Setting | Default | Range | What it's for |
|---|---|---|---|
| **Show Offsets** | `On` | On / Off | **This is the crop-tuning toggle, and it changes what the thermal image shows.** *On* = the **full** camera frame with vertical guide lines marking where the crop offsets sit — use this while tuning the offsets so you can see what you're cutting. *Off* = the image is cropped to the region **between** the offsets and stretched to fill the display, which is the real working view. Turn it on to tune, off to drive. |
| **Thermal Gradient** | `On` | On / Off | *On* = smooth interpolated colour within each temperature band (finer detail). *Off* = one flat colour per band, so the image posterizes into cold/warm/ideal/hot blocks — easier to read *which band* a region is in at a glance. Preference, not calibration. |
| **Hi Freq Updates** | `Off` | On / Off | Redraws the tire display every read (~10 Hz) instead of once a second. Smoother, but far more redraw work. Leave off unless you're chasing something transient. |
| **Segment Deltas** | `Off` | On / Off | Paints per-band verdict colour bars under each tire, showing which band tripped the inflation or alignment check. Since #21 the **inflation** verdict painted here is the IMU gate's *latched, straight-line-only* one — it no longer flickers through corners — while the alignment fallback stays instantaneous. A diagnostic overlay: the colours are always computed and always logged over NBP, this only controls whether they're painted. |
| **Inflation Delta %** | `10` | 0–16 % | How far **edge-vs-centre** must diverge before a tire is called over/under-inflated. The comparison is `avg(outer, inner)` vs `centre`, and the threshold is this percent *of the edge temperature* (so ~16 °F at a 160 °F edge). **Lower = more sensitive**, more false calls; higher = only flags gross deviations. |
| **Alignment Delta %** | `15` | 0–16 % | How far **outer-vs-inner shoulder** must diverge before it's flagged as an alignment (camber) issue rather than a pressure one. Same percent-of-edge basis. **Only evaluated if the inflation check didn't already trip** — inflation takes priority. Note the default of 15 sits near the 16 ceiling, so it's currently a fairly insensitive check. |

---

## IMU Gate

The onboard 6-axis IMU (QMI8658C) suppresses tire judgements while cornering. This exists
because the mid-corner reading is a body-roll artifact — the camera crop bleeds onto the
sidewall at roll angle, manufacturing a fake centre-hot that scales with lateral g and is
*opposite* to real load physics. Gating to straight-line frames removes roughly 24 of the
27 °F of that artifact. Full analysis in the design doc §5.4.

**The two dwells are not the same thing.** *Gate Dwl* decides when capture **starts** — how
long the car must hold lateral g inside the zone before its readings count at all. *Dwell*
decides when an over/under verdict **latches**, measured only over frames already being
captured. One gates the input, the other debounces the output.

| Setting | Default | Range | What it's for |
|---|---|---|---|
| **Gate Enable** | `On` | On / Off | Master switch for straight-line gating. Off means inflation reads accumulate everywhere, including mid-corner — which is what produced the original backwards verdict. Leave on. |
| **Show G Bar** | `On` | On / Off | Paints the live lateral-g test bar in the gap between the front and rear tires (see below). It's how you tune everything else in this menu, and how you confirm the boot calibration was good. Costs a sliver of screen and nothing else — leave it on unless you want the cleanest possible display. |
| **Lateral cg** | `35` (0.35 g) | 10–100 (0.10–1.00 g) | Lateral-g threshold below which the car counts as "straight and steady" and readings are accepted. **Lower = stricter**, fewer but cleaner frames; higher lets more cornering contamination back in. Raise it only if you're getting too few captured frames to be useful. With the G Bar on, this is the width of the centre segment — turn it and watch the segment resize. |
| **Gate Dwl 0.1s** | `5` (0.5 s) | 0–50 (0–5 s) | How long lateral g must stay **inside** the zone before capture begins. Before #20 capture flipped on the instant g dropped below threshold, which let the tail of a corner — still unwinding, load still shifting — count as straight-line data. **`0` reproduces that old instant behaviour**, which makes it easy to A/B on the car. **Higher = cleaner frames, fewer of them.** |
| **Dwell 0.1s** | `25` (2.5 s) | 5–100 (0.5–10 s) | How long the over/under condition must persist **on already-captured frames** before the verdict latches. Stops the verdict flickering on brief noise. **Higher = steadier but slower to react.** Unrelated to *Gate Dwl* above. |
| **Orientation** | `Auto` | Auto / X / Y / Z | Which accelerometer axis is the car's lateral axis. `Auto` picks a horizontal axis from the sensed gravity direction, preferring Y (correct for a level dash mount). Force X/Y/Z only if Auto guesses wrong on an unusual mounting angle. **Test it with the G Bar: a hard left and a hard right must push the dot in opposite directions.** |

### The G Bar

A 4 px bar in the gutter between the tire rows, with a white dot riding on it:

```
x 10 ......... 113 | 114 ..... 165 | 166 ......... 269
  RED (40%)        |  centre (20%) |   RED (40%)
                        ( o )  <- current lateral g
```

- **Red flanks** are outside the capture zone; the **centre segment** is inside it. The
  segment is drawn from *Lateral cg*, so it grows and shrinks live as you turn that setting.
  At the default 35 it spans 20 % of the bar. Full-scale deflection is ±1.75 g — derived
  from that 20 % rule, not chosen independently.
- **Centre green** = capturing right now. **Centre yellow** = not capturing, either because
  you're outside the zone or because you're inside it but still serving *Gate Dwl*.
- The dot tracks the same smoothed lateral-g value the gate itself compares against the
  threshold, so **the dot crossing the segment edge is the gate decision**, not a
  visualisation of it. Under heavy cornering the dot pegs at the end of the bar; that's
  intended.

**Calibration check.** The IMU learns its at-rest gravity vector once, at boot, and the car
must be stationary and roughly level for that to be right. With the car stopped, the dot
should sit dead centre. If it doesn't, the unit booted while rolling or on a slope — reboot
at a standstill. There is no in-menu recalibrate; a reboot is the recovery.

**Bench test: tilt it 90°.** Rolling the whole device onto its side swings gravity fully
onto the lateral axis, which is *exactly* 1.0 g of apparent lateral acceleration — a known,
repeatable input, which makes it the best bench check available.

> At ±1.75 g full scale, 1.0 g is `1.0 / 1.75` = **57 % of the way out**, or roughly
> **46 % into the red**. It does **not** peg, and it shouldn't: pegging would prove nothing,
> since every value at or above full scale looks identical. A dot that lands near 57 %
> confirms the scale factor, the bias subtraction and the lateral-axis pick are all correct.
> Tilting the other way must move it the same distance the other side.

A dot that barely moves on a 90° tilt means the IMU isn't being read or `Orientation` picked
the wrong axis. A dot that pegs means the scale is wrong.

**Noise check.** On a steady straight, watch how much the dot wanders. That wander is your
mount noise, and it sets a sensible floor for *Lateral cg* — a threshold below the noise
band will chatter the gate on and off while going straight.

### The per-tire dwell bars

`Show G Bar` also paints a thin centre-origin bar inside each tire tile, showing how much
evidence that corner has built toward an inflation verdict. Each one hugs the edge facing
the g bar — **bottom** of the front tiles, **top** of the rear ones — so all five read as a
single instrument clustered around the centre gutter rather than four strays:

```
|<------------ tile width ------------>|
[        |####|      :      |    |     ]
         ^ latch    centre   ^ latch
   cyan <- UNDER            OVER -> yellow
```

Each corner carries a **signed evidence score** in milliseconds. On captured (straight-line)
frames it grows toward OVER while that tire reads centre-hot and toward UNDER while it reads
edge-hot; a neutral frame leaks it back toward zero at 0.75× rate. Mid-corner the gate stops
feeding it, so the bar **freezes** rather than drifting. Cross a latch tick and the verdict
trips, colouring the segment delta bars above.

Two deliberate asymmetries: an *opposite* reading subtracts at full rate (it is real evidence
the other way) while *neutral* only leaks at 0.75×, and the score saturates at 2× the latch
point. That saturation is the hysteresis — a fully-committed corner needs roughly 1.3× the
dwell of neutral driving before it falls back out, so a verdict can't chatter on and off at
the boundary.

Use them to tune `Dwell 0.1s`: if bars slam to a latch on one straight, the dwell is too
short; if they never get there over a whole lap, it's too long or `Inflation Delta %` is set
too tight.

---

## Tire Profiles

A profile bundles all the tire-specific calibration, so changing tires swaps everything at
once instead of re-tuning field by field. Three slots, seeded `ECF` / `EC02` / `Custom`.

**All three profiles are live in memory at all times.** Changing `Profile` only changes
which slot the fields below display and edit — it never discards or commits anything. Edit
several profiles in one visit and a single `Save Config` persists them all.

| Setting | Default | Range | What it's for |
|---|---|---|---|
| **Profile** | mode's default | slot 0–2 | Which profile is active *and* being edited. **Not persisted** — on boot it's the current mode's *Default Profile*. A manual pick holds until the next mode change or reboot. |
| **Name** | `ECF` / `EC02` / `Custom` | 7 chars | Slot name. Also relabels the choices in each mode's *Default Profile* picker. |
| **Min** | ECF 120 · EC02 110 · Custom 100 | 0–255 | Bottom of the operating window — below this the tire is cold / not up to temperature. **Carcass-frame °F.** |
| **Ideal** | ECF 160 · EC02 140 · Custom 160 | 0–255 | Target operating temperature. |
| **Max** | ECF 200 · EC02 170 · Custom 180 | 0–255 | Overheat threshold — above this you're cooking the tire. |
| **Carcass Offset °** | `20` | 0–80 °F | The surface→carcass correction. The camera reads the tire *surface*, which runs 10–40 °F cooler than the carcass a needle probe reads, and the window above is authored in carcass terms. Adding this offset makes the displayed number comparable to published tire specs. **+20 is a literature midpoint, not a measurement** — anchor it per tire by probing a tire centre with a needle pyrometer within seconds of pit-in; the camera-vs-probe gap at that instant is your value. Expect it to drift with tire, wear, speed and ambient. Only applies in `Calculated` display mode. |
| **Carcass Lag s** | `15` | 1–60 s | Smoothing time constant. The carcass isn't just hotter than the surface, it's **slower** — this low-pass makes the displayed value rise and fall like bulk rubber instead of skin, killing brief surface spikes that would otherwise flash as overheating. It's a *better* overheat guard, not a softer one: genuine overheating is sustained and survives the filter. **15 s was fitted from logged data** (knocks corner-frequency ripple to ~11 % while lagging a 60–120 s warm-up by only ~15 s). Useful range 10–20: below 10 leaves ripple, above 20 adds warm-up lag for little gain. Only applies in `Calculated` mode. |
| **Reset** | — | action | Reverts **only the selected slot** to its seeded defaults. Still needs `Save Config` to persist. |

### Offsets

`Tire Profiles → Offsets → <corner> → Left` / `Right`. Per-corner, per-side camera crop, in
thermal-frame pixel columns.

| Setting | Default | Range | What it's for |
|---|---|---|---|
| **Set Offsets** | — | action | **The way these are meant to be set.** Hands the whole screen to the four live camera images with their crop guides drawn on, and walks all eight values in one pass — see [Setting offsets interactively](#setting-offsets-interactively) below. Edits land in the selected profile in RAM; `Save Config` still persists. |
| **Left** / **Right** | `0` | 0–16 columns | How many pixel columns to crop off each side of that corner's camera frame, so the three temperature bands land on actual tread rather than sidewall, wheel or background. **This is where a mis-aimed camera gets corrected** — fixing aim here is right because it also fixes the displayed band temperatures, whereas a temperature fudge would only shift the derived delta. Kept for fine adjustment and for reading back what **Set Offsets** left behind; tuning by hand here needs **Camera Settings → Show Offsets** on so you can see the guide lines against the live image. They're per-profile because different tires and mountings want different crops; all three profiles seed identical. |

#### Setting offsets interactively

These eight numbers are a **pixel aim against a camera image**, and the numeric fields above
hide the very image you're aiming. `Set Offsets` fixes that: the menu closes, the four camera
images fill the screen with no tire map, and every crop guide is drawn on top (regardless of
the *Show Offsets* toggle, and always over the **uncropped** frame — a guide drawn on an
already-cropped image tells you nothing).

Exactly one guide is armed at a time, and it **blinks**. That's the only indicator, so the
gestures are worth knowing:

| Swipe | What it does |
|---|---|
| left / right | Slides the **armed** guide the way you swiped. The stored value moves opposite ways on the two guides — both are measured inward from their own edge, so a right offset grows leftward — but on screen the line always follows your finger. Stops at 0 (the image edge) and 16 (mid-image). |
| down | Next value. Order is fixed: FL, FR, RL, RR, and Left before Right within a corner. **Corners with no camera are skipped** — there's nothing to aim there. |
| up | Previous value. |
| down past the last value | **Green border pulses** — you're about to keep. Down again exits to this menu with the edits; up returns to the last value. |
| up past the first value | **Red border pulses** — you're about to discard. Up again exits and restores every offset to what it was on entry; down returns to the first value. |

Keeping means keeping in the **selected profile, in RAM** — exactly like typing into the
numeric fields. Nothing reaches EEPROM until root **Save Config**, so a session of aiming is
still discardable by simply not saving.

---

## Hardware Settings

| Setting | Default | Range | What it's for |
|---|---|---|---|
| **Temp Sensor Indices** → Front Left / Front Right / Rear Left / Rear Right | `0` | 0–7 | ⚠️ **Currently does nothing.** These are editable and persisted (EEPROM 20/22/24/26) but **nothing in the firmware reads them** — there is no getter. They were presumably intended to remap which physical I²C sensor feeds which corner. Changing them has no effect on the display or any reading. |

---

## Removed settings

Recorded so an old note or screenshot doesn't send you hunting for something that's gone.

| Setting | Was in | Removed | Why |
|---|---|---|---|
| **Base FL / FR / RL / RR** | Tire Profiles | #18 | Per-corner inflation baselines. The shipped values encoded one track's load pattern rather than a property of the car, the artifact they appeared to correct is already removed by the straight-line gate, and any genuinely static component is a camera aim error belonging in the crop offsets above. Full reasoning in the design doc's Amendments section. |
| **Load** | Tire Profiles | #18 | Manually pulled the selected slot into the edit buffer. The edit buffer is gone (#19) — the fields now point straight at the live slot, so there's nothing to load. |
| **Save Prof** | Tire Profiles | #18 | Saved only the current profile. `Save Config` already writes all three, and a profile-only save became actively misleading once several profiles can hold pending edits at once. |
| **Street/Track Min · Ideal · Max** | Street/Track Settings | #14 | Modes no longer carry their own window; they name a *Default Profile* and the profile supplies the window. |
| **Inflation** | Track Settings | #21 | Gated the over/under verdict and its top-left badge. The badge is gone — it reported one global verdict, so it could say something was wrong but never which corner, and the per-tire delta bars now carry that verdict on the tire it belongs to. With the latch driving those bars, an off switch left them showing an alignment verdict but no inflation one, which reads as "this tire is fine" rather than "this check is disabled". *Segment Deltas* is the honest visibility control. EEPROM 31 is free again. |
| **Camera offset values** | Camera Settings | #15 | Moved onto the tire profile (Tire Profiles → Offsets) so they swap with the tire. The display toggles stayed global. |
