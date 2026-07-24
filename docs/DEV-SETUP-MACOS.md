# Dev environment setup — macOS

Sets up building and flashing the `tires_esp32` firmware from VS Code on a Mac.
Start to finish: ~15 minutes, most of it one large download.

You do **not** need the Arduino IDE. Everything runs through `arduino-cli`, which is
the same toolchain the Arduino IDE uses internally.

> **Handing this to Claude Code?** Every step below is a literal command. Say:
> *"Follow docs/DEV-SETUP-MACOS.md and get the firmware building."*

---

## 1. Install Homebrew (skip if you have it)

Check:

```bash
brew --version
```

If that fails:

```bash
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
```

## 2. Install arduino-cli

```bash
brew install arduino-cli
arduino-cli version      # expect 1.5.x or newer
```

## 3. Install VS Code extensions

```bash
code --install-extension ms-vscode.cpptools
code --install-extension ms-vscode.vscode-serial-monitor
```

Or open the repo in VS Code and accept the "recommended extensions" prompt — they're
listed in `.vscode/extensions.json`.

> Do **not** install the Arduino extension. Microsoft archived it in October 2024, and
> its community fork insists on managing its own `.vscode/arduino.json`, which fights
> the `sketch.yaml` build profile this project uses.

## 4. First build

From the repo root:

```bash
./scripts/tm.sh setup
```

This downloads the pinned ESP32 core (`esp32:esp32` 3.0.7) and all seven libraries, then
compiles. **The first run downloads roughly 1 GB and can take 5–15 minutes** — the
Xtensa cross-compiler toolchain is large. Later builds take seconds.

You should end with a `Sketch uses ... bytes` summary and no errors.

Nothing here is global: `sketch.yaml` pins every version, and arduino-cli excludes
globally-installed cores and libraries from a profile build. Another project on this Mac
cannot break this one.

## 5. Generate IntelliSense data

```bash
./scripts/tm.sh intellisense
```

Then reload VS Code (`Cmd+Shift+P` → *Developer: Reload Window*). Code navigation and
autocomplete now resolve into the real ESP32 core headers.

## 6. Connect the board

Plug the Waveshare board into the Mac with a USB-C cable, then:

```bash
./scripts/tm.sh port
```

Expect something like `/dev/cu.usbmodem2101`.

