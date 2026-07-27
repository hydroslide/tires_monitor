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
| Menu settings | `SETTINGS_MAGIC`, currently `0xA7` | a setting moves address or changes meaning |
| Tire profiles | `PROFILE_MAGIC`, currently `0x5D` | the `TireProfile` struct layout changes |

**The active tire profile is deliberately not saved.** On boot it's set to the current
mode's *Default Profile*. See [Tire Profiles](#tire-profiles).

---

## Root menu

| Setting | Default | Range | What it's for |
|---|---|---|---|
| **Current Mode** | `Street` | Street / Track | Master mode switch. Selects which *Default Profile* is adopted, and enables the Track-only features (inflation indicator, session recording, balance readout). Changing it immediately re-snaps the active tire profile. |
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
| **Inflation** | Track | `On` | On / Off | Shows the latched over/under-inflation badge. Off suppresses it entirely. |

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
| **Segment Deltas** | `Off` | On / Off | Paints per-band verdict colour bars under each tire, showing which band tripped the inflation or alignment check. A diagnostic overlay — the underlying colours are always computed and always logged over NBP, this only controls whether they're painted on screen. |
| **Inflation Delta %** | `10` | 0–16 % | How far **edge-vs-centre** must diverge before a tire is called over/under-inflated. The comparison is `avg(outer, inner)` vs `centre`, and the threshold is this percent *of the edge temperature* (so ~16 °F at a 160 °F edge). **Lower = more sensitive**, more false calls; higher = only flags gross deviations. |
| **Alignment Delta %** | `15` | 0–16 % | How far **outer-vs-inner shoulder** must diverge before it's flagged as an alignment (camber) issue rather than a pressure one. Same percent-of-edge basis. **Only evaluated if the inflation check didn't already trip** — inflation takes priority. Note the default of 15 sits near the 16 ceiling, so it's currently a fairly insensitive check. |

---

## IMU Gate

The onboard 6-axis IMU (QMI8658C) suppresses tire judgements while cornering. This exists
because the mid-corner reading is a body-roll artifact — the camera crop bleeds onto the
sidewall at roll angle, manufacturing a fake centre-hot that scales with lateral g and is
*opposite* to real load physics. Gating to straight-line frames removes roughly 24 of the
27 °F of that artifact. Full analysis in the design doc §5.4.

| Setting | Default | Range | What it's for |
|---|---|---|---|
| **Gate Enable** | `On` | On / Off | Master switch for straight-line gating. Off means inflation reads accumulate everywhere, including mid-corner — which is what produced the original backwards verdict. Leave on. |
| **Lateral cg** | `35` (0.35 g) | 10–100 (0.10–1.00 g) | Lateral-g threshold below which the car counts as "straight and steady" and readings are accepted. **Lower = stricter**, fewer but cleaner frames; higher lets more cornering contamination back in. Raise it only if you're getting too few captured frames to be useful. |
| **Dwell 0.1s** | `25` (2.5 s) | 5–100 (0.5–10 s) | How long the over/under condition must persist before the alert latches. Stops the badge flickering on brief noise. **Higher = steadier but slower to react.** |
| **Orientation** | `Auto` | Auto / X / Y / Z | Which accelerometer axis is the car's lateral axis. `Auto` picks a horizontal axis from the sensed gravity direction, preferring Y (correct for a level dash mount). Force X/Y/Z only if Auto guesses wrong on an unusual mounting angle. |

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
| **Left** / **Right** | `0` | 0–16 columns | How many pixel columns to crop off each side of that corner's camera frame, so the three temperature bands land on actual tread rather than sidewall, wheel or background. **This is where a mis-aimed camera gets corrected** — fixing aim here is right because it also fixes the displayed band temperatures, whereas a temperature fudge would only shift the derived delta. Tune with **Camera Settings → Show Offsets** on so you can see the guide lines against the live image. They're per-profile because different tires and mountings want different crops; all three profiles seed identical. |

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
| **Camera offset values** | Camera Settings | #15 | Moved onto the tire profile (Tire Profiles → Offsets) so they swap with the tire. The display toggles stayed global. |
