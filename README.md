# JerryOS

Custom firmware for the **AIPI Lite** (ESP32-S3) that turns it into Jerry the
Squatch's pocket/dash sidekick, "Chief Gear Inspector" for the
[5quatch Rated](https://5quatchrated.com) rig.

It's a **personal, on-the-spot monitor**: it warns you about trackers
following you and shows the Bluetooth and Wi-Fi around you. It only
listens (never transmits) and only saves when you press REC. It's also the
handheld starting point for the Pi Zero box (Scanner v0).

Jerry's animated head lives on the screen. The v0 modes are:

- **Tracker Watch:** passive BLE scan for AirTag / Tile / SmartTag-style
  trackers that keep following you.
- **Followers:** any Bluetooth device that stays with you while you move
  (car, watch, hidden gadget), using Wi-Fi changes as a "you moved" signal.
- **Shield:** listen-only attack detector: Wi-Fi "deauth" floods, Flipper-style
  Bluetooth pop-up spam, evil-twin and many-names fake networks.
- **Cameras:** spots Flock licence-plate cameras, their battery packs and
  Raven sensors as you pass (signatures from flock-you / flock-you-go), and
  lists hidden-camera hints (camera-brand Wi-Fi, spy-cam hotspots).
- **Colour LED:** red blink = look at Jerry, amber = watching, blue blip =
  recording, soft green = all clear. Works with the screen off.
- **BLE Nearby:** every Bluetooth device in range, closest first.
- **WiFi Scan:** listen-only Wi-Fi scanner. Hold R on the home screen to
  record networks to a log (copy it to your PC with `tools/pull_wifi_log.py`).
- **Home screen:** Jerry plus live counts (trackers, Bluetooth, Wi-Fi),
  speech bubbles, and a battery-saving screen timeout. Silent by design.
- **Rig Dashboard:** placeholder for the Scanner v0 Pi Zero link.
- **Settings.**

Two buttons, a hint bar that always tells you what they do, and the device
stays physically stock (no soldering).

## Status

| # | Checkpoint | State |
|---|---|---|
| 1 | Firmware backup confirmed | ✅ 2026-10-04 |
| 2 | Hello world: screen, buttons, battery, serial log | ✅ 2026-10-04 |
| 3 | JerryOS shell: home, menu, hint bar, modes (placeholder faces) | ✅ 2026-10-05 |
| 4 | Tracker Watch + BLE Nearby | ✅ 2026-10-05 ([guide](docs/CHECKPOINT4.md), [how it works](docs/TRACKERS.md)) |
| 5 | Wi-Fi scanner + logger | ✅ 2026-10-05 ([guide](docs/CHECKPOINT5.md)) |
| 6 | Personal monitor: home screen, REC, screen saver | ✅ 2026-10-06 ([guide](docs/CHECKPOINT6.md)) |
| 7 | Followers: any Bluetooth device that moves with you | 🟡 home test ✅, drive test pending ([guide](docs/CHECKPOINT7.md)) |
| 8 | Shield: Wi-Fi kick-offs, Bluetooth pop-up spam, fake networks | ✅ quiet-day test 2026-10-07 ([guide](docs/CHECKPOINT8.md)) |
| 9 | Colour LED as a silent signal | ✅ 2026-10-07 ([guide](docs/CHECKPOINT9.md)) |
| 10 | Flock licence-plate cameras | 🟡 home ✅, drive-by pending ([guide](docs/CHECKPOINT10.md)) |
| 11 | Hidden-camera hints (hotel / rental check) | 🟡 ready to test ([guide](docs/CHECKPOINT11.md)) |

See [BUILDLOG.md](BUILDLOG.md) for the story so far.

![JerryOS](docs/img/cp6-monitor.png)

## Repo layout

```
backup/         where YOUR firmware backup goes (not in git)  (backup/README.md)
docs/           hardware reference, per-checkpoint guides     (docs/)
firmware/       ready-to-flash builds (+ SHA256)
include/        board_pins.h - every pin in one place
src/            the firmware source
tools/          backup / restore / flash / inspect / pull-log scripts, PC simulator (tools/sim)
logs/           Wi-Fi logs pulled off the device (not in git)
platformio.ini  build settings (PlatformIO)
BUILDLOG.md     checkpoint-by-checkpoint build diary
```


## Step 0: back up the original firmware

Do this before anything gets flashed. The backup stays on your own PC (it's
never put in git). Full instructions are in
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
- h2zero: [NimBLE-Arduino](https://github.com/h2zero/NimBLE-Arduino)
- SEEMOO lab, TU Darmstadt: [AirGuard](https://github.com/seemoo-lab/AirGuard) (tracker signatures)
- colonelpanichacks: [flock-you](https://github.com/colonelpanichacks/flock-you) (MIT), DaveMath:
  flock-you-go, @NitekryDPaul (Flock camera signatures)
