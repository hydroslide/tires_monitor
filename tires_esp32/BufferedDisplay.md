# Display abstraction & buffered rendering

> Rewritten on `dev` in #28. `main` still carries the March version, which described a
> refactor that did not happen (it names `DisplayWrapper.h` and `AlternateDisplay.h` as
> renamed/replaced files — neither has ever existed in this repo's history) and omits
> several changes the commit actually made. Trust this copy.

## The problem

`Adafruit_ST7789` draws straight to the panel on every call. A frame here is many calls —
band fills, temperature text, delta bars, dwell bars, four camera blits, the g bar, the
session badge — and each one pushes pixels over SPI as it happens. You see the frame being
assembled: partial repaints, tearing, and visible flicker wherever two components overlap.

## The shape

Three classes and one interface, in `DisplayBase.h`:

| | |
|---|---|
| `DisplayBase` | The interface every rendering component draws through. Inherits `Print`, so `print`/`println` work. |
| `StandardDisplay` | Straight to the glass. The original behaviour, kept as an escape hatch. |
| `BufferedDisplay` | Composes into an off-screen `GFXcanvas16`, pushed by `drawScreen()`. |
| `DisplayProxy` | Forwards to whichever of the two is selected. It is what `display` actually is. |

The sketch holds one global reference, `DisplayBase& display`, bound to the proxy.
**Nothing outside these files may hold an `Adafruit_ST7789&`** — on the buffered path such a
reference paints straight to the glass and is wiped by the next flush, which presents as a
region that flickers at 10 Hz or never appears, with no compile error. Two greps enforce it:

```sh
grep -rn "Adafruit_ST7789" tires_esp32/*.{h,cpp,ino}
  # DisplayBase.h, BufferedDisplay.*, StandardDisplay.*, and the sketch's include + the
  # one global definition. Nothing else.
grep -rn "\btft\." tires_esp32/*.{h,cpp,ino}
  # BufferedDisplay.cpp and StandardDisplay.cpp only.
```

### Every method is pure virtual, on purpose

`DisplayBase`'s methods were no-op default bodies (`{}`) until #28. That combination — an
interface a component must draw through, and defaults that silently succeed — means a
forgotten override **compiles clean and draws nothing**. Five methods were already missing
when the port started (`drawRect`, `drawFastHLine`, `fillCircle`, `setTextWrap`, `cp437`),
one of which, `drawRect`, is the only draw in the entire offset-setup border.

With `= 0` the compiler enforces completeness. If you add a method, all three
implementations must implement it or the build fails. Keep it that way.

## The canvas is retained

**The canvas is never cleared between frames.** This is load-bearing, not an oversight.

The tire map is composed on the 1 Hz update cadence while the flush runs at 10 Hz. Clearing
each frame would blank the map on 9 of every 10 flushes. `clearScreen()` exists but nothing
calls it.

The useful consequence: **the canvas is a faithful shadow of the glass.** Every
skip-if-unchanged gate and every erase-by-repainting-the-background trick in the rendering
code behaves exactly as it did when drawing direct, because a retained canvas and a physical
panel have the same semantics — you draw over what is already there. That is why this port
did not have to touch a single redraw gate.

You can see it on the device: the tire colour bars persist in the margins around the camera
quadrants even though they are only drawn on a threshold crossing.

## Flush points

One per thing that can own the screen. Miss one and that screen composes into the canvas and
never reaches the glass.

| | Site | Cadence | Owner |
|---|---|---|---|
| F1 | `MenuRenderer::render()` | event (gesture) | The menu |
| F2 | end of `setup()` | boot, once | The boot paint |
| F3 | end of `doOffsetSetupMode()` | 10 Hz | Offset setup (#23) |
| F4 | the `summaryAutoDirty` branch | edge | Auto session summary (#2) |
| F5 | end of `doRunningMode()`'s read block | 10 Hz | The running display |

March had F5 and F1 only, because the tree then had two screen owners. F3 and F4 cover modes
written afterwards.

**F1 is split.** `MenuRenderer::render()` has three early returns — the summary, balance and
name-entry sub-screens — each owning the whole screen. A flush at the end of the compose path
never runs for any of them. Composition is `renderFrame()`; `render()` is
`renderFrame()` + `drawScreen()`.

**F5 owns the overlays.** `drawSessionFeedback()` and `drawImuGateBar()` are called from
inside `doRunningMode()` immediately before the flush, *not* from `loop()`. Called from
`loop()` they ran after the flush, so once a second — when the 1 Hz block repaints the tire
map and wipes the badge footprint and the g bar's gutter — the flush pushed a frame with both
missing, and they were re-asserted only after the glass had already updated. That is a
~100 ms dropout of the g bar and session badge, every second.

No flush exceeds 10 Hz, so SPI load is no higher than the direct path's worst case.

## Memory: why `begin()` exists

The canvas is 280 × 240 × 2 = **134,400 bytes**, allocated in `BufferedDisplay::begin()` from
`initializeSystem()` — **never at static init**.

`CONFIG_SPIRAM_BOOT_INIT` is not set on the pinned core, so `psramAddToHeap()` runs inside
`initArduino()`, which `app_main()` calls *after* C++ global constructors. A by-value
`GFXcanvas16` member therefore allocates before PSRAM exists as far as the heap is concerned,
and 131 KB lands in internal DRAM — before the SoftAP starts, and even when the buffered path
is switched off. The symptom is WiFi failing, not the display.

Deferred to `setup()`, `CONFIG_SPIRAM_USE_MALLOC=y` with
`CONFIG_SPIRAM_MALLOC_ALWAYSINTERNAL=4096` sends it to PSRAM. `begin()` logs which it got:

```
BufferedDisplay: 134400 byte canvas in PSRAM (internal free: ...)
```

**If that ever says `INTERNAL DRAM`, stop and fix it.** `begin()` returns false on failure and
`initializeSystem()` falls back to the direct path with a message, rather than running a
display that silently draws nothing.

Static RAM is 50,080 bytes, +40 over the pre-port baseline — the abstraction's vtables and
references, and proof the canvas is not in there.

`ThermalDisplay`'s 24 KB quadrant framebuffer has the same static-init problem and does not
get the same treatment: its `heap_caps_malloc(MALLOC_CAP_SPIRAM)` runs before PSRAM joins the
heap, returns NULL, and falls through to internal `malloc()`. It works — it just uses scarce
RAM instead of plentiful RAM. Tracked separately; fixing it means moving the four
`ThermalDisplay` globals out of static init.

## Blitting: `pushPixels()`, not the SPI quartet

`ThermalDisplay` pushes each camera quadrant with
`display.pushPixels(x, y, w, h, framebuf, len)`. It replaced a raw
`startWrite`/`setAddrWindow`/`writePixels`/`endWrite` quartet, which on the buffered path
would go to the panel and be wiped by the next flush.

`DisplayBase` still exposes that quartet as an escape hatch, but **nothing calls it** and
`pushPixels()` is the one that works on both paths.

`BufferedDisplay::pushPixels()` clips on all four sides and honours `len`. The March version
did an unclipped `memcpy` that trusted its arguments; no caller is out of bounds today, but
the failure mode was heap corruption past the buffer.

## Text state is per-implementation

Each implementation keeps its own GFX text state — `BufferedDisplay` forwards every text call
to the canvas, `StandardDisplay` to the panel. Both are internally consistent, but **state set
while one implementation is active does not carry to the other.**

This bites at boot: `setup()` calls `display.cp437(true)` while the proxy still points at
`StandardDisplay` and the canvas does not yet exist. So `initializeSystem()` re-asserts it
after choosing the implementation. Without that the degree ring (0xF8) renders as the stray
dot at 0xF9 — on the buffered path only.

Anything else made sticky at boot needs the same treatment.

## Selecting the path (staged — see #28)

The `Test` menu item currently chooses, at runtime, applied on menu close:

```cpp
bool useBuffered = getTestEnabled();
if (useBuffered && !bufferedDisplay.begin()) useBuffered = false;   // reports and falls back
displayProxy.setImplementation(useBuffered ? &bufferedDisplay : &standardDisplay);
```

This is **temporary**, so both paths can be compared on the car by eye. It costs `Test` its
documented meaning: `enableThermalTemps` is pinned `true` meanwhile, so the camera-image views
are always on and the swipe-cycled display has 3 states rather than 4.

**Phase 2** decouples the two, restores `enableThermalTemps = getTestEnabled()`, and makes
buffered the default. `DisplayProxy` and `StandardDisplay` stay — the direct path remains a
working escape hatch, just not the default one.

## Cost

A full flush is 134,400 bytes at 40 MHz ≈ **26.9 ms**, at 10 Hz.

For scale, the device already blocks ~19.7 ms per 100 ms pushing four camera quadrants in
thermal mode, so the real added cost is about +7 ms there and +27 ms in `thermalMode 0`. If
touch starts feeling unresponsive, the fix is a dirty flag that skips `drawScreen()` when
nothing was drawn that pass.
