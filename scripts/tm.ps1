<#
.SYNOPSIS
  tm -- tire monitor build helper (Windows / PowerShell)

.DESCRIPTION
  One entry point for everything you do with the ESP32 firmware. Thin wrapper
  over arduino-cli; all the build configuration lives in tires_esp32/sketch.yaml.
  Mirrors scripts/tm.sh command for command.

    .\scripts\tm.ps1 setup         install/verify arduino-cli, pre-download the core
    .\scripts\tm.ps1 build         compile (default profile from sketch.yaml)
    .\scripts\tm.ps1 upload        flash the last build to the board
    .\scripts\tm.ps1 flash         build + upload
    .\scripts\tm.ps1 monitor       open a serial monitor
    .\scripts\tm.ps1 port          print the detected board port and exit
    .\scripts\tm.ps1 intellisense  regenerate compile_commands.json for VS Code
    .\scripts\tm.ps1 clean         delete build artifacts
    .\scripts\tm.ps1 doctor        diagnose a board that will not show up

  Extra args pass through to arduino-cli:
    .\scripts\tm.ps1 build --profile waveshare169-latest

  If you get an execution-policy error, run this once:
    Set-ExecutionPolicy -Scope CurrentUser -ExecutionPolicy RemoteSigned
#>

[CmdletBinding()]
param(
    [Parameter(Position = 0)]
    [string]$Command = 'help',

    [Parameter(Position = 1, ValueFromRemainingArguments = $true)]
    [string[]]$Rest = @()
)

$ErrorActionPreference = 'Stop'

$RepoRoot  = Split-Path -Parent $PSScriptRoot
$SketchDir = Join-Path $RepoRoot 'tires_esp32'
$BuildDir  = Join-Path $RepoRoot 'build'

# Waveshare ESP32-S3-Touch-LCD-1.69 native USB.
# Matched on VID/PID because COM numbers get reassigned per port.
#
# 0x303A is Espressif. The PID depends on which USB mode the running firmware uses:
#   0x1001  hardware USB-Serial-JTAG -- what USBMode=hwcdc gives you, i.e. OUR build,
#           and also what the ROM bootloader presents in download mode.
#   0x821E  the Waveshare descriptor from the board variant, only used when the
#           firmware runs USB-OTG/TinyUSB (USBMode=default).
$BoardVid  = '0x303a'
$BoardPids = @('0x1001', '0x821e')
$MonitorBaud = if ($env:TM_BAUD) { $env:TM_BAUD } else { '9600' }  # USBSerial.begin(9600)

function Write-Info { param($m) Write-Host "==> $m" -ForegroundColor Green }
function Write-Warn { param($m) Write-Host "warning: $m" -ForegroundColor Yellow }
function Stop-WithError { param($m) Write-Host "error: $m" -ForegroundColor Red; exit 1 }

# --- timing -------------------------------------------------------------------
$script:LastElapsed   = 0
$script:UploadElapsed = 0

function Format-Duration {
    param([int]$Seconds)
    if ($Seconds -ge 60) { '{0}m {1:d2}s' -f [int]($Seconds / 60), ($Seconds % 60) }
    else { "${Seconds}s" }
}

# Runs a scriptblock, reports how long it took, stashes it in $script:LastElapsed.
# The scriptblock must leave $LASTEXITCODE meaningful.
function Invoke-Timed {
    param([string]$Label, [scriptblock]$Body)
    $sw = [System.Diagnostics.Stopwatch]::StartNew()
    & $Body
    $rc = $LASTEXITCODE
    $sw.Stop()
    $script:LastElapsed = [int]$sw.Elapsed.TotalSeconds
    $pretty = Format-Duration $script:LastElapsed
    if ($rc -eq 0) { Write-Info "$Label took $pretty" } else { Write-Warn "$Label failed after $pretty" }
    return $rc
}

function Invoke-Cli {
    param([string[]]$CliArgs)
    Write-Host "$ arduino-cli $($CliArgs -join ' ')" -ForegroundColor DarkGray
    & arduino-cli @CliArgs
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}

