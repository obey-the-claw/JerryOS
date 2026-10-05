# JerryOS build log

Short notes for each checkpoint: what we did, what broke, how we fixed it.
This is raw material for the blog and video.

---

## Checkpoint 1: back up the original firmware

**Goal:** before flashing anything, save a perfect copy of what's on the
AIPI Lite so we can always put it back.

### What we did

- **Researched the hardware instead of trusting the box.** Found four
  community projects that had already mapped the AIPI Lite's pins. Wrote
  them up in `docs/HARDWARE.md` with credit, and flagged where they disagree.
  - Brief's guess (ESP32-S3 N16R8, 16 MB flash, 8 MB PSRAM, ST7735 128×128):
    **matches the community sources.** The device still has to confirm it
    itself (that's part of this checkpoint).
  - Surprises worth knowing before we write code:
    - The **left button is the power button.** It's wired into the power
      latch. We have to make sure "hold LEFT = go home" never turns Jerry off.
    - **GPIO 10 is a "keep me alive" pin.** On battery, the firmware must hold
      it high or the device switches itself off.
    - **Sources disagree on screen colour inversion.** We'll settle it with a
      test card at Checkpoint 2.
- **Wrote `tools/backup_firmware.ps1`.** One command on Windows. It:
  1. installs esptool (Espressif's official tool),
  2. finds the device's USB port on its own,
  3. asks the chip what it is (model, flash, PSRAM, MAC, security locks),
  4. reads all 16 MB of flash **twice** and checks both reads match,
  5. makes the chip checksum its own flash and compares that to the file,
  6. checks the file really contains firmware (`tools/inspect_backup.py`).
- **Wrote `tools/restore_firmware.ps1`.** The undo button. It checks the
  backup's fingerprint and makes you type `RESTORE` before writing.
- **Tested both scripts in the sandbox** against a fake device: good dump,
  blank dump, truncated dump, corrupted partition table, Secure Boot on, and
  a tampered backup file. Every bad case stops with a clear message.

### Plain-English: why so paranoid about a backup?

A flash read can "succeed" and still hand you junk: a loose cable, a chip
that's encrypted, a read that silently stopped early. If we only found out
after flashing JerryOS over the original, the stock firmware would be gone
for good. So we don't take "it finished" as proof. We read twice, have the
chip check itself, and open the file to look for a real bootloader and
partition table inside.

### What broke / how we fixed it

- `inspect_backup.py` crashed on a truncated dump (it tried to look past the
  end of the file). Caught in testing. Now it reports "outside the dump" and
  fails cleanly.

### On the real device (2026-10-04)

**Result: BACKUP CONFIRMED.** ✅

- First try, the script couldn't find the device: Windows only listed two
  Bluetooth COM ports. **Fix: switch the device fully off, take the battery
  off, then plug USB back in.** After that Windows saw it as **COM7**
  (`USB\VID_303A&PID_1001`, Espressif's built-in USB), and `-Port COM7`
  worked. *(Why, probably: the stock firmware had the chip in a state where
  its USB wasn't answering. A full power cut, battery included, resets
  everything. The battery keeps the chip alive even when it "looks" off.)*
- What the chip says it is: **ESP32-S3 rev v0.2, 8 MB embedded PSRAM, 16 MB
  flash.** It matches the brief exactly.
- Security: **Secure Boot off, flash encryption off.** The door is open for
  custom firmware.
- Two full 16 MB reads, about 2.5 min each, came out byte-identical. The
  chip's own checksum matched the file too.
- SHA256 of the backup:
  `47eff76e932958084df49ce7550f213c894aa26901927698cc19251c52680305`
- What's inside: the **XiaoZhi** AI chatbot firmware, with *two* versions
  side by side (v1.2.5 and v1.2.7). That's an A/B update setup: the device
  downloads updates into the spare slot and flips over. There's also a 960 KB
  "model" partition for the "hey, wake up" voice model.
- 61% of the chip is empty space. Lots of room for Jerry.
- [ ] Copy the `.bin` off the laptop (USB stick / cloud).

---

## Checkpoint 2: Hello World (hardware test)

**Goal:** prove the screen, both buttons, the battery reading and the serial
log all work before building JerryOS on top.

### What we did

- **Set up the project:** PlatformIO + Arduino framework, with LovyanGFX for
  the screen. `platformio.ini` pins exact versions so builds are repeatable.
- **Picked LovyanGFX over TFT_eSPI.** Every panel quirk (colour order,
  inversion, offsets, rotation) is a plain setting in *our* code
  (`src/display.h`). TFT_eSPI wants you to edit files inside the library,
  which is easy to lose.
- **All pins live in one file**, `include/board_pins.h`. If one is wrong,
  that's the only place to fix it.
- **Wrote the button handler JerryOS will use**, `src/buttons.h`. It filters
  out contact bounce and turns presses into "short" and "long" events.
- **Power:** the very first line of code holds GPIO10 high, so the device
  doesn't switch itself off on battery. `src/power.h` reads the battery,
  USB and charging state, and guesses "no battery" when the reading makes no
  sense for a LiPo.
- **The test card:** colour swatches with letters, a 1-pixel frame around
  the edge, and different-coloured corner markers. One glance shows whether
  the colours, orientation and screen offsets are right.
- **Compile-checked before shipping.** My sandbox can't reach PlatformIO's
  servers, so I built it with the same Arduino-ESP32 2.0.17 core from GitHub.
  That caught two real compile errors before they ever reached Steve's laptop.
- **Made a ready-to-flash `.bin`** plus `tools\flash_prebuilt.ps1`. That
  means the first flash uses the same esptool from Checkpoint 1, with
  nothing new to install yet.

### What broke / how we fixed it

- My first version used a C++ trick (a "variadic generic lambda") that this
  compiler version doesn't support. I swapped it for a plain helper function.
- I referenced a reset-reason name (`ESP_RST_USB`) that doesn't exist in this
  ESP-IDF version. Removed it.
- **The big one: first flash boot-looped.** The flash "succeeded", but the
  screen never changed and the COM port kept vanishing. The serial log
  showed the chip's built-in boot ROM giving up and resetting over and over:
  ```
  rst:0x7 (TG0WDT_SYS_RST),boot:0xa (SPI_FAST_FLASH_BOOT)
  mode:QIO, clock div:1
  load:0x3fce3808,len:0x4bc
  ets_loader.c 78
  ```
  **Cause:** when I packed the firmware into one file, I told the packer to
  stamp the bootloader as **QIO** (4-wire, the fastest flash-reading mode).
  The compiler had *correctly* stamped it **DIO** (2-wire). This board's flash
  chip won't boot in QIO, so the boot ROM couldn't even load the bootloader,
  and the watchdog reset it, forever.
  **Fix:** rebuilt the file keeping the compiler's DIO setting, and set
  `board_build.flash_mode = dio` in `platformio.ini` so self-builds can't
  repeat it. *(Plain English: the chip has a tiny permanent program, the boot
  ROM, that reads the first few kilobytes of memory. We told it to read those
  kilobytes in a mode the memory chip doesn't support at power-up.)*
  **Why it wasn't really bricked:** the boot ROM lives inside the chip and
  can't be overwritten, so esptool can always reach it and re-flash.

### On the real device (2026-10-04)

- ✅ After the DIO fix: **the test card appeared.** First JerryOS pixels!
- ✅ Orientation correct, all 4 edges visible, inversion OFF is right.
- ❌→✅ **Red and blue were swapped.** The "R" box was blue, gold looked
  light blue, and brown looked navy. The community setting said "BGR"; this
  panel actually wants RGB in LovyanGFX's terms. Changed one setting
  (`rgb_order = false`). *Lesson: "verified" community values still need
  checking on your own unit.*
- ✅ Re-flashed: colours now match the letters, title bar is brown.
- ✅ Buttons: short and long presses counted correctly on both.
- ✅ Holding LEFT 5+ s on USB: stays on.
- ✅ Unplugged USB: stays on battery, reads 4.15 V / 95%.
- ✅ Holding LEFT 5+ s **on battery**: stays on. Our firmware owns the left
  button now. The stock "hold to power off" was a software feature, not
  wiring, so **"hold LEFT = home" is safe.**

**Checkpoint 2 complete.**

---

## Checkpoint 3: the JerryOS shell

**Goal:** home screen with Jerry, menu, hint bar, status bar, mode switching.
Placeholder faces drawn in code.

### What we did

- **Jerry, drawn in code** (`src/jerry_face.cpp`): shaggy fur clumps, big
  googly eyes (resting look: down-left), mismatched bushy brows, triangle
  nose, huge orange grin with white zigzag teeth. Five moods: **idle,
  scanning, alert, approved, sleepy**. Blinking (sometimes a double blink)
  and glancing are animation layered on top, so every mood blinks.
- **Jerry reacts:** grins when poked or when USB is plugged in, gets sleepy
  on low battery or after 2 minutes of no buttons, and looks around while
  "scanning" in Tracker / Rig.
- **The shell** (`src/shell.cpp`): a stack of screens, the status bar (mode
  name + battery) and the hint bar. LEFT-hold = home lives here, so no screen
  can forget it.
- **Every mode is one class** (`src/screens.cpp`). The menu is just a list
  of them. Adding the real Rig Dashboard later = write one class, swap it in.
- **Settings saved to flash:** brightness (live preview), tracker alert
  window (default 30 min), sound on/off. Plus Diagnostics and About/credits.
- **A simulator** (`tools/sim/`): runs the *real* JerryOS code on a PC with
  fake button presses and saves screenshots. Every screen got checked before
  it reached the device. Those screenshots are in `docs/img/`.

### What broke / how we fixed it

- **Jerry v1 looked like a sea urchin.** Even spikes all round a circle read
  as a hedgehog, not a squatch. The simulator made this obvious *before*
  flashing. Fix: uneven fur clumps that **droop downward**, a darker
  under-layer for depth, messy bangs, and a slightly taller head.
- A colour named `OK` clashed with a name already used inside Espressif's
  code (compile error). Renamed it `GOOD`.
- My flash script edit used a helper before defining it (PowerShell needs
  definitions first). Caught by testing the script, fixed before pushing.

### On the real device (Steve fills this in)

- ✅ Jerry appears on the real screen and blinks. First time Jerry is alive!
- ✅ Poke works, menu works, brightness works.
- ❌→✅ **Battery % jumped around in real time.** It was recalculated from a
  single voltage reading every second, and LiPo voltage naturally wobbles
  (the screen and chip draw power in bursts). **Fix:** `BatteryGauge` in
  `src/power.h` does what phones do: a slow running average (~15 s), the
  number only moves for real changes, it never ticks *up* on battery, and it
  resets cleanly when USB is plugged or unplugged. Tested with 15 simulated
  minutes of noisy readings: the raw % changed **818** times, the gauge
  **15** times, with zero upward ticks on battery.
- [ ] Brightness change survives a power cycle
- [ ] Sleepy after 2 minutes idle
