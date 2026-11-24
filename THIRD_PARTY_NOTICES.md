# Third-party notices

JerryOS's own code is under the PolyForm Noncommercial License (see
[LICENSE.md](LICENSE.md)). The firmware is built with, and the pre-built
`.bin` files contain, these open-source projects, each under its own license:

| Project | Used for | License |
|---|---|---|
| [Arduino-ESP32](https://github.com/espressif/arduino-esp32) 2.0.17 | the Arduino core for the ESP32-S3 | LGPL-2.1 |
| [ESP-IDF](https://github.com/espressif/esp-idf) 4.4 (inside Arduino-ESP32) | the chip's system software | Apache-2.0 |
| [LovyanGFX](https://github.com/lovyan03/LovyanGFX) 1.2.7 | the display driver and fonts | FreeBSD (BSD-2-Clause), fonts under their own licenses (see its license.txt) |
| [NimBLE-Arduino](https://github.com/h2zero/NimBLE-Arduino) 1.4.3 | Bluetooth LE (listen-only) | Apache-2.0 |
| [esptool](https://github.com/espressif/esptool) | flashing (a tool, not in the firmware) | GPL-2.0 |
| [ESP Web Tools](https://github.com/esphome/esp-web-tools) 10.4.0 | the web installer's Install button (loaded by the page, not in the firmware) | Apache-2.0 |
| [esptool-js](https://github.com/espressif/esptool-js) 0.6.1 | the web installer's backup / restore (loaded by the page) | Apache-2.0 |

**LGPL note (Arduino-ESP32):** you can rebuild JerryOS against your own copy
of Arduino-ESP32: the full source and the exact build recipe are in this repo
(`tools/build_firmware.sh`).

## Data and research we built on

Credited with thanks. These are facts (device signatures, published
identifiers, pin maps), re-implemented in our own code, not copied code.

- **Tracker signatures:** [AirGuard](https://github.com/seemoo-lab/AirGuard),
  SEEMOO lab, TU Darmstadt (Apache-2.0).
- **Flock camera signatures:**
  [flock-you](https://github.com/colonelpanichacks/flock-you) by
  colonelpanichacks (MIT, Copyright (c) 2026 colonelpanichacks),
  [flock-you-go](https://github.com/DaveMath/flock-you-go) by DaveMath / @GGDM,
  and @NitekryDPaul.
- **Camera-maker Wi-Fi prefixes:** the IEEE public OUI registry
  (`tools/make_camera_ouis.py`).
- **AIPI Lite hardware research:** delacerda-mrd, DaveMath (@GGDM),
  sticks918, bcarroll and Robert Lipe. Details in
  [docs/HARDWARE.md](docs/HARDWARE.md#sources-thank-you).