function Assert-Cli {
    if (-not (Get-Command arduino-cli -ErrorAction SilentlyContinue)) {
        Stop-WithError @"
arduino-cli not found. Install it with ONE of:

    winget install ArduinoSA.CLI
    choco install arduino-cli
    scoop install arduino-cli

Then open a NEW terminal and re-run: .\scripts\tm.ps1 setup
See docs/DEV-SETUP-WINDOWS.md
"@
    }
}

# Returns the board's COM port, or $null.
# Honours TM_PORT as an override.
function Get-BoardPort {
    if ($env:TM_PORT) { return $env:TM_PORT }

    $json = & arduino-cli board list --json 2>$null
    if ($LASTEXITCODE -ne 0 -or -not $json) { return $null }

    try { $data = $json | ConvertFrom-Json } catch { return $null }
    $ports = $data.detected_ports
    if (-not $ports) { return $null }

    $exact = @(); $espressif = @()
    foreach ($p in $ports) {
        $addr = $p.port.address
        if (-not $addr) { continue }
        $props = $p.port.properties
        # NB: do NOT name these $vid/$pid -- $PID is a read-only automatic variable
        # (the current process id) and assigning to it is a hard error.
        $devVid = if ($props.vid) { $props.vid.ToLower() } else { '' }
        $devPid = if ($props.pid) { $props.pid.ToLower() } else { '' }
        if ($devVid -eq $BoardVid -and $BoardPids -contains $devPid) { $exact += $addr }
        elseif ($devVid -eq $BoardVid) { $espressif += $addr }
    }
    if ($exact.Count)     { return $exact[0] }
    if ($espressif.Count) { return $espressif[0] }
    return $null
}

