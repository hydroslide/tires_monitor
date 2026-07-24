# Build & flash — the daily loop

How to build the firmware, get it onto the board, and watch it run. Platform-neutral;
first-time machine setup lives in [DEV-SETUP-MACOS.md](DEV-SETUP-MACOS.md) and
[DEV-SETUP-WINDOWS.md](DEV-SETUP-WINDOWS.md).

---

## The three ways to drive it

Same actions, pick whichever suits you.

### 1. Status-bar buttons (fewest keystrokes)

With the `actboy168.tasks` extension installed (it's in the recommended list), four
buttons appear in the **bottom-left status bar**:

```
 🔧 Build    ⚡ Flash    ⚡︎ Monitor    🔌 Doctor
```

Click `⚡ Flash` and you're done — it compiles, uploads, and prints a timing summary.

> VS Code has no built-in task buttons; that extension is what provides them. Everything
> still works without it, just from the command palette instead.

### 2. Keyboard / command palette

| Action | How |
|---|---|
| Build | `Cmd+Shift+B` (`Ctrl+Shift+B` on Windows) — it's the default build task |
| Anything else | `Cmd+Shift+P` → **Tasks: Run Task** → pick from the list |

### 3. Terminal

```bash
./scripts/tm.sh build      # compile
./scripts/tm.sh flash      # compile + upload  <- the usual one
./scripts/tm.sh monitor    # watch serial output
./scripts/tm.sh doctor     # why won't it connect?
```

Windows: `.\scripts\tm.ps1 <same commands>`.

Full command list: `./scripts/tm.sh --help`.

---

## How long does it take?

Every build and upload prints its own elapsed time, so you never have to guess. `flash`
prints a breakdown at the end:

```
==> Build took 33s
==> Upload took 21s

=== flash summary ===
  build   33s
  upload  21s
  total   56s
```

**Measured on this project** (2019 Intel Core i9 MacBook Pro, macOS 15.7.5, ESP32 core
3.0.7). Yours will differ, especially on Apple Silicon, which is typically much faster:

| Operation | Time | Notes |
|---|---|---|
| First-ever setup | **10–20 min** | One-time. Downloads ~5 GB of core + toolchain. |
| `build` — nothing changed | **~32 s** | arduino-cli still re-links and re-checks every library. |
| `build` — one `.cpp` edited | **~35 s** | Realistically your normal edit-build cycle. |
| `build --clean` | **~2 m 41 s** | Full rebuild, cache discarded. Rarely needed. |
| `upload` | **~21 s** | ~9 s of it is the actual 990 KB flash write at ~855 kbit/s. |
| **`flash` (build + upload)** | **~56 s** | The realistic edit → running-on-hardware turnaround. |

So: **expect about a minute from code change to running firmware.**

### Why an unchanged build still takes ~30s

arduino-cli re-runs library detection and re-links every time; it doesn't have a
no-op fast path like `make`. The per-file compile *is* cached — that's why a clean build
is 5× slower than an incremental one. If you only changed one `.cpp`, the ~35 s is mostly
fixed overhead, not your file.

Don't use `--clean` habitually. Reach for it only when you change `sketch.yaml` (library
or core versions) or hit an inexplicable link error.

---

## Typical session

```bash
# edit code in VS Code ...
./scripts/tm.sh flash        # ~1 min, ends with the timing summary
./scripts/tm.sh monitor      # watch it boot; Ctrl-] to quit
```

A healthy boot looks like:

```
Top of ESP32 Tires Setup
WiFi AP started
WiFi server started
WiFi initialized.
CST816Touch initialized.
7 items loaded from EEPROM
EEPROM values loaded
Bottom of ESP32 Tires Setup
```

`CST816Touch initialized.` is the meaningful one — it only appears if the I2C and touch
pins in `pin_config.h` are right, so it doubles as a hardware sanity check.

With no sensors attached you'll then see `I2C error: address send, NACK received`
repeating forever. **That's expected** — it's the firmware failing to reach the TCA9548A
mux at `0x70`. Tires read 0. Use the `esp32_i2c_scanner` sketch to debug the sensor bus
once hardware is wired up.

---

## Rules that will save you time

**Close the monitor before you upload.** The serial port is exclusive. An upload that
fails immediately after a successful one is almost always this. (`tm.sh flash` doesn't
open a monitor, so it's only an issue if you left one running.)

**The monitor only shows what's printed after it attaches.** USB CDC has no backlog, so
attaching late means the `setup()` banner is already gone. Tap **RST** with the monitor
open to see the boot from the top.

**Don't raise `UploadSpeed`.** It's pinned to 115200 in `sketch.yaml`. At 921600 esptool
dies with `No serial data received` while renegotiating baud — USB-Serial-JTAG isn't a
real UART. Raising it buys nothing; the flash write is USB-speed-bound either way.

---

## Changing the build configuration

Everything lives in [`tires_esp32/sketch.yaml`](../tires_esp32/sketch.yaml) — core
version, all seven library versions, and every board option. It is the single source of
truth, and it's committed, so any machine that clones this repo builds the identical
binary.

Two profiles:

| Profile | What |
|---|---|
| `waveshare169` | **Default.** ESP32 core 3.0.7 + pinned libraries. Verified working on hardware. |
| `waveshare169-latest` | Opt-in: core 3.3.11 + current libraries. Unverified — expect to fix API drift. |

Try the newer stack without committing to it:

```bash
./scripts/tm.sh build --profile waveshare169-latest
```

After changing library or core versions, do a `--clean` build.

---

## Flash budget

```
Sketch uses      989,649 bytes  (31%)  of 3,145,728  program storage
Global variables  48,872 bytes  (14%)  of   327,680  dynamic memory
```

The 3 MB app partition comes from `PartitionScheme=app3M_fat9M_16MB`. Plenty of room —
the firmware could roughly triple before partitioning becomes a concern.

---

## When something breaks

Run `./scripts/tm.sh doctor` first. It checks, in order: toolchain present → board
enumerated on USB at all → serial devices → arduino-cli detection → resolved port. That
sequence tells you immediately whether you have a cable problem or a software problem.

| Symptom | Cause / fix |
|---|---|
| `No board detected` | **Cable first** — many USB-C cables are charge-only. Then try another port. `doctor` tells you if the OS sees the device at all. |
| `No serial data received` during baud change | `UploadSpeed` was raised above 115200. Put it back. |
| `Unable to verify flash chip connection` | Leftover state from an aborted upload. `tm.sh upload` auto-retries once after a hard reset; if it still fails, hold **BOOT**, tap **RST**, release **BOOT**, retry. |
| Upload fails right after a good one | A serial monitor is holding the port. Close it. |
| Monitor silent | Attached after boot. Tap **RST**. |
| Red squiggles in the editor | `./scripts/tm.sh intellisense`, then reload the VS Code window. |
| Board port name changed | Normal — it varies per physical USB port. Detection is by USB VID/PID, so it just works. Override with `TM_PORT=/dev/cu.usbmodemXXXX` if needed. |

Deeper per-platform troubleshooting is in the two setup guides.
