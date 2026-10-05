<#
.SYNOPSIS
  Flash a ready-made JerryOS firmware file from the firmware\ folder.

.DESCRIPTION
  For when you want to put JerryOS on the device without building it
  yourself. Uses the same esptool as the backup script.

  It checks the file's SHA256 before writing. Your original firmware stays
  safe in backup\, and tools\restore_firmware.ps1 puts it back any time.

.PARAMETER Firmware
  Path to the .bin. Default: the newest firmware\jerryos-cp*.bin

.PARAMETER Port
  COM port, e.g. COM7. If omitted, the script finds the device automatically.

.EXAMPLE
  powershell -ExecutionPolicy Bypass -File tools\flash_prebuilt.ps1 -Port COM7
#>
param(
    [string]$Firmware,
    [string]$Port
)

$ErrorActionPreference = 'Stop'
function Fail($msg) { Write-Host ''; Write-Host "STOP: $msg" -ForegroundColor Red; exit 1 }

$RepoRoot = Split-Path -Parent $PSScriptRoot
if (-not $Firmware) {
    # Newest checkpoint build (cp3 sorts after cp2, and so on).
    $latest = Get-ChildItem -Path (Join-Path $RepoRoot 'firmware') -Filter 'jerryos-cp*.bin' -ErrorAction SilentlyContinue |
        Sort-Object Name -Descending | Select-Object -First 1
    if (-not $latest) { Fail 'No firmware\jerryos-cp*.bin found. Pull the latest from GitHub.' }
    $Firmware = $latest.FullName
}

$Py = $null; $PyArgs = @()
if (Get-Command py -ErrorAction SilentlyContinue)         { $Py = 'py'; $PyArgs = @('-3') }
elseif (Get-Command python -ErrorAction SilentlyContinue) { $Py = 'python' }
else { Fail 'Python 3 not found.' }

if (-not (Test-Path $Firmware)) { Fail "Firmware file not found: $Firmware" }

$shaFile = "$Firmware.sha256"
if (Test-Path $shaFile) {
    $want = ((Get-Content $shaFile -Raw).Trim() -split '\s+')[0].ToLower()
    $have = (Get-FileHash $Firmware -Algorithm SHA256).Hash.ToLower()
    if ($want -ne $have) { Fail 'SHA256 mismatch - the firmware file is damaged or was changed. Re-download it.' }
    Write-Host 'Firmware file SHA256 OK.' -ForegroundColor Green
}

if (-not $Port) {
    $devs = Get-CimInstance Win32_PnPEntity |
        Where-Object { $_.Name -match '\((COM\d+)\)' -and $_.PNPDeviceID -match 'VID_303A' }
    if (@($devs).Count -ne 1) {
        Fail ('Could not find exactly one AIPI Lite on USB. Pass -Port COMx. (Tip from Checkpoint 1: ' +
              'if it will not show up, switch it fully off, take the battery off, then plug USB back in.)')
    }
    $Port = [regex]::Match(@($devs)[0].Name, 'COM\d+').Value
}

Write-Host "Flashing $(Split-Path -Leaf $Firmware) to $Port ..." -ForegroundColor Cyan
$prev = $ErrorActionPreference; $ErrorActionPreference = 'Continue'
& $Py @PyArgs -m esptool --chip esp32s3 --port $Port write-flash 0x0 $Firmware 2>&1 | ForEach-Object { Write-Host "$_" }
$code = $LASTEXITCODE
$ErrorActionPreference = $prev
if ($code -ne 0) { Fail 'Flashing failed (see above). Unplug/replug and try again.' }

Write-Host ''
Write-Host 'DONE. The device is restarting into JerryOS.' -ForegroundColor Green
