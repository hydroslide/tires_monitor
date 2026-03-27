# Display Abstraction & Buffered Rendering

All changes below are relative to commit `2fa5db3` (Merge pull request #1 from hydroslide/mlx90640).

---

## Problem

The Adafruit ST7789 display library draws directly to the screen on every function call. When a single frame requires many draw calls (fill rectangles, print text, draw outlines, thermal overlays), each call immediately pushes pixels over SPI. This causes:

1. **Flickering** - partial frames are visible as individual draw operations complete
2. **Low framerates** - each SPI transaction has overhead; many small transactions are slower than one large one

## Solution

A display abstraction layer with two interchangeable implementations:

- **StandardDisplay** - draws directly to screen (original behavior, zero risk)
- **BufferedDisplay** - accumulates all drawing into a 280x240 RAM buffer (`GFXcanvas16`), then flushes the entire screen in a single SPI transaction via `drawScreen()`

The implementation is selectable at runtime via the menu's **Test** toggle.

---

## Architecture

```
Print (Arduino built-in)
  └── DisplayBase (abstract base class, virtual methods with no-op defaults)
        ├── DisplayProxy (forwards all calls to a switchable DisplayBase*)
        ├── StandardDisplay (wraps Adafruit_ST7789, draws directly to screen)
        └── BufferedDisplay (draws to GFXcanvas16, flushes via fast SPI)
```

### Why DisplayProxy?

Multiple objects (MenuRenderer, ThermalDisplay, QuadrantFactory) hold `DisplayBase&` references captured at construction time. A proxy object is the permanent global — its identity never changes, but its internal target can be swapped at runtime via `setImplementation()`. This avoids recreating dependent objects when switching display modes.

### Why GFXcanvas16?

`GFXcanvas16` inherits from `Adafruit_GFX`, which means all standard drawing primitives (fillRect, print, setCursor, etc.) work on it out of the box — they write to a RAM buffer instead of the screen. We then flush that buffer using the fast SPI path (`startWrite/setAddrWindow/writePixels/endWrite`), NOT `drawRGBBitmap` which is ~18x slower (pixel-by-pixel).

---

## New Files

### `DisplayBase.h`
Abstract base class. All display methods are `virtual` with no-op default implementations, except `width()` and `height()` which are pure virtual. Inherits from Arduino's `Print` class so `print()`/`println()` work. Any subclass compiles by implementing just `width()` and `height()`.

Key new methods added beyond the original Adafruit API surface:
- `clearScreen()` - clears the buffer (BufferedDisplay) or fills screen black (StandardDisplay)
- `drawScreen()` - flushes buffer to hardware (BufferedDisplay) or no-op (StandardDisplay)
- `pushPixels(x, y, w, h, *colors, len)` - blits a pre-built pixel buffer to a screen region in one operation

### `StandardDisplay.h` / `StandardDisplay.cpp`
Renamed from `DisplayWrapper`. Concrete implementation that delegates every call to `Adafruit_ST7789`. Behavior is identical to the original code.

- `clearScreen()` calls `tft.fillScreen(ST77XX_BLACK)`
- `drawScreen()` is a no-op (already drawing directly)
- `pushPixels()` wraps the 4-line SPI sequence: `startWrite/setAddrWindow/writePixels/endWrite`

### `BufferedDisplay.h` / `BufferedDisplay.cpp`
The new buffered implementation. Holds a `GFXcanvas16` (280x240, 134KB RGB565 buffer) and a reference to the physical `Adafruit_ST7789` for hardware init and SPI flushing.

- All drawing methods (fillRect, setCursor, print, etc.) delegate to the canvas
- Hardware init methods (init, setRotation, setSPISpeed) delegate to the physical TFT
- `clearScreen()` zeros the canvas buffer
- `drawScreen()` pushes the entire canvas to hardware via fast SPI
- `pushPixels()` copies pixel data into the correct position in the canvas buffer using `memcpy` per row
- `write(uint8_t)` delegates to `canvas.write()` so text rendering targets the buffer

### `DisplayProxy.h`
Header-only forwarding class. Every method is a one-liner that delegates to `_impl->method()`. Has `setImplementation(DisplayBase*)` to switch the active display at runtime.

---

## Deleted Files

- `DisplayWrapper.h` / `DisplayWrapper.cpp` - renamed to `StandardDisplay`
- `AlternateDisplay.h` - replaced by `BufferedDisplay`

---

## Modified Files

### `tires_esp32.ino`

**Global variables changed:**
- Replaced `DisplayWrapper displayImpl(tft)` / `DisplayBase& display = displayImpl` with:
  ```cpp
  StandardDisplay standardDisplay(tft);
  BufferedDisplay bufferedDisplay(tft);
  DisplayProxy displayProxy;
  DisplayBase& display = displayProxy;
  ```
- `QuadrantFactory` and `MenuRenderer` now receive `display` (the proxy) instead of raw `tft`

**`setup()` changes:**
- Added `displayProxy.setImplementation(&standardDisplay)` before hardware init
- Hardware init calls (`setSPISpeed`, `init`, `setRotation`, `fillScreen`) now go through `display` instead of raw `tft`

**`initializeSystem()` changes:**
- The menu's **Test** toggle (`getTestEnabled()`) now controls which display implementation is active:
  ```cpp
  bool useBuffered = getTestEnabled();
  displayProxy.setImplementation(useBuffered ? &bufferedDisplay : &standardDisplay);
  ```
- Removed `tempReader->autoRecoverTire = getTestEnabled()` (auto-recover now defaults to `true` in TempReader.h)

**`doRunningMode()` changes:**
- Added `display.drawScreen()` at the end of the sensor read block, after `updateThermalDisplays()`. For StandardDisplay this is a no-op. For BufferedDisplay this flushes the canvas to the screen.

### `ThermalDisplay.h` / `ThermalDisplay.cpp`

- Member renamed from `Adafruit_ST7789 &tft` to `DisplayBase &display` for consistency
- Constructor parameter type changed from `Adafruit_ST7789&` to `DisplayBase&`
- All `tft.` calls renamed to `display.`
- The 4-line SPI blit sequence (`startWrite/setAddrWindow/writePixels/endWrite`) replaced with single `display.pushPixels(areaX, areaY, areaW, areaH, framebuf, areaW * areaH)` call in both `updateDisplay()` overloads

### `MenuRenderer.h` / `MenuRenderer.cpp`

- Include changed from `<Adafruit_GFX.h>` + `<Adafruit_ST7789.h>` to `"DisplayBase.h"`
- Constructor parameter and member type changed from `Adafruit_ST7789&` to `DisplayBase&`

### `QuadrantFactory.h` / `QuadrantFactory.cpp`

- Include changed from `<Adafruit_ST7789.h>` to `"DisplayBase.h"`
- Constructor parameter and member type changed from `Adafruit_ST7789&` to `DisplayBase&`

### `Tire.h` / `Tire.cpp`

- Header: replaced Adafruit includes with forward declaration `class DisplayBase;`
- Cpp: replaced `extern Adafruit_ST7789 tft;` with `extern DisplayBase& display;`
- All `tft.` calls changed to `display.` (fillRoundRect, drawRoundRect, setFont, setTextSize, getTextBounds, setCursor, setTextColor, println)

### `Wheels.h` / `Wheels.cpp`

- Header: replaced Adafruit includes with `"DisplayBase.h"`
- Cpp: replaced `extern Adafruit_ST7789 tft;` with `extern DisplayBase& display;`
- `tft.width()` / `tft.height()` changed to `display.width()` / `display.height()`

### `ThreeSectionTire.h` / `ThreeSectionTire.cpp`

- Header: replaced `<Adafruit_GFX.h>` with `"DisplayBase.h"`
- Cpp: replaced `extern Adafruit_ST7789 tft;` with `extern DisplayBase& display;`
- All `tft.` calls changed to `display.` (~15 call sites: fillRect, fillRoundRect, setFont, setTextSize, setTextColor, getTextBounds, setCursor, println)

### `TempReader.h`

- `bool autoRecoverTire;` changed to `bool autoRecoverTire = true;` — this feature is now always enabled by default rather than being controlled by the Test toggle

---

## How to Use

### Standard mode (default, original behavior)
Menu: set **Test** to `false` (or leave it off). All drawing goes directly to the screen exactly as before.

### Buffered mode (flicker-free)
Menu: set **Test** to `true`. All drawing accumulates in a RAM buffer and flushes to the screen once per loop iteration.

The switch takes effect immediately when exiting the menu (no reboot required).

### Memory impact
BufferedDisplay allocates a 280x240x2 = 134,400 byte (131 KB) buffer via `GFXcanvas16`. ESP32 with PSRAM typically auto-redirects large `malloc` calls to PSRAM. If internal RAM is tight, this is the primary cost.

---

## Future Work

- `clearScreen()` is implemented but not called anywhere yet — available for experimentation
- The `DisplayBase` abstract class makes it straightforward to add additional display implementations (e.g., partial-buffer, dirty-rect tracking)
- Consider adding `drawScreen()` to the menu rendering path if BufferedDisplay is used during menu interaction