**If it prints an error, the cable is the most likely cause** — see
[Troubleshooting](#troubleshooting-connections) below. Run `./scripts/tm.sh doctor`
first; it distinguishes a cable problem from a software problem.

## 7. Flash it

```bash
./scripts/tm.sh flash      # compile + upload
./scripts/tm.sh monitor    # watch it boot
```

Expected serial output (captured from a real run of this setup):

```
Top of ESP32 Tires Setup
WiFi AP started
WiFi server started
WiFi initialized.
CST816Touch initialized.
7 items loaded from EEPROM
EEPROM values loaded
Bottom of ESP32 Tires Setup
I2C error: address send, NACK received      <- only if no sensors are attached
```

...and the round LCD shows four tire quadrants. Swipe left on the screen to open the menu.

Press `Ctrl-]` to exit the monitor.

> **`CST816Touch initialized.` is the line that matters.** It only appears if the I2C and
> touch pins in `pin_config.h` are correct, so it doubles as a wiring sanity check.

> The repeating `I2C error: address send, NACK received` is **expected with no sensors
> attached** — it's the code failing to reach the TCA9548A mux at 0x70. Tires will read 0.
> Not a setup problem. Use the `esp32_i2c_scanner` sketch to debug the sensor bus once
> the hardware is wired up.

> The monitor only shows output produced *after* it attaches, so a monitor opened late
> shows nothing until the next reset. Tap **RST** to see the banner.

---

## Daily workflow

See [BUILD-AND-FLASH.md](BUILD-AND-FLASH.md). Short version: `Cmd+Shift+B` builds;
*Run Task → Flash* uploads.

---

## What you should know about USB serial on a Mac

If your only experience is COM ports on Windows, this is the orientation.

### There is no driver to install

This board uses the ESP32-S3's **native USB** (USB-Serial-JTAG) — not a CP2102/CH340/FTDI
bridge chip. macOS enumerates it as a standard CDC serial device with drivers already in
the OS.

It appears as **VID `0x303A`, PID `0x1001`** — Espressif's hardware USB-Serial-JTAG
identity. (The board variant also declares PID `0x821E`, but that descriptor is only used
if the firmware runs USB-OTG/TinyUSB. This project builds with `USBMode=hwcdc`, so you get
`0x1001`. The scripts accept both.)

Every "install the Silicon Labs VCP driver" tutorial you'll find for ESP32 boards is for
a *different kind of board* and does not apply here. Installing one won't help and adds a
kernel extension you don't need.

### Ports are files, and there are two of every one

macOS exposes serial devices as `/dev/cu.*` and `/dev/tty.*` — the same hardware, two
device nodes:

| Node | Use |
|---|---|
| `/dev/cu.usbmodem*` | **Always use this one.** "callout" — opens immediately. |
| `/dev/tty.usbmodem*` | Blocks waiting for carrier detect. Will hang an upload. |

The scripts filter `tty.*` out and only ever return a `cu.*` device.

### Names are not stable

The suffix changes with the *physical port* you plug into, and changes again when the
board reboots into download mode. That's why the scripts resolve the board by USB VID/PID
out of `arduino-cli board list --json` instead of hardcoding a name.

Override it if you ever need to:

```bash
TM_PORT=/dev/cu.usbmodem2101 ./scripts/tm.sh flash
```

### No sudo, no group membership

Unlike Linux (`dialout`), macOS lets your user open serial devices directly. If something
suggests `sudo`, it's wrong.

### Only one program can hold the port

**The single most common failure**: an upload fails right after a successful one because
a serial monitor is still attached. Close the monitor (`Ctrl-]`, or close the VS Code
Serial Monitor pane) before uploading.

---

## Troubleshooting connections

Run this first — it separates hardware problems from software problems:

```bash
./scripts/tm.sh doctor
```

### "No board detected"

**1. Suspect the cable first.** Many USB-C cables are charge-only, with no data
conductors. This is by far the most common cause. Check whether macOS sees the device
*at all*:

```bash
system_profiler SPUSBDataType | grep -i -A8 "ESP32\|Waveshare\|USB JTAG"
```

- **Output** → the board is enumerated; it's a software issue, keep reading.
- **No output** → the cable or the port. Swap the cable for one you know carries data
  (e.g. one that syncs a phone), then try a different port on the Mac. Nothing in
  software can fix this.

**2. Force download mode.** If the board is enumerated but flashing fails, put the
ESP32-S3 into the ROM bootloader by hand:

> Hold **BOOT**, tap **RST**, then release **BOOT**.

The port re-enumerates under a different name — another reason detection is by VID/PID.
Then run `./scripts/tm.sh upload`.

**3. Check nothing else holds the port.**

```bash
lsof | grep cu.usbmodem
```

### "No serial data received" while changing baud rate

Already fixed in `sketch.yaml` — recorded here so you know why. The Arduino IDE config
this project came from used `UploadSpeed=921600`, and on this board that fails:

```
Stub running...
Changing baud rate to 921600
A fatal error occurred: No serial data received.
```

USB-Serial-JTAG isn't a real UART, so renegotiating baud breaks the link. The profile
pins `UploadSpeed=115200`; flashing still runs at ~865 kbit/s because the USB link speed
has nothing to do with that number. **Don't "optimise" it back up.**

### "Unable to verify flash chip connection"

A previous aborted upload left the chip half-way into download mode. `tm.sh upload`
detects this, hard-resets, and retries once automatically. If it still fails, force
download mode by hand: hold **BOOT**, tap **RST**, release **BOOT**, then upload again.

### Upload starts but fails partway

Usually the cable — a marginal one enumerates but drops packets under load. Swap it.

### Monitor shows nothing

Expected, and not a fault: USB CDC only delivers what the device prints *after* you
attach, so the `setup()` banner is already gone by the time the monitor is up. Tap **RST**
with the monitor open to see it from the top.

If it is still silent after a reset, check the monitor is not fighting an upload for the
port, then re-flash.

### Nothing works and you want a second opinion

```bash
arduino-cli board list        # what arduino-cli sees
ls /dev/cu.*                  # what the OS sees
```

If `ls /dev/cu.*` shows only `cu.BLTH` and `cu.Bluetooth-Incoming-Port`, no board is
attached — go back to the cable.

---

## Version notes

- **macOS 26 (Tahoe)** has an open esptool issue
  ([arduino-esp32#11849](https://github.com/espressif/arduino-esp32/issues/11849)) causing
  upload failures. macOS 15.x and earlier are unaffected.
- **Apple Silicon** shows a one-time *"Allow accessory to connect"* prompt on first plug-in.
  Approve it in System Settings → Privacy & Security. Intel Macs don't prompt.
