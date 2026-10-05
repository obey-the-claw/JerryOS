# AIPI Lite: hardware reference for JerryOS

Status: **from community sources, not yet verified on Steve's unit.**
Every row gets a ✅ once we've seen it on the real device at Checkpoint 1
(chip facts) or Checkpoint 2 (pins).

Sources are listed at the bottom. Where they disagree, both values are shown
and the device will settle it.

## The chip

| Thing | Brief said | Community says | Verified on our unit |
|---|---|---|---|
| SoC | ESP32-S3 | ESP32-S3, module ESP32-S3-WROOM-1 **N16R8** [A][B] | ⏳ CP1 (`device-info-*.txt`) |
| Flash | 16 MB | 16 MB [A][B][C] | ⏳ CP1 |
| PSRAM | 8 MB | 8 MB **octal** PSRAM, embedded in the module [A][B] | ⏳ CP1 |
| USB | USB-C | Native USB (USB-Serial-JTAG), no separate USB chip. Shows up as a COM port with vendor ID `303A` [A] | ⏳ CP1 |

Plain-English note: "N16R8" literally means **N**16 = 16 MB flash, **R**8 = 8 MB PSRAM.
Octal PSRAM uses GPIO 33–37 internally, so those pins are off-limits.

## Display

| Thing | Value | Source |
|---|---|---|
| Panel | 1.44" TFT, 128×128 | [A][B][C] |
| Controller | ST7735-compatible (stock firmware drives it as ST7789, and that works too) | [A][B][C] |
| SPI SCLK | GPIO 16 | [A][B][C][D] |
| SPI MOSI | GPIO 17 | [A][B][C][D] |
| CS | GPIO 15 | [A][B][C][D] |
| DC | GPIO 7 | [A][B][C][D] |
| RST | GPIO 18 | [A][B][C][D] |
| Backlight | GPIO 3, PWM-dimmable | [A][B][C][D] |
| Orientation | swap XY + mirror X (MADCTL `0x68`), offsets 0,0 | [A][B] |
| Colour order | BGR | [A][B] |
| Colour inversion | **Conflict:** OFF per [A][B] (both verified on hardware); ON per [C] | settle at CP2 with a colour test card |

## Buttons

| Button | GPIO | Notes | Source |
|---|---|---|---|
| Right | 42 | Active-low (reads 0 when pressed) | [A][B][C][D] |
| Left | 1 | Active-low. **It's the stock power button** (tap = wake, hold = power off). | [A][B] |
| BOOT | 0 | Under the back cover. Only needed if USB flashing ever fails. We don't touch it. | [A][C] |

⚠️ The left button is wired into the power circuit. [C] saw 5–15 s hangs when
using it as a normal button. [A] and [B] use it fine. Since the brief wants
**LEFT long-press = home**, Checkpoint 2 must test: long-pressing LEFT on
USB power and on battery, and confirm it never cuts power.

## Power and battery

| Thing | GPIO / value | Source |
|---|---|---|
| Battery voltage sense | GPIO 2 (ADC1 ch1) through a voltage divider. Multiplier ≈ 2.5, needs calibrating against a multimeter | [A][B] |
| Power hold ("soft latch") | GPIO 10. Firmware **must drive it HIGH at boot**, or the device turns itself off on battery. Driving it LOW on battery = power off. | [A][B] |
| USB power present | GPIO 8: 1 = USB plugged in, 0 = on battery | [A] (verified on hardware) |
| Charging | GPIO 21: 0 = charging, 1 = idle/full. 4056-class Li-ion charger chip. | [A] (observed on hardware) |
| Cell | Single-cell LiPo, 3.3–4.2 V. Product page says 600 mAh. | [A], aipi.com |

**What "the battery module" actually is:** the board has its own charger
chip and a power latch, and the case is magnetic. The battery is a separate
single-cell LiPo that clips on. This doesn't need to stay a mystery: at
CP2 the hello-world screen will show the GPIO 8 / GPIO 21 / GPIO 2 readings
live, and we'll match them against the hardware as it plugs in and unplugs.

**"No battery attached":** with no cell, the charger output floats near
4.2 V, so a naive reading would show 100%. JerryOS will use GPIO 8 (USB
present) together with the voltage. On USB with no plausible battery voltage
it shows `USB` instead of a fake percentage.

## Audio (not used in v0 except maybe a beep)

| Thing | GPIO | Source |
|---|---|---|
| ES8311 codec I2C | SDA 5, SCL 4 (address 0x18) | [A][B][C] |
| I2S | MCLK 6, BCLK 14, WS 12, DOUT 11, DIN (mic) 13 | [A][B][C] |
| Speaker amp enable | GPIO 9, active-high | [A][B][C][D] |

## Other

| Thing | GPIO | Source |
|---|---|---|
| WS2812 RGB LED (one pixel) | 46 (a strapping pin, but fine as an output after boot) | [A][C] |

## Full GPIO map

```
 0 BOOT btn     1 LEFT btn     2 BATT ADC     3 BACKLIGHT
 4 I2C SCL      5 I2C SDA      6 I2S MCLK     7 LCD DC
 8 USB-present  9 SPK AMP EN  10 POWER HOLD  11 I2S DOUT
12 I2S WS      13 I2S DIN     14 I2S BCLK    15 LCD CS
16 LCD SCLK    17 LCD MOSI    18 LCD RST     19/20 USB D-/D+
21 CHARGING    33-37 PSRAM (never use)       42 RIGHT btn
46 RGB LED
```

## Sources (thank you!)

- **[A]** delacerda-mrd, *Claude-Monitor-AiPi-Lite*, `docs/BOARD_REFERENCE.md`
  (the most thorough reference: pins marked verified on hardware, with dates).
  https://github.com/delacerda-mrd/Claude-Monitor-AiPi-Lite
- **[B]** DaveMath (@GGDM), *flock-you-go*: passive 2.4 GHz detector on the AIPI Lite. Display settings validated on hardware.
  https://github.com/DaveMath/flock-you-go
- **[C]** sticks918, *AIPI-Lite-ESPHome*: the original ESPHome template and board trace-out.
  https://github.com/sticks918/AIPI-Lite-ESPHome
- **[D]** bcarroll, *aipi-lite-minitv*. https://github.com/bcarroll/aipi-lite-minitv
- Robert Lipe's teardown, *Let's study the AIPI-Lite AI Robot*
  (https://www.robertlipe.com/aipi-lite-ai-robot/). [A] cites it. I couldn't
  load it from my sandbox, and [A] notes two of its labels were later corrected.
