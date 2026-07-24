#!/usr/bin/env bash
#
# tm -- tire monitor build helper (macOS / Linux)
#
# One entry point for everything you do with the ESP32 firmware. Thin wrapper
# over arduino-cli; all the build configuration lives in tires_esp32/sketch.yaml.
#
#   ./scripts/tm.sh setup         install/verify arduino-cli, pre-download the core
#   ./scripts/tm.sh build         compile (default profile from sketch.yaml)
#   ./scripts/tm.sh upload        flash the last build to the board
#   ./scripts/tm.sh flash         build + upload
#   ./scripts/tm.sh monitor       open a serial monitor
#   ./scripts/tm.sh port          print the detected board port and exit
#   ./scripts/tm.sh intellisense  regenerate compile_commands.json for VS Code
#   ./scripts/tm.sh clean         delete build artifacts
#   ./scripts/tm.sh doctor        diagnose a board that will not show up
#
# Extra args pass through to arduino-cli, e.g.:
#   ./scripts/tm.sh build --profile waveshare169-latest
#   ./scripts/tm.sh build --clean

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SKETCH_DIR="$REPO_ROOT/tires_esp32"
BUILD_DIR="$REPO_ROOT/build"

# Waveshare ESP32-S3-Touch-LCD-1.69 native USB.
# We match on VID/PID rather than a device name because macOS assigns a
# different /dev/cu.usbmodem* per physical port and Windows shuffles COM numbers.
#
# 0x303A is Espressif. The PID depends on which USB mode the running firmware uses:
#   0x1001  hardware USB-Serial-JTAG -- what USBMode=hwcdc gives you, i.e. OUR build,
#           and also what the ROM bootloader presents in download mode.
#   0x821E  the Waveshare descriptor from the board variant, only used when the
#           firmware runs USB-OTG/TinyUSB (USBMode=default).
# Accept both, then fall back to any Espressif device.
readonly BOARD_VID="0x303a"
readonly BOARD_PIDS="0x1001 0x821e"
readonly MONITOR_BAUD="${TM_BAUD:-9600}"   # matches USBSerial.begin(9600) in tires_esp32.ino

# --- output helpers -----------------------------------------------------------
if [ -t 1 ]; then
  C_RED=$'\033[31m'; C_GRN=$'\033[32m'; C_YEL=$'\033[33m'; C_DIM=$'\033[2m'; C_OFF=$'\033[0m'
else
  C_RED=''; C_GRN=''; C_YEL=''; C_DIM=''; C_OFF=''
fi
info()  { printf '%s==>%s %s\n' "$C_GRN" "$C_OFF" "$*" >&2; }
warn()  { printf '%swarning:%s %s\n' "$C_YEL" "$C_OFF" "$*" >&2; }
die()   { printf '%serror:%s %s\n' "$C_RED" "$C_OFF" "$*" >&2; exit 1; }
run()   { printf '%s$ %s%s\n' "$C_DIM" "$*" "$C_OFF" >&2; "$@"; }

# --- timing -------------------------------------------------------------------
# Integer seconds via date(1): macOS ships bash 3.2, so no EPOCHREALTIME, and
# BSD date has no %N. Second resolution is plenty for builds and flashes.
now_s()   { date +%s; }
fmt_dur() {
  local s="$1"
  if [ "$s" -ge 60 ]; then printf '%dm %02ds' "$((s / 60))" "$((s % 60))"
  else printf '%ds' "$s"; fi
}
# Prints "==> <label> took Xs" and keeps the elapsed value in TM_LAST_ELAPSED.
timed() {
  local label="$1"; shift
  local t0 rc
  t0="$(now_s)"
  set +e; "$@"; rc=$?; set -e
  TM_LAST_ELAPSED=$(( $(now_s) - t0 ))
  if [ "$rc" -eq 0 ]; then
    info "$label took $(fmt_dur "$TM_LAST_ELAPSED")"
  else
    warn "$label failed after $(fmt_dur "$TM_LAST_ELAPSED")"
  fi
  return "$rc"
}

require_cli() {
  command -v arduino-cli >/dev/null 2>&1 || die \
"arduino-cli not found. Install it with:

    brew install arduino-cli

then re-run: ./scripts/tm.sh setup
See docs/DEV-SETUP-MACOS.md"
}

# --- port detection -----------------------------------------------------------
# Prints the board's serial device, or exits non-zero with guidance.
# Honours TM_PORT as an override for when auto-detection is not what you want.
detect_port() {
  if [ -n "${TM_PORT:-}" ]; then
    printf '%s\n' "$TM_PORT"
    return 0
  fi

  local found
  found="$(arduino-cli board list --json 2>/dev/null | python3 -c '
import json, sys
try:
    data = json.load(sys.stdin)
except Exception:
    sys.exit(1)

VID = "0x303a"
PIDS = {"0x1001", "0x821e"}   # USB-Serial-JTAG (hwcdc / ROM), and TinyUSB descriptor
ports = data.get("detected_ports") or []

def prop(port, name):
    props = port.get("port", {}).get("properties") or {}
    return str(props.get(name, "")).lower()

# Prefer an exact VID+PID match, then any Espressif device, then any USB CDC device.
exact, espressif, usbcdc = [], [], []
for p in ports:
    addr = p.get("port", {}).get("address", "")
    if not addr or "/dev/tty." in addr:
        continue  # never hand back a tty.* device -- it blocks on carrier detect
    vid, pid = prop(p, "vid"), prop(p, "pid")
    if vid == VID and pid in PIDS:
        exact.append(addr)
    elif vid == VID:
        espressif.append(addr)
    elif "usbmodem" in addr.lower():
        usbcdc.append(addr)

for group in (exact, espressif, usbcdc):
    if group:
        print(group[0])
        break
' 2>/dev/null || true)"

  if [ -z "$found" ]; then
    return 1
  fi
  printf '%s\n' "$found"
}

