<#
.SYNOPSIS
  Put the AIPI Lite back to factory firmware from a backup made by backup_firmware.ps1.

.DESCRIPTION
  WRITES THE WHOLE FLASH CHIP. This erases JerryOS (or anything else on the
  device) and writes back the exact bytes from your backup, so the device
  boots the original AIPI firmware again.

  Safety checks before writing:
    - the backup's SHA256 matches its .sha256 file (the file isn't corrupted)
    - tools\inspect_backup.py says the file is a valid firmware image
    - you type RESTORE to confirm
  After writing, esptool reads it back and verifies it.

.PARAMETER Backup
  Path to the .bin backup. If omitted, uses the newest backup\aipi-lite-stock-*.bin.

.PARAMETER Port
  COM port, e.g. COM5. If omitted, the script finds the device automatically.

.EXAMPLE
  powershell -ExecutionPolicy Bypass -File tools\restore_firmware.ps1
#>
param(
    [string]$Backup,
    [string]$Port
)

$ErrorActionPreference = 'Stop'
$RepoRoot  = Split-Path -Parent $PSScriptRoot
$BackupDir = Join-Path $RepoRoot 'backup'

function Fail($msg) { Write-Host ''; Write-Host "STOP: $msg" -ForegroundColor Red; exit 1 }

$Py = $null; $PyArgs = @()
if (Get-Command py -ErrorAction SilentlyContinue)         { $Py = 'py'; $PyArgs = @('-3') }
elseif (Get-Command python -ErrorAction SilentlyContinue) { $Py = 'python' }
else { Fail 'Python 3 not found.' }

function Run-Py([string[]]$ToolArgs) {
    $prev = $ErrorActionPreference; $ErrorActionPreference = 'Continue'
    & $Py @PyArgs @ToolArgs 2>&1 | ForEach-Object { Write-Host "$_" }
    $code = $LASTEXITCODE
    $ErrorActionPreference = $prev
    return $code
}

# Pick the backup file.
if (-not $Backup) {
    $latest = Get-ChildItem -Path $BackupDir -Filter 'aipi-lite-stock-*.bin' -ErrorAction SilentlyContinue |
        Sort-Object LastWriteTime -Descending | Select-Object -First 1
    if (-not $latest) { Fail "No backup\aipi-lite-stock-*.bin found. Pass -Backup <path>." }
    $Backup = $latest.FullName
}
if (-not (Test-Path $Backup)) { Fail "Backup file not found: $Backup" }
Write-Host "Backup file: $Backup" -ForegroundColor Cyan

# Check SHA256 against the .sha256 file written at backup time.
$shaFile = "$Backup.sha256"
if (Test-Path $shaFile) {
    $want = ((Get-Content $shaFile -Raw).Trim() -split '\s+')[0].ToLower()
    $have = (Get-FileHash $Backup -Algorithm SHA256).Hash.ToLower()
    if ($want -ne $have) { Fail "SHA256 mismatch - this backup file has changed since it was made. Do not use it." }
    Write-Host 'SHA256 matches the original backup.' -ForegroundColor Green
} else {
    Write-Host "WARNING: no $shaFile to check against. Continuing on inspect_backup.py alone." -ForegroundColor Yellow
}

# Check it's a sane image.
if ((Run-Py @((Join-Path $PSScriptRoot 'inspect_backup.py'), $Backup)) -ne 0) {
    Fail 'The backup does not look like a valid firmware image.'
}

# Find the port.
if (-not $Port) {
    $devs = Get-CimInstance Win32_PnPEntity |
        Where-Object { $_.Name -match '\((COM\d+)\)' -and $_.PNPDeviceID -match 'VID_303A' }
    if (@($devs).Count -ne 1) { Fail 'Could not pick exactly one Espressif USB device. Pass -Port COMx.' }
    $Port = [regex]::Match(@($devs)[0].Name, 'COM\d+').Value
}

Write-Host ''
Write-Host "About to OVERWRITE the entire flash of the device on $Port with this backup." -ForegroundColor Yellow
$answer = Read-Host 'Type RESTORE to continue'
if ($answer -cne 'RESTORE') { Fail 'Cancelled. Nothing was written.' }

# Write the whole image at address 0. esptool keeps the image's own flash
# mode/frequency/size header bytes untouched (its default is "keep"), so the
# result is byte-for-byte the original.
$code = Run-Py @('-m', 'esptool', '--chip', 'esp32s3', '--port', $Port, 'write-flash', '0x0', $Backup)
if ($code -ne 0) { Fail 'write-flash failed. The device may not boot until a restore succeeds - just re-run this script.' }

$code = Run-Py @('-m', 'esptool', '--chip', 'esp32s3', '--port', $Port, 'verify-flash', '0x0', $Backup)
if ($code -ne 0) { Fail 'Restore written, but verification FAILED. Re-run this script.' }

Write-Host ''
Write-Host 'RESTORE COMPLETE. The device should now boot the original AIPI firmware.' -ForegroundColor Green
