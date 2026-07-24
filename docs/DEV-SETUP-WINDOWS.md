# Dev environment setup — Windows

Sets up building and flashing the `tires_esp32` firmware from VS Code on Windows.
Start to finish: ~15 minutes, most of it one large download.

You do **not** need the Arduino IDE. Everything runs through `arduino-cli`, which is the
same toolchain the Arduino IDE uses internally.

> **Handing this to Claude Code?** Every step below is a literal command. Say:
> *"Follow docs/DEV-SETUP-WINDOWS.md and get the firmware building."*

> The macOS guide was verified end to end on real hardware. This one mirrors it, with the
> Windows-specific differences called out. The `sketch.yaml` build definition is identical
> on both platforms, so builds are byte-for-byte reproducible across them.

---

## 1. Install arduino-cli

Pick whichever package manager you have (PowerShell):

```powershell
winget install ArduinoSA.CLI
# or:  choco install arduino-cli
# or:  scoop install arduino-cli
```

No package manager? Download the 64-bit MSI from
<https://arduino.github.io/arduino-cli/latest/installation/>.

**Open a new terminal** so `PATH` picks it up, then check:

```powershell
arduino-cli version      # expect 1.5.x or newer
```

## 2. Allow the helper script to run

Windows blocks unsigned scripts by default. Once per machine:

```powershell
Set-ExecutionPolicy -Scope CurrentUser -ExecutionPolicy RemoteSigned
```

(The VS Code tasks pass `-ExecutionPolicy Bypass` and work regardless, but this makes the
script runnable directly from a terminal.)

## 3. Install VS Code extensions

```powershell
code --install-extension ms-vscode.cpptools
code --install-extension ms-vscode.vscode-serial-monitor
```

Or open the repo in VS Code and accept the "recommended extensions" prompt.

> Do **not** install the Arduino extension. Microsoft archived it in October 2024, and its
> community fork manages its own `.vscode/arduino.json`, which fights the `sketch.yaml`
> build profile this project uses. (The old, now-deleted `arduino.json` in this repo is
> where the `COM7` and `new_menu_test` leftovers came from.)

## 4. First build

From the repo root:

```powershell
.\scripts\tm.ps1 setup
```

This downloads the pinned ESP32 core (`esp32:esp32` 3.0.7) and all seven libraries, then
compiles. **The first run downloads roughly 1 GB and can take 5–15 minutes.** Later builds
take seconds.

Expected result:

```
Sketch uses 989649 bytes (31%) of program storage space. Maximum is 3145728 bytes.
Global variables use 48872 bytes (14%) of dynamic memory...
```

## 5. Generate IntelliSense data

```powershell
.\scripts\tm.ps1 intellisense
```

Then reload VS Code (`Ctrl+Shift+P` → *Developer: Reload Window*).

## 6. Connect the board

```powershell
.\scripts\tm.ps1 port
```

Expect something like `COM7`. If it errors, run `.\scripts\tm.ps1 doctor`.

## 7. Flash it

```powershell
.\scripts\tm.ps1 flash      # compile + upload
.\scripts\tm.ps1 monitor    # watch it boot
```

Expected serial output:

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

`CST816Touch initialized.` is the line that proves the pin map is right. The repeating
`I2C error` lines are expected with no sensors attached.

---

## Windows-specific notes

### Drivers

This board uses the ESP32-S3's **native USB** (USB-Serial-JTAG), enumerating as
**VID `0x303A`, PID `0x1001`**. Windows 10/11 bind their in-box USB CDC driver
automatically — **no CP210x/CH340 driver needed**. Tutorials telling you to install the
Silicon Labs VCP driver are for different boards.

If Device Manager shows the device with a yellow warning triangle, right-click →
*Update driver* → *Search automatically*.

### COM port numbers move

Windows assigns a COM number per USB port, and hands out a different one when the board
re-enumerates into download mode. That's why the scripts resolve the port by USB VID/PID
rather than hardcoding a number. Override when you need to:

```powershell
$env:TM_PORT = 'COM7'
.\scripts\tm.ps1 flash
```

### Only one program can hold the port

Close the serial monitor before uploading — this is the most common cause of an upload
that fails right after a successful one.

### Line endings

`.gitattributes` is not set in this repo. If git converts `scripts/tm.sh` to CRLF it will
not run under WSL or Git Bash. It doesn't affect the PowerShell path.

### Python

`tm.ps1` does all its JSON handling natively in PowerShell, so — unlike `tm.sh` — it does
**not** need Python installed.

---

## Troubleshooting

Run this first:

```powershell
.\scripts\tm.ps1 doctor
```

It reports the toolchain, whether Windows sees a `VID_303A` device at all, the COM ports
present, and the resolved port.

### "No board detected"

**1. Suspect the cable first.** Many USB-C cables are charge-only. Check whether Windows
sees the device at all:

```powershell
Get-PnpDevice -PresentOnly | Where-Object InstanceId -match 'VID_303A'
```

- **Rows returned** → enumerated; keep reading.
- **Nothing** → cable or port. Swap for a cable you know carries data, then try a
  different USB port. No software change will fix this.

**2. Force download mode.** Hold **BOOT**, tap **RST**, release **BOOT**, then retry the
upload.

### "No serial data received" while changing baud rate

Already handled: `sketch.yaml` pins `UploadSpeed=115200`. USB-Serial-JTAG isn't a real
UART and fails when esptool renegotiates to 921600. Don't raise it.

### "Unable to verify flash chip connection"

A previous aborted upload left the chip half-way into download mode. `tm.ps1 upload`
hard-resets and retries once by itself; if it still fails, use BOOT+RST as above.

### Monitor shows nothing

USB CDC only delivers output produced after you attach, so the boot banner is long gone.
Tap **RST** with the monitor open.