port_or_die() {
  local p
  if ! p="$(detect_port)" || [ -z "$p" ]; then
    die "No board detected.

Run './scripts/tm.sh doctor' -- it will tell you whether this is a cable
problem or a software problem.

Quick checks:
  1. Many USB-C cables are CHARGE-ONLY. Try a different cable first.
  2. Try a different port on the Mac.
  3. Force download mode: hold BOOT, tap RST, release BOOT, then retry.
  4. Override detection if you know the device: TM_PORT=/dev/cu.usbmodemXXXX ./scripts/tm.sh flash"
  fi
  printf '%s\n' "$p"
}

# --- subcommands --------------------------------------------------------------
cmd_setup() {
  require_cli
  info "arduino-cli $(arduino-cli version | sed 's/arduino-cli *//')"
  info "Downloading the pinned ESP32 core and libraries (first run is ~1GB, be patient)"
  # A compile against the profile installs everything the profile pins.
  run arduino-cli compile --profile waveshare169 --build-path "$BUILD_DIR" "$SKETCH_DIR" "$@"
  info "Setup complete. Plug the board in and run: ./scripts/tm.sh port"
}

cmd_build() {
  require_cli
  timed "Build" run arduino-cli compile --build-path "$BUILD_DIR" "$SKETCH_DIR" "$@"
}

cmd_upload() {
  require_cli
  local port; port="$(port_or_die)"
  info "Uploading to $port"
  warn "If this hangs, close any open serial monitor -- the port is exclusive."

  if timed "Upload" run arduino-cli upload --port "$port" --input-dir "$BUILD_DIR" "$SKETCH_DIR" "$@"; then
    TM_UPLOAD_ELAPSED="$TM_LAST_ELAPSED"
    info "Upload complete. View output with: ./scripts/tm.sh monitor"
    return 0
  fi

  # A failed/aborted esptool run can leave the ESP32-S3 in a half-open download
  # state, where the next attempt dies at "Unable to verify flash chip connection".
  # A hard reset clears it, so retry once before bothering the user.
  warn "Upload failed. Hard-resetting the board and retrying once..."
  reset_board "$port" || true
  if timed "Upload (retry)" run arduino-cli upload --port "$(port_or_die)" --input-dir "$BUILD_DIR" "$SKETCH_DIR" "$@"; then
    TM_UPLOAD_ELAPSED="$TM_LAST_ELAPSED"
    info "Upload complete on retry. View output with: ./scripts/tm.sh monitor"
    return 0
  fi

  die "Upload failed twice.

Put the board into download mode by hand and try again:
    hold BOOT, tap RST, release BOOT
    ./scripts/tm.sh upload

If it still fails, the cable is the usual culprit -- see docs/BUILD-AND-FLASH.md"
}

# Pulse the reset line via esptool to get the chip back to a known state.
reset_board() {
  local port="$1" esptool
  esptool="$(find "$HOME/Library/Arduino15/internal" -maxdepth 2 -name esptool -type f 2>/dev/null | head -1)"
  [ -n "$esptool" ] || return 1
  "$esptool" --chip esp32s3 --port "$port" --after hard_reset chip_id >/dev/null 2>&1
}

cmd_flash() {
  local t0 build upload
  t0="$(now_s)"
  cmd_build
  build="$TM_LAST_ELAPSED"
  cmd_upload
  upload="${TM_UPLOAD_ELAPSED:-0}"
  printf '\n%s=== flash summary ===%s\n' "$C_GRN" "$C_OFF" >&2
  printf '  build   %s\n' "$(fmt_dur "$build")"  >&2
  printf '  upload  %s\n' "$(fmt_dur "$upload")" >&2
  printf '  total   %s\n' "$(fmt_dur "$(( $(now_s) - t0 ))")" >&2
}

cmd_monitor() {
  require_cli
  local port; port="$(port_or_die)"
  info "Monitoring $port at ${MONITOR_BAUD} baud -- press Ctrl-] or Ctrl-C to exit"
  warn "Output only appears from the moment you attach. Tap RST to see the boot banner."
  # --profile (run from the sketch dir) is REQUIRED: the core is installed in the
  # profile's isolated directory, so a plain --fqbn lookup fails with
  # "platform esp32:esp32 is not installed", and omitting both fails with
  # "No monitor available for the port protocol serial".
  # Baud itself is ignored by USB-Serial-JTAG, but matters on the real UART pins.
  ( cd "$SKETCH_DIR" && run arduino-cli monitor \
      --port "$port" --profile waveshare169 --config "baudrate=$MONITOR_BAUD" )
}

