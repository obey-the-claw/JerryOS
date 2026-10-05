<#
.SYNOPSIS
  JerryOS Step 0: back up the AIPI Lite's original firmware, and prove the backup is good.

.DESCRIPTION
  READ-ONLY. Nothing in this script writes to the device. It:
    1. Installs/updates esptool (Espressif's official flashing tool) with pip.
    2. Finds the AIPI Lite's USB serial port (Espressif USB vendor ID 303A).
    3. Records the chip, flash size, PSRAM, MAC, and security settings.
    4. Reads the ENTIRE flash chip to backup\aipi-lite-stock-<date>.bin.
    5. Reads it a SECOND time and checks both reads are byte-identical.
    6. Asks the chip to checksum its own flash and compares that to the file.
    7. Parses the file to confirm it contains a bootloader, partition table,
       and firmware image (tools\inspect_backup.py).
  Everything is logged to backup\backup-log-<date>.txt.

.PARAMETER Port
  COM port, e.g. COM5. If omitted, the script finds the device automatically.

.PARAMETER SkipInstall
  Don't run pip install (use the esptool you already have).

.EXAMPLE
  powershell -ExecutionPolicy Bypass -File tools\backup_firmware.ps1
.EXAMPLE
  powershell -ExecutionPolicy Bypass -File tools\backup_firmware.ps1 -Port COM7
#>
param(
    [string]$Port,
    [switch]$SkipInstall
)

$ErrorActionPreference = 'Stop'
$RepoRoot  = Split-Path -Parent $PSScriptRoot
$BackupDir = Join-Path $RepoRoot 'backup'
$Stamp     = Get-Date -Format 'yyyyMMdd-HHmm'
$DumpFile  = Join-Path $BackupDir "aipi-lite-stock-$Stamp.bin"
$CheckFile = Join-Path $env:TEMP  "aipi-lite-check-$Stamp.bin"
$InfoFile  = Join-Path $BackupDir "device-info-$Stamp.txt"
$LogFile   = Join-Path $BackupDir "backup-log-$Stamp.txt"

New-Item -ItemType Directory -Force -Path $BackupDir | Out-Null

function Say($msg, $color = 'Cyan') {
    Write-Host ''
    Write-Host "==> $msg" -ForegroundColor $color
    Add-Content -Path $LogFile -Value "`r`n==> $msg"
}

function Fail($msg) {
    Write-Host ''
    Write-Host "STOP: $msg" -ForegroundColor Red
    Add-Content -Path $LogFile -Value "`r`nSTOP: $msg"
    Write-Host "Full log: $LogFile"
    exit 1
}

# Find Python: prefer the Windows 'py' launcher, fall back to 'python'.
$Py = $null; $PyArgs = @()
if (Get-Command py -ErrorAction SilentlyContinue)         { $Py = 'py'; $PyArgs = @('-3') }
elseif (Get-Command python -ErrorAction SilentlyContinue) { $Py = 'python' }
else { Fail 'Python 3 not found. Install it from https://www.python.org/downloads/ (tick "Add python.exe to PATH") and re-run.' }

# Run python with arguments, show + log the output, return the output lines.
# Native tools write progress to stderr, so stderr is merged in and errors are
# judged by exit code only.
function Run-Py([string[]]$ToolArgs, [string]$AlsoSaveTo) {
    Add-Content -Path $LogFile -Value ("`r`n> $Py " + (($PyArgs + $ToolArgs) -join ' '))
    $prev = $ErrorActionPreference; $ErrorActionPreference = 'Continue'
    $out = & $Py @PyArgs @ToolArgs 2>&1 | ForEach-Object { "$_" }
    $code = $LASTEXITCODE
    $ErrorActionPreference = $prev
    $out | ForEach-Object { Write-Host $_ }
    Add-Content -Path $LogFile -Value $out
    if ($AlsoSaveTo) { Add-Content -Path $AlsoSaveTo -Value $out }
    return [pscustomobject]@{ Code = $code; Lines = $out }
}

Set-Content -Path $LogFile -Value "JerryOS firmware backup log - $(Get-Date)"

# ---------------------------------------------------------------- 1. esptool
if (-not $SkipInstall) {
    Say 'Installing/updating esptool (Espressif''s official tool)...'
    $r = Run-Py @('-m', 'pip', 'install', '--upgrade', 'esptool>=5,<6')
    if ($r.Code -ne 0) { Fail 'pip install esptool failed (see above).' }
}
$r = Run-Py @('-m', 'esptool', 'version')
if ($r.Code -ne 0) { Fail 'esptool is not runnable. Re-run without -SkipInstall.' }

# ---------------------------------------------------------------- 2. find port
if (-not $Port) {
    Say 'Looking for the AIPI Lite on USB (Espressif vendor ID 303A)...'
    $devs = Get-CimInstance Win32_PnPEntity |
        Where-Object { $_.Name -match '\((COM\d+)\)' -and $_.PNPDeviceID -match 'VID_303A' }
    if (-not $devs) {
        $all = Get-CimInstance Win32_PnPEntity | Where-Object { $_.Name -match '\((COM\d+)\)' }
        if ($all) { Write-Host 'COM ports present (none look like an ESP32-S3):'; $all | ForEach-Object { Write-Host "   $($_.Name)" } }
        Fail ('No Espressif USB device found. Check: (1) the USB-C cable carries data, not just power; ' +
              '(2) the device is switched on; (3) try another USB port. Or pass -Port COMx.')
    }
    if (@($devs).Count -gt 1) {
        $devs | ForEach-Object { Write-Host "   $($_.Name)" }
        Fail 'More than one Espressif device is plugged in. Unplug the others or pass -Port COMx.'
    }
    $Port = [regex]::Match(@($devs)[0].Name, 'COM\d+').Value
    Write-Host "Found: $(@($devs)[0].Name)" -ForegroundColor Green
}
$Base = @('-m', 'esptool', '--chip', 'esp32s3', '--port', $Port)

# ---------------------------------------------------------------- 3. device facts
Say "Reading chip facts from the device on $Port (read-only)..."
Set-Content -Path $InfoFile -Value "AIPI Lite device info - $(Get-Date) - port $Port"
$r = Run-Py ($Base + @('flash-id')) $InfoFile
if ($r.Code -ne 0) {
    Fail ('esptool could not talk to the chip. Unplug/replug and retry. If it still fails, the stock ' +
          'firmware may be blocking USB auto-reset: see backup\README.md, "If esptool cannot connect".')
}
$facts = $r.Lines -join "`n"
if ($facts -match 'Secure Download Mode') {
    Fail 'The chip is locked in Secure Download Mode, which blocks reading flash. Nothing was changed. Tell Claude.'
}
$r = Run-Py ($Base + @('get-security-info')) $InfoFile
$sec = $r.Lines -join "`n"
$r = Run-Py @('-m', 'espefuse', '--chip', 'esp32s3', '--port', $Port, 'summary') $InfoFile

# Quick summary of what matters for us.
$flashSize = [regex]::Match($facts, 'Detected flash size:\s*(\S+)').Groups[1].Value
$psram     = [regex]::Match($facts, '(Embedded PSRAM[^\r\n]*)').Groups[1].Value
$mac       = [regex]::Match($facts, 'MAC:\s*([0-9a-fA-F:]{17})').Groups[1].Value
Say 'What the device says about itself:' 'Green'
Write-Host "   Flash size : $flashSize"
Write-Host "   PSRAM      : $(if ($psram) { $psram } else { '(not reported - see device-info file)' })"
Write-Host "   MAC        : $mac"

if ($sec -match 'Flash Encryption:\s*Enabled' -or $sec -match 'Flash encryption:\s*enabled') {
    Write-Host '   WARNING: flash encryption is ENABLED. The backup will be encrypted, but it can still be restored to THIS unit.' -ForegroundColor Yellow
}
if ($sec -match 'Secure Boot:\s*Enabled' -or $sec -match 'Secure boot:\s*enabled') {
    Fail ('Secure Boot is ENABLED on this chip. It will refuse unsigned custom firmware. Do NOT try to flash JerryOS - ' +
          'tell Claude and we will rethink. (Your backup has not been taken yet; nothing was changed.)')
}

$expect = if ($flashSize) { $flashSize } else { '16MB' }

# ---------------------------------------------------------------- 4. read #1
Say "Reading the ENTIRE flash ($expect) -> $DumpFile . This takes a few minutes; don't unplug."
$r = Run-Py ($Base + @('read-flash', '0', 'ALL', $DumpFile))
if ($r.Code -ne 0) { Fail 'First flash read failed (see above).' }

# ---------------------------------------------------------------- 5. read #2
Say 'Reading it AGAIN to make sure both reads match (catches flaky USB)...'
$r = Run-Py ($Base + @('read-flash', '0', 'ALL', $CheckFile))
if ($r.Code -ne 0) { Fail 'Second flash read failed (see above).' }
$h1 = (Get-FileHash $DumpFile  -Algorithm SHA256).Hash
$h2 = (Get-FileHash $CheckFile -Algorithm SHA256).Hash
Remove-Item $CheckFile -ErrorAction SilentlyContinue
Write-Host "   read 1 SHA256: $h1"
Write-Host "   read 2 SHA256: $h2"
if ($h1 -ne $h2) { Fail 'The two reads are DIFFERENT. Do not trust this backup. Try another cable/port and re-run.' }
Write-Host '   Both reads are identical.' -ForegroundColor Green

# ---------------------------------------------------------------- 6. on-chip verify
Say 'Asking the chip to checksum its own flash and compare against the file...'
$r = Run-Py ($Base + @('verify-flash', '0x0', $DumpFile))
if ($r.Code -ne 0) { Fail 'verify-flash says the file does NOT match the chip.' }

# ---------------------------------------------------------------- 7. inspect contents
Say 'Checking the file actually contains bootable firmware...'
$r = Run-Py @((Join-Path $PSScriptRoot 'inspect_backup.py'), $DumpFile, '--expect-size', $expect) $InfoFile
if ($r.Code -ne 0) { Fail 'The dump does not look like a valid firmware image (see RESULT above).' }

# ---------------------------------------------------------------- done
"$($h1.ToLower())  $(Split-Path -Leaf $DumpFile)" | Set-Content -Path "$DumpFile.sha256" -Encoding ascii

Say 'BACKUP CONFIRMED' 'Green'
Write-Host "   Firmware dump : $DumpFile"
Write-Host "   SHA256 file   : $DumpFile.sha256"
Write-Host "   Device info   : $InfoFile"
Write-Host "   Full log      : $LogFile"
Write-Host ''
Write-Host 'Next: copy the .bin somewhere OFF this laptop too (USB stick / cloud drive).' -ForegroundColor Yellow
Write-Host 'Then send Claude the device-info file contents. Checkpoint 1 done.' -ForegroundColor Yellow
