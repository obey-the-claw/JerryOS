# /backup: the original AIPI Lite firmware

This folder holds a byte-for-byte copy of everything that was on the AIPI
Lite's 16 MB flash chip before JerryOS touched it. It's the undo button: as
long as this file exists, the device can go back to exactly how it shipped.

## What's in here (after you run the backup)

| File | What it is |
|---|---|
| `aipi-lite-stock-<date>.bin` | The full flash dump: bootloader, partition table, firmware, settings, assets. 16 MB. |
| `aipi-lite-stock-<date>.bin.sha256` | Its fingerprint. If even one byte changes, this won't match. |
| `device-info-<date>.txt` | What the chip reported about itself: chip, flash, PSRAM, MAC, security settings, eFuses, and the partition table parsed from the dump. |
| `backup-log-<date>.txt` | Everything the backup script printed. |

> ⚠️ **Privacy:** the dump includes the stock firmware's settings area (NVS).
> If you ever entered your Wi-Fi password into the stock AIPI app/firmware,
> it is in this file. That's fine in a **private** repo. If this repo ever
> goes public (the plug-and-play kit), the `.bin` must be removed from
> git history first, not just deleted.

## How to make the backup (Windows)

1. Install **Python 3** from https://www.python.org/downloads/ and tick
   **"Add python.exe to PATH"** in the installer.
2. Plug the AIPI Lite into the laptop with a USB-C cable that carries
   **data** (some cables only charge). Switch the device on.
3. Open **PowerShell** in the repo folder and run:

   ```powershell
   powershell -ExecutionPolicy Bypass -File tools\backup_firmware.ps1
   ```

   (`-ExecutionPolicy Bypass` lets Windows run a script you downloaded,
   just this once. It changes no system settings.)
4. Wait a few minutes. It reads the chip **twice**. Don't unplug it.
5. Look for **`BACKUP CONFIRMED`** in green at the end.
6. Copy the `.bin` somewhere **off the laptop** too (USB stick, cloud drive).

The script only **reads**. Nothing on the device changes. Afterwards the
device reboots into its normal stock firmware.

## How we know the backup is good

The script refuses to say "confirmed" unless all of these pass:

1. **Two reads, identical.** Reads the whole chip twice and compares SHA256
   fingerprints. A flaky cable gives two different results.
2. **Chip agrees.** `esptool verify-flash` makes the chip checksum its own
   flash and compares that to the file. This proves the file matches the
   device, not just itself.
3. **It's really firmware.** `tools/inspect_backup.py` checks the file is the
   right size, starts with an ESP bootloader (magic byte `0xE9`), has a valid
   partition table with a matching MD5, and contains at least one firmware
   image. It prints that image's name, version and build date. An all-blank
   or garbage read fails here.
4. **Chip isn't locked.** It checks Secure Boot is off. If it were on, the
   chip would refuse our custom firmware, so the script stops and says so
   before anything else happens.

You can re-check a backup any time:

```powershell
py tools\inspect_backup.py backup\aipi-lite-stock-<date>.bin
```

## How to restore the original firmware

```powershell
powershell -ExecutionPolicy Bypass -File tools\restore_firmware.ps1
```

It picks the newest backup and checks its SHA256 against the `.sha256` file.
It re-inspects the image, then asks you to type `RESTORE`. Then it writes all
16 MB back and verifies it. After a reboot the device runs stock AIPI
firmware again.

Manual equivalent, if you ever need it without the script (replace `COM5`):

```powershell
py -m esptool --chip esp32s3 --port COM5 write-flash 0x0 backup\aipi-lite-stock-<date>.bin
py -m esptool --chip esp32s3 --port COM5 verify-flash 0x0 backup\aipi-lite-stock-<date>.bin
```

(esptool v4 and older spell these `write_flash` / `verify_flash`.)

## If esptool cannot connect

The AIPI Lite has no separate USB chip. The ESP32-S3 handles USB itself, and
esptool normally resets it into "download mode" over USB automatically. If
that fails:

1. Unplug, replug, re-run. Try a different USB port or cable.
2. Close anything else that might have the COM port open (Arduino IDE, a
   serial monitor, Cura, etc.).
3. Last resort, still no soldering: there's a **BOOT button under the back
   cover** (4 screws; mind the screen ribbon and speaker wires). Hold BOOT,
   plug in USB, release BOOT, then run the script again. The device screen
   stays dark in this mode; that's expected.