cmd_port() {
  require_cli
  port_or_die
}

cmd_intellisense() {
  require_cli
  info "Generating compile_commands.json for VS Code IntelliSense"
  run arduino-cli compile --only-compilation-database \
      --build-path "$BUILD_DIR" "$SKETCH_DIR" "$@"
  [ -f "$BUILD_DIR/compile_commands.json" ] || die "compile_commands.json was not produced"

  # arduino-cli compiles COPIES of the sketch under build/sketch/, so every entry
  # points there instead of at the files you actually edit. Without this remap,
  # opening tires_esp32/ThermalDisplay.cpp finds no matching entry and cpptools
  # falls back to a bare config -- red squiggles everywhere.
  # Duplicate each entry against the real path (.ino.cpp maps back to .ino).
  info "Remapping build/sketch/* entries onto the real source files"
  python3 - "$BUILD_DIR/compile_commands.json" "$SKETCH_DIR" <<'PY'
import json, os, sys

db_path, sketch_dir = sys.argv[1], sys.argv[2]
with open(db_path) as fh:
    entries = json.load(fh)

marker = os.sep + "sketch" + os.sep
added = []
for e in entries:
    f = e.get("file", "")
    if marker not in f:
        continue
    name = os.path.basename(f)
    if name.endswith(".ino.cpp"):
        name = name[: -len(".cpp")]          # tires_esp32.ino.cpp -> tires_esp32.ino
    real = os.path.join(sketch_dir, name)
    if not os.path.exists(real):
        continue
    clone = dict(e)
    clone["file"] = real
    if "command" in clone:
        clone["command"] = clone["command"].replace(f, real)
    if "arguments" in clone:
        clone["arguments"] = [real if a == f else a for a in clone["arguments"]]
    added.append(clone)

if added:
    with open(db_path, "w") as fh:
        json.dump(entries + added, fh, indent=2)
print(f"  mapped {len(added)} source files", file=sys.stderr)
PY
  info "Wrote build/compile_commands.json -- reload the VS Code window to pick it up"
}

cmd_clean() {
  info "Removing $BUILD_DIR"
  rm -rf "$BUILD_DIR"
}

cmd_doctor() {
  echo "=== tire monitor environment check ==="
  echo

  echo "-- toolchain --"
  if command -v arduino-cli >/dev/null 2>&1; then
    echo "  OK    $(arduino-cli version)"
  else
    echo "  MISS  arduino-cli not installed -> brew install arduino-cli"
  fi
  if arduino-cli core list 2>/dev/null | grep -q '^esp32:esp32'; then
    echo "  OK    esp32 core: $(arduino-cli core list 2>/dev/null | grep '^esp32:esp32')"
  else
    echo "  INFO  esp32 core not in the global list (normal -- profile builds keep it isolated)"
  fi
  echo

  echo "-- USB enumeration (is the board visible to macOS at all?) --"
  if [ "$(uname)" = "Darwin" ]; then
    local usb
    usb="$(system_profiler SPUSBDataType 2>/dev/null | grep -i -B2 -A8 'ESP32\|Waveshare\|USB JTAG' || true)"
    if [ -n "$usb" ]; then
      echo "  OK    device is enumerated:"
      printf '%s\n' "$usb" | sed 's/^/        /'
    else
      echo "  FAIL  no ESP32 device on the USB bus."
      echo "        This is a CABLE or PORT problem, not a software problem."
      echo "        Many USB-C cables carry power only. Swap the cable, then the port."
    fi
  fi
  echo

  echo "-- serial devices --"
  # shellcheck disable=SC2012
  ls /dev/cu.* 2>/dev/null | sed 's/^/  /' || echo "  (none)"
  echo

  echo "-- arduino-cli board detection --"
  arduino-cli board list 2>/dev/null | sed 's/^/  /' || echo "  (arduino-cli unavailable)"
  echo

  echo "-- resolved port --"
  local p
  if p="$(detect_port)" && [ -n "$p" ]; then
    echo "  OK    $p"
  else
    echo "  FAIL  could not resolve a board port (see hints above)"
  fi
}

usage() {
  sed -n '2,26p' "${BASH_SOURCE[0]}" | sed 's/^#//; s/^ //'
}

main() {
  local cmd="${1:-}"
  [ $# -gt 0 ] && shift || true
  case "$cmd" in
    setup)        cmd_setup "$@" ;;
    build)        cmd_build "$@" ;;
    upload)       cmd_upload "$@" ;;
    flash)        cmd_flash "$@" ;;
    monitor)      cmd_monitor "$@" ;;
    port)         cmd_port "$@" ;;
    intellisense) cmd_intellisense "$@" ;;
    clean)        cmd_clean "$@" ;;
    doctor)       cmd_doctor "$@" ;;
    ""|-h|--help|help) usage ;;
    *)            die "unknown command: $cmd (try --help)" ;;
  esac
}

main "$@"