function Get-BoardPortOrDie {
    $p = Get-BoardPort
    if (-not $p) {
        Stop-WithError @"
No board detected.

Run '.\scripts\tm.ps1 doctor' -- it will tell you whether this is a cable
problem or a driver problem.

Quick checks:
  1. Many USB-C cables are CHARGE-ONLY. Try a different cable first.
  2. Try a different USB port.
  3. Check Device Manager for a device with a yellow warning triangle.
  4. Force download mode: hold BOOT, tap RST, release BOOT, then retry.
  5. Override detection: `$env:TM_PORT='COM7'; .\scripts\tm.ps1 flash
"@
    }
    return $p
}

function Invoke-Setup {
    Assert-Cli
    Write-Info "arduino-cli $((& arduino-cli version) -replace 'arduino-cli\s*','')"
    Write-Info 'Downloading the pinned ESP32 core and libraries (first run is ~1GB, be patient)'
    Invoke-Cli (@('compile', '--profile', 'waveshare169', '--build-path', $BuildDir, $SketchDir) + $Rest)
    Write-Info 'Setup complete. Plug the board in and run: .\scripts\tm.ps1 port'
}

function Invoke-Build {
    Assert-Cli
    $buildArgs = @('compile', '--build-path', $BuildDir, $SketchDir) + $Rest
    $rc = Invoke-Timed 'Build' {
        Write-Host "$ arduino-cli $($buildArgs -join ' ')" -ForegroundColor DarkGray
        & arduino-cli @buildArgs
    }
    if ($rc -ne 0) { exit $rc }
}

# Pulse the reset line via esptool to get the chip back to a known state.
function Reset-Board {
    param([string]$Port)
    $root = Join-Path $env:LOCALAPPDATA 'Arduino15\internal'
    if (-not (Test-Path $root)) { return $false }
    $esptool = Get-ChildItem -Path $root -Filter 'esptool*.exe' -Recurse -ErrorAction SilentlyContinue |
               Select-Object -First 1
    if (-not $esptool) { return $false }
    & $esptool.FullName --chip esp32s3 --port $Port --after hard_reset chip_id *> $null
    return $true
}

function Invoke-Upload {
    Assert-Cli
    $port = Get-BoardPortOrDie
    Write-Info "Uploading to $port"
    Write-Warn 'If this hangs, close any open serial monitor -- the port is exclusive.'

    $uploadArgs = @('upload', '--port', $port, '--input-dir', $BuildDir, $SketchDir) + $Rest
    $rc = Invoke-Timed 'Upload' {
        Write-Host "$ arduino-cli $($uploadArgs -join ' ')" -ForegroundColor DarkGray
        & arduino-cli @uploadArgs
    }
    if ($rc -eq 0) {
        $script:UploadElapsed = $script:LastElapsed
        Write-Info 'Upload complete. View output with: .\scripts\tm.ps1 monitor'
        return
    }

    # A failed/aborted esptool run can leave the ESP32-S3 in a half-open download
    # state, where the next attempt dies at "Unable to verify flash chip connection".
    # A hard reset clears it, so retry once before bothering the user.
    Write-Warn 'Upload failed. Hard-resetting the board and retrying once...'
    [void](Reset-Board -Port $port)
    Start-Sleep -Milliseconds 500
    $retryArgs = @('upload', '--port', (Get-BoardPortOrDie), '--input-dir', $BuildDir, $SketchDir) + $Rest
    $rc = Invoke-Timed 'Upload (retry)' {
        & arduino-cli @retryArgs
    }
    if ($rc -eq 0) {
        $script:UploadElapsed = $script:LastElapsed
        Write-Info 'Upload complete on retry. View output with: .\scripts\tm.ps1 monitor'
        return
    }

    Stop-WithError @"
Upload failed twice.

Put the board into download mode by hand and try again:
    hold BOOT, tap RST, release BOOT
    .\scripts\tm.ps1 upload

If it still fails, the cable is the usual culprit -- see docs/BUILD-AND-FLASH.md
"@
}

function Invoke-Monitor {
    Assert-Cli
    $port = Get-BoardPortOrDie
    Write-Info "Monitoring $port at $MonitorBaud baud -- press Ctrl-C to exit"
    Write-Warn 'Output only appears from the moment you attach. Tap RST to see the boot banner.'
    # --profile (run from the sketch dir) is REQUIRED: the core lives in the
    # profile's isolated directory, so a plain --fqbn lookup fails with
    # "platform esp32:esp32 is not installed", and omitting both fails with
    # "No monitor available for the port protocol serial".
    Push-Location $SketchDir
    try {
        Invoke-Cli @('monitor', '--port', $port, '--profile', 'waveshare169', '--config', "baudrate=$MonitorBaud")
    } finally { Pop-Location }
}

function Invoke-IntelliSense {
    Assert-Cli
    Write-Info 'Generating compile_commands.json for VS Code IntelliSense'
    Invoke-Cli (@('compile', '--only-compilation-database', '--build-path', $BuildDir, $SketchDir) + $Rest)
    $db = Join-Path $BuildDir 'compile_commands.json'
    if (-not (Test-Path $db)) { Stop-WithError 'compile_commands.json was not produced' }

    # arduino-cli compiles COPIES of the sketch under build\sketch\, so every entry
    # points there instead of at the files you actually edit. Without this remap,
    # opening tires_esp32\ThermalDisplay.cpp finds no matching entry and cpptools
    # falls back to a bare config -- red squiggles everywhere.
    Write-Info 'Remapping build\sketch\* entries onto the real source files'
    $entries = Get-Content $db -Raw | ConvertFrom-Json
    $added = @()
    foreach ($e in $entries) {
        if ($e.file -notmatch '[\\/]sketch[\\/]') { continue }
        $name = Split-Path $e.file -Leaf
        if ($name.EndsWith('.ino.cpp')) { $name = $name.Substring(0, $name.Length - 4) }
        $real = Join-Path $SketchDir $name
        if (-not (Test-Path $real)) { continue }
        $clone = $e | ConvertTo-Json -Depth 10 | ConvertFrom-Json
        if ($clone.PSObject.Properties.Name -contains 'command') {
            $clone.command = $clone.command.Replace($e.file, $real)
        }
        if ($clone.PSObject.Properties.Name -contains 'arguments') {
            $clone.arguments = @($clone.arguments | ForEach-Object { if ($_ -eq $e.file) { $real } else { $_ } })
        }
        $clone.file = $real
        $added += $clone
    }
    if ($added.Count) {
        ($entries + $added) | ConvertTo-Json -Depth 10 | Set-Content $db -Encoding UTF8
    }
    Write-Host "  mapped $($added.Count) source files"
    Write-Info 'Wrote build/compile_commands.json -- reload the VS Code window to pick it up'
}

function Invoke-Clean {
    Write-Info "Removing $BuildDir"
    if (Test-Path $BuildDir) { Remove-Item -Recurse -Force $BuildDir }
}

function Invoke-Doctor {
    Write-Host '=== tire monitor environment check ==='
    Write-Host ''

    Write-Host '-- toolchain --'
    if (Get-Command arduino-cli -ErrorAction SilentlyContinue) {
        Write-Host "  OK    $(& arduino-cli version)"
    } else {
        Write-Host '  MISS  arduino-cli not installed -> winget install ArduinoSA.CLI'
    }
    Write-Host ''

    Write-Host '-- USB enumeration (is the board visible to Windows at all?) --'
    # Get-PnpDevice ships only with Windows PowerShell/Windows. Guard it so that
    # `doctor` -- the thing you run WHEN things are broken -- never itself crashes.
    if (Get-Command Get-PnpDevice -ErrorAction SilentlyContinue) {
        $usb = Get-PnpDevice -PresentOnly -ErrorAction SilentlyContinue |
               Where-Object { $_.InstanceId -match 'VID_303A' }
        if ($usb) {
            $usb | ForEach-Object { Write-Host "  OK    $($_.Status)  $($_.FriendlyName)" }
            if ($usb | Where-Object { $_.Status -ne 'OK' }) {
                Write-Host '  WARN  a device is present but not OK -- check Device Manager'
            }
        } else {
            Write-Host '  FAIL  no VID_303A (Espressif) device on the USB bus.'
            Write-Host '        This is a CABLE or PORT problem, not a software problem.'
            Write-Host '        Many USB-C cables carry power only. Swap the cable, then the port.'
        }
    } else {
        Write-Host '  SKIP  Get-PnpDevice unavailable (not running on Windows)'
    }
    Write-Host ''

    Write-Host '-- serial devices --'
    $coms = @()
    try { $coms = [System.IO.Ports.SerialPort]::GetPortNames() } catch { }
    if (-not $coms -and $IsMacOS) { $coms = @(Get-ChildItem /dev/cu.* -ErrorAction SilentlyContinue | ForEach-Object Name) }
    if ($coms) { $coms | ForEach-Object { Write-Host "  $_" } } else { Write-Host '  (none)' }
    Write-Host ''

    Write-Host '-- arduino-cli board detection --'
    if (Get-Command arduino-cli -ErrorAction SilentlyContinue) {
        & arduino-cli board list | ForEach-Object { Write-Host "  $_" }
    }
    Write-Host ''

    Write-Host '-- resolved port --'
    $p = Get-BoardPort
    if ($p) { Write-Host "  OK    $p" } else { Write-Host '  FAIL  could not resolve a board port (see hints above)' }
}

function Show-Usage { Get-Help $PSCommandPath -Detailed }

switch ($Command.ToLower()) {
    'setup'        { Invoke-Setup }
    'build'        { Invoke-Build }
    'upload'       { Invoke-Upload }
    'flash'        {
        $total = [System.Diagnostics.Stopwatch]::StartNew()
        Invoke-Build
        $buildTime = $script:LastElapsed
        Invoke-Upload
        $total.Stop()
        Write-Host ''
        Write-Host '=== flash summary ===' -ForegroundColor Green
        Write-Host ("  build   {0}" -f (Format-Duration $buildTime))
        Write-Host ("  upload  {0}" -f (Format-Duration $script:UploadElapsed))
        Write-Host ("  total   {0}" -f (Format-Duration ([int]$total.Elapsed.TotalSeconds)))
    }
    'monitor'      { Invoke-Monitor }
    'port'         { Get-BoardPortOrDie }
    'intellisense' { Invoke-IntelliSense }
    'clean'        { Invoke-Clean }
    'doctor'       { Invoke-Doctor }
    { $_ -in 'help', '-h', '--help', '' } { Show-Usage }
    default        { Stop-WithError "unknown command: $Command (try -Help)" }
}
