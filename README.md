# JerryOS

Custom firmware for the **AIPI Lite** (ESP32-S3) that turns it into Jerry the
Squatch's pocket/dash sidekick, "Chief Gear Inspector" for the
[5quatch Rated](https://5quatchrated.com) rig.

Jerry's animated head lives on the screen. The v0 modes are:

- **Tracker Watch:** passive BLE scan for AirTag / Tile / SmartTag-style
  trackers that keep following you.
- **Rig Dashboard:** placeholder for the Scanner v0 Pi Zero link.
- **Settings.**

Two buttons, a hint bar that always tells you what they do, and the device
stays physically stock (no soldering).

## Status

| # | Checkpoint | State |
|---|---|---|
| 1 | Firmware backup confirmed | ✅ 2026-10-04 |
| 2 | Hello world: screen, buttons, battery, serial log | ✅ 2026-10-04 |
| 3 | JerryOS shell: home, menu, hint bar, modes (placeholder faces) | 🟡 ready to test ([guide](docs/CHECKPOINT3.md)) |
| 4 | Tracker Watch working | ⏳ |
| 5 | Art spec + PNG converter | ⏳ |

See [BUILDLOG.md](BUILDLOG.md) for the story so far.

![Jerry's moods](docs/img/cp3-jerry-moods.png)

## Repo layout

```
backup/         original firmware dump + how to restore it   (backup/README.md)
docs/           hardware reference, per-checkpoint guides     (docs/)
firmware/       ready-to-flash builds (+ SHA256)
include/        board_pins.h - every pin in one place
src/            the firmware source
tools/          backup / restore / flash / inspect scripts, PC simulator (tools/sim)
platformio.ini  build settings (PlatformIO)
BUILDLOG.md     checkpoint-by-checkpoint build diary
```

Coming later: `art/` (Jerry PNGs) and the PNG-to-firmware converter.

## Step 0: back up the original firmware

Do this before anything gets flashed. Full instructions are in
[backup/README.md](backup/README.md). Short version, on Windows in this folder:

```powershell
powershell -ExecutionPolicy Bypass -File tools\backup_firmware.ps1
```

To put the device back to stock at any time:

```powershell
powershell -ExecutionPolicy Bypass -File tools\restore_firmware.ps1
```

## Credits

Pin maps and hardware notes come from people who reverse-engineered the AIPI
Lite before us. Full details are in [docs/HARDWARE.md](docs/HARDWARE.md#sources-thank-you).

- delacerda-mrd: [Claude-Monitor-AiPi-Lite](https://github.com/delacerda-mrd/Claude-Monitor-AiPi-Lite) board reference
- DaveMath / @GGDM: [flock-you-go](https://github.com/DaveMath/flock-you-go)
- sticks918: [AIPI-Lite-ESPHome](https://github.com/sticks918/AIPI-Lite-ESPHome)
- bcarroll: [aipi-lite-minitv](https://github.com/bcarroll/aipi-lite-minitv)
- Robert Lipe's AIPI-Lite teardown
- Espressif: [esptool](https://github.com/espressif/esptool), [Arduino-ESP32](https://github.com/espressif/arduino-esp32)
- lovyan03: [LovyanGFX](https://github.com/lovyan03/LovyanGFX)
