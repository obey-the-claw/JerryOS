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
- Decision (2026-10-05): the backup stays on Steve's laptop, **not** in
  GitHub. It can hold the stock firmware's Wi-Fi password, and kit users
  will make their own backup anyway. `.gitignore` now keeps `backup/` out.

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
- (Not yet checked on device: brightness surviving a power cycle, sleepy after 2 min.)

---

## Checkpoint 4: Tracker Watch

**Goal:** passively listen for item trackers and alert when one keeps being
seen over a configurable window (default 30 min), even as addresses rotate.

### What we did

- **Research first.** Rather than guess at byte formats, we used the source
  code of **AirGuard** (TU Darmstadt's open-source anti-stalking app) as the
  reference, plus Google's Find Hub spec. Full table + sources:
  `docs/TRACKERS.md`.
- **The key insight: "with owner" vs "separated".** Trackers broadcast
  differently when they're away from their owner's phone. Jerry only counts
  separated ones. Your passenger's keys and your own AirTag never set him off.
- **Detector** (`src/tracker_detect.cpp`): recognises AirTag, other Find My
  accessories/AirPods, Tile, Samsung SmartTag and Google Find Hub trackers
  from the raw broadcast bytes.
- **Persistence brain** (`src/tracker_watch.cpp`): per tracker *kind*, a
  64-minute strip of "seen this minute?". A streak (gaps ≤ 2 min forgiven)
  as long as the alert window = alert. Works even though addresses rotate.
- **Passive radio** (`src/ble_scanner.cpp`, NimBLE): listen-only scan. The
  radio's callback just drops tracker sightings into a queue; the main loop
  picks them up, so the two never fight over memory.
- **Screens:** Tracker Watch (status + one row per kind + "seen for"), a
  per-kind device list, an auto-open alert, a blinking red **!** on every
  screen, and shocked-Jerry on the home screen. Hold R on a red row = "ok".
- **Tested before flashing:** 41 unit checks (with memory-error checking on)
  and a simulator run of a full 30-minute "followed" scenario with
  screenshots.

### What broke / how we fixed it

- My first "find the oldest entry" line in the radio code was unreadable
  and probably wrong. Rewrote it plainly before it ever compiled.
- I'd used a cast to dodge a C++ ordering problem in the screens file.
  Replaced it with a proper declaration.

### On the real device (Steve fills this in)

- [x] Tracker Watch shows a "BLE nearby" count (18 in the office)
- [x] BLE Nearby lists devices (v0.4.1, 2026-10-05)
- [x] BLE Nearby: devices in the office show up, and the signal follows
  Steve as he moves around
- [ ] Own-AirTag and separated-AirTag alert tests: **not done on the device
  (no AirTag on hand).** The alert is covered by the unit tests and the
  30-minute simulator run. Retest when an AirTag is around.

**Checkpoint 4: ✅ done (2026-10-05).**

### v0.4.1: "18 nearby... but who?"

First flash: **BLE nearby: 18**, but every tracker row said 0. That's
correct (all 18 were phones, earbuds and other gear, and only *lost*
trackers count), but it raised two good questions.

- **Own count.** Each tracker row now shows a dim **+N** for trackers that
  are with their owner. They still never alert, but now you can see that
  Jerry noticed them.
- **BLE Nearby mode.** A new menu item that lists everything Jerry hears,
  closest first, with signal bars. Each device gets the best label its own
  broadcast gives away: a name ("JBL Flip 5"), a type ("AirPods/Beats",
  "Windows PC"), or just the maker. Hold R for details and a live signal
  meter, for a game of "hotter / colder".
- **Still passive.** Phones hide their names; getting a name would mean
  asking, and asking is transmitting. So lots of rows say "Apple device".
  That's privacy working as intended.
- **Calm list.** Signal is smoothed and the list re-sorts every 2 s. Same
  lesson as the battery number.
- Under the hood: the radio task and the screen now share one device table,
  behind a lock so they never trip over each other. Unit tests: 41 → 60.

**What broke:** in the simulator the top line and a hint were a few pixels
too wide and wrapped onto the next line, which looked like a stray "+". I
shortened both before building the firmware.

### v0.4.2: more Bigfoot

Steve's call: the code-drawn Jerry already has the right vibe, so we keep
him and skip swapping in PNG art for now. One change: **more Sasquatch in
the neck.** Bigfoot hardly has a neck, so the home-screen Jerry is now a
"portrait": his head sits low between big, hunched, shaggy shoulders, with
fur sloping down from the sides of his head, a thick ruff under his chin
and lighter chest fur. The small Jerrys next to text stay head-only so they
don't cover the words.

![Jerry v0.4.2](docs/img/jerry-moods.png)

---

## Checkpoint 5: Wi-Fi Scanner + Logger

**Change of plan:** Steve likes the code-drawn Jerry, so the old Checkpoint 5
(art spec + PNG converter) is shelved. The new Checkpoint 5 is the Wi-Fi
scanner and logger, which Steve wanted in this build.

**Goal:** hear the Wi-Fi networks around, show them, and keep a log that
can go to a PC. Still listen-only.

### What we did

- **Passive scan.** Jerry listens on channels 1-13 for router beacons
  (~120 ms each, every 20 s). He sends no probe requests and never joins
  anything. ESP32-S3 = 2.4 GHz only.
- **Its own background task.** Scanning and writing to flash happen off the
  screen's loop, so the UI never freezes. The network table is shared with
  the screens behind a lock, same pattern as Bluetooth.
- **Logger.** `/wifi.csv` on the chip's 3.4 MB storage area (LittleFS),
  one line per network per power-on, capped at 3 MB. A "session" number
  counts power-ons, so different trips can be told apart. No clock or GPS
  on board, so there's no date or location yet (later, from the Scanner v0).
- **Getting it off.** A tiny USB command line (`wifi dump`, `wifi clear`)
  and `tools/pull_wifi_log.py COM7 [--clear]`, which saves
  `logs/wifi-<date>.csv`. `logs/` is git-ignored.
- **Screens.** Menu → WiFi Scan (closest first, green "open" tag, hidden
  networks shown as "(hidden)"), a details page with a live signal,
  Settings → WiFi scan on/off (off = radio off, saves battery) and
  WiFi log (hold R twice to erase). The Settings list now scrolls.
- **Tested before flashing:** 76 unit checks, including network names with
  quotes, commas, newlines and emoji, which could otherwise break the CSV
  or the screen. The PC script was tested against a fake Jerry on a virtual
  serial port. Simulator screenshots of every new screen.

### What broke / how we fixed it

- In the simulator, "Ring Setup 4C" ran straight into the green "open" tag.
  Added a gap.
- `dumpLog()` first took a "Print" object, which the PC simulator doesn't
  have. It prints straight to USB now.

### On the real device (Steve fills this in)

- [x] WiFi Scan lists networks on the real device (v0.5.0, 2026-10-05)
- [ ] Signal changes as you walk around
- [ ] WiFi scan off/on in Settings works
- [ ] Log size survives a power cycle
- [ ] `py tools\pull_wifi_log.py COM7` saves a CSV that opens in Excel
- [ ] Erase works
- [ ] Tracker Watch and BLE Nearby still work alongside Wi-Fi

---

## Checkpoint 6: The personal monitor

**What Steve said JerryOS really is:** "personal on the spot monitoring...
saving when you want but not default w/o GPS to remember where... a great
handheld starting point for the pizero box project." This checkpoint makes
it feel like that.

### What we did

- **Home screen = dashboard.** Jerry plus three live pills along the
  bottom: tracker status (ok / ? / !), Bluetooth count, Wi-Fi count. Poke
  him and he chirps and says something in a speech bubble, sometimes with
  live numbers.
- **REC is opt-in.** Scanning shows networks live, but the log only saves
  while you're recording: hold R on home. REC always starts off at power-on.
  Each recording gets its own session number, and it saves what's in range
  straight away. Red REC pill, blinking red dot on every screen, and Jerry's
  eyes sweep while he records.
- **Sound.** First use of the speaker: ES8311 audio chip set up over I2C
  (sequence checked against Espressif's own driver), tones made on the
  chip in a background task, amp only on while playing. Sounds for boot,
  poke, REC on/off, tracker alert (repeats every minute until "ok"), low
  battery and erase. Settings → Sound is now off/low/med/high, with a
  preview tick.
- **Screen saver on battery:** dim after 1 min, off after 5 min. Any button
  (or a tracker alert) wakes it, and the waking press is swallowed so it
  doesn't also do something.
- **Diagnostics:** audio status + hold R = beep test. **About** says what
  Jerry is.

### What broke / how we fixed it

- Pasting in the new home screen, my edit cut the old one off at the wrong
  `};` (one inside a line of code). The simulator build caught it straight
  away.
- The ESP32's compiler (older than the PC one) rejected a shorthand for
  filling in the pills. Rewrote it the plain way.
- In the simulator, the REC pill sat at 0 because the fake radio only
  "scanned" when asked for a count. Simulator-only; fixed so the
  screenshots are honest.

### On the real device (Steve fills this in)

- [x] Boot sound works: "jerry says hello!" (2026-10-06). First time the speaker was used.
- [ ] Volume off/low/med/high; Diagnostics beep
- [x] Home pills, REC, screen saver: "rest is great"
- [ ] REC → walk → stop → pull log: new lines share one session number
- [ ] Screen dims at ~1 min and goes off at ~5 min on battery; a button wakes it
- [ ] Tracker Watch, BLE Nearby, WiFi Scan still fine

### v0.6.1: "just a beep sound, no squatch voice"

Fair point: Jerry beeped like a microwave. Now he sounds like a Sasquatch.

- **Squatch sounds from math** (`src/squatch_voice.cpp`): the classic Bigfoot
  whoop call (pitch glides up then down, with a throaty wobble), wood knocks
  (a hollow thump + click) and grunts (a buzzy low tone, roughened). The
  speaker is tiny, so everything carries lots of overtones, and the ear fills
  in the low notes.
- No microphone or recordings needed. The same code renders the sounds to
  `docs/sounds/*.wav` on a PC, so Steve can hear them before flashing.
  (I can't listen myself, so I checked them as spectrograms: smooth whoop
  arcs, buzzy grunt stacks, sharp knocks.)
- **Real voice lines, optional:** drop `.wav` files in `voice/` named after
  the moment (`rec_on.wav` = "Recording WiFi!"). `tools/make_voice.py`
  trims them, evens out the volume and builds them in. Several clips for one
  moment take turns. Tested with a 44.1 kHz stereo file and a badly named one.
- Unit tests 76 → 101 (every sound ends, is under 3 s, and is loud enough).

**What broke:** the memory checker flagged a "shift of a negative number"
in the clip player (works on the chip, but technically undefined in C++).
Changed it to a multiply.

- [x] Sound works on the device (boot hello heard 2026-10-06)

### v0.6.2: silent and cute

Steve's verdict on the Squatch sounds: "terrible... rather it be silent and
cute." Fair. All sound is gone: the synth, the voice-clip tools, the Sound
setting and the Diagnostics beep. The speaker amp is driven OFF at boot so
it can't hiss or waste battery. Jerry reacts on screen instead (grins,
speech bubbles, the shocked face, the blinking red !).

The lesson: a quick "does this sound good?" listen before building a whole
voice system would have saved a round trip. The working ES8311 driver is
kept in git history (commit 2b89171) if real voice lines are ever wanted.

**Checkpoint 6: ✅ done.**

---

## Checkpoint 7: Followers

Steve asked for everything else the AIPI can do to protect him as an on-me
device. Plan, in order: (1) followers, (3) attack detector, (5) colour LED,
(2) Flock cameras, (4) hidden cameras, (6) pocket mode, (7) sound meter.
One checkpoint each, tested on the device in between.

### What we did

- **"Have I moved?" without GPS** (`SceneTracker`): every 20 s, compare the
  Wi-Fi networks heard with the ones where we "are". Sharing less than 25%,
  twice in a row = a new place.
- **"What's still with me?"** (`FollowWatch`): any Bluetooth device seen at
  4+ places over 20+ minutes, never gone more than 5 min, is a follower.
  20 minutes is longer than phones' ~15-minute address changes, so
  passengers' phones don't count. Trackers are left to Tracker Watch.
- **"Mine":** hold R on a follower to trust it forever (saved, up to 16).
  Settings → My devices shows/clears them.
- **Screens:** Followers list + details; it opens by itself on a new
  follower. Home's first pill is now overall safety: SAFE / WATCH / ALERT.
- **Simulator** got a fake drive (Wi-Fi changes every 2 min from minute 8)
  with a speaker and a mystery gadget riding along.
- Unit tests 76 → 91: home for 2 hours (nothing), a 40-min drive (the
  stowaway caught after 20 min; rotating phone and a drive-by ignored),
  "mine" sticks, a long gap restarts the count, trackers excluded, trusted
  list save/load and cap.

### What broke / how we fixed it

- I counted followers by asking the list function for "zero entries",
  which always answers zero, so the home pill would never have said WATCH
  for followers. Caught it reading my own code, and added a proper count.

### On the real device (Steve fills this in)

- [x] Followers at home: "nobody following" (2026-10-07)
- [ ] Drive 30+ min: "moved" goes up; own static gear shows up; "mine" works
- [ ] My devices count survives a power cycle

---

## Checkpoint 8: Shield (attack detector)

### What we did

- **Wi-Fi kick-offs:** after each 20-s scan, ~6.5 s of listening to raw
  Wi-Fi management frames ("promiscuous mode", still receive-only),
  hopping channels 1-13 every 0.5 s. Deauth/disassoc frames are counted;
  30+ in 10 s = flood.
- **Bluetooth pop-up spam:** the radio task flags pop-up style broadcasts
  (Apple proximity pairing / nearby action, Google Fast Pair, Microsoft
  Swift Pair, Samsung easy-setup) and queues them to the main loop. 12+
  *different* addresses in 10 s = spam. One AirPods case all day never
  counts.
- **Fake networks:** on every scan, an open network with the same name as a
  locked one (evil twin), or 8+ names from one box (Pineapple-style).
- **Shield screen** with the three rows; opens itself on a new threat; home
  pill and the red **!** cover it.
- Unit tests 91 → 107, including the "must stay quiet" cases (own AirPods,
  occasional disconnects, a mesh network, an office access point with 5
  names, xfinitywifi next to XFINITY).

### What broke / how we fixed it

- The simulator flagged "1 box, 12 names" because I'd given all the fake
  neighbours almost the same hardware address. Fake data, not a firmware
  bug, but it showed the rule works. Fixed the fake data.
- The details line and "18m ago" ran into other text on the little screen:
  shortened both. A compiler warning about a cut-off message: fixed.

### On the real device (Steve fills this in)

- [x] Shield says "all quiet" on the device (2026-10-07)
- [ ] BLE Nearby / Tracker Watch counts look the same as before
- [ ] (optional) Flipper pop-up spam → ATTACK NEARBY
- [ ] (optional) open phone hotspot named like the home Wi-Fi → Fake networks

---

## Checkpoint 9: the colour LED

### What we did

- First use of the board's WS2812 colour LED (GPIO 46), through the
  built-in `neopixelWrite`. It's only updated when the colour changes.
- One colour language for everything: **red fast blink** = unseen alert,
  **amber pulse** = watching, **blue blip** = REC, **soft green** = all clear.
  All dim (max 60/255).
- Settings → LED: all / alerts / off (default all).
- The pattern logic is pure and tested (8 checks: off is off, alert is red
  and blinks, green only in "all", amber has no blue, REC blip, max
  brightness). The simulator draws the LED as a dot under each screenshot.

### What broke / how we fixed it

- My first "LED setting" screenshot was taken one row too early. Script fix
  only.

### On the real device (Steve fills this in)

- [x] LED breathing green on the device: colours right (2026-10-07)
- [ ] Blue blip while REC; alerts / off modes work
- [ ] Red blink on an alert, amber after looking

---

## Checkpoint 10: Flock cameras

### What we did

- Read the flock-you source (colonelpanichacks, MIT) and flock-you-go rather
  than guessing. Kept only high-confidence signatures: Flock's own Wi-Fi
  address prefix, "Flock-XXXXXX" hotspots, "Penguin-" + 10 digits and
  "FS Ext Battery" battery names, the Flock Bluetooth service ID. "Maybe":
  the battery chip maker's ID and the Raven service range.
- Skipped the wider community address list on purpose: it includes generic
  chip makers (Espressif, etc.) that would flag ordinary smart plugs, and
  Jerry himself.
- Three ways in: Bluetooth broadcasts (radio task → queue), Wi-Fi scan
  results (every 20 s), and Flock-addressed frames heard during the
  kick-off listening window.
- New **toast**: a short message over any screen ("Flock camera nearby!")
  that also wakes the screen. Cameras screen, LED amber and home WATCH while
  one is near. Credits added to About and README.
- Unit tests 115 → 139 (incl. near-misses: "Penguin Speaker", 9 or 11 digits,
  a bare phone number, "Flockton Library", one byte off the service ID).

### What broke / how we fixed it

- **flash_prebuilt.ps1 sorted firmware files by name**, so `cp10` would have
  sorted *before* `cp9` and Steve would have flashed the old version. Now it
  sorts by checkpoint number. Checked with PowerShell against the real folder.

### On the real device (Steve fills this in)

- [x] Cameras: "no Flock cameras" at home (2026-10-07)
- [ ] Drive past a known Flock camera (deflock.me): pop-up + listed

---

## Checkpoint 11: Hidden-camera hints

### What we did

- **Camera-maker addresses from the official registry:**
  `tools/make_camera_ouis.py` reads the IEEE MA-L list and keeps only
  companies that mostly make cameras, matched on their exact registered
  names (so "Carlo Gavazzi" can't pass for "Arlo"). That gives 157
  prefixes in `src/camera_ouis.h`, sorted for a fast binary search.
- **Spy / IP camera hotspot names:** HDWiFiCam, V380 ("MV" + digits),
  IPCAM, Yoosee GW_AP, Lookcam, "...camera...", Ring Setup, Tapo_Cam...
- **Streaming cameras:** the listening window now also hears Wi-Fi data
  frames, so a camera uploading video to the house Wi-Fi gets caught by
  its sender address. Each device is reported at most every 2 s, so a busy
  camera can't flood the queue.
- Hints share the Cameras screen ("Wyze cam?", "HDWiFiCam?"), stay quiet
  (no pop-up, no LED), and Flock still wins if both match.
- Unit tests 139 → 176: every 7th table entry found, table sorted, brands,
  hotspot names and near-misses ("Campus WiFi", "MVP Sports", "MV12a45678").

### What broke / how we fixed it

- A test compared brand names by memory address. The test file and the
  firmware each keep their own copy of the text, so it failed. Firmware was
  fine; the test now compares the words.
- The simulator's fake neighbours accidentally used a real Ring prefix.
  Moved them to "locally administered" addresses that can't belong to any
  company.
- The top line wrapped, and labels read "HDWiFiCam cam?". Shortened the line
  and made the "cam" check ignore upper/lower case.

### On the real device (Steve fills this in)

- [ ] Own camera / doorbell shows as "<brand> cam?" within a minute
- [ ] Bars grow when walking closer

---

## Checkpoint 12: Pocket mode + sound meter

Steve: "keep going and build everything". He'll test at night from the home
office and take Jerry out the next day. So checkpoints 11 and 12 ship
together with one test plan: `docs/TEST-PLAN.md`.

### What we did

- **Pocket mode:** both buttons held 1 s toggles a lock. While both are
  down, single-button events are swallowed, so L-long (home) and R-long
  don't fire by accident. Locked: presses only peek (4 s). Settings →
  Pocket `dark` keeps the screen off even for alerts (LED only); `lit`
  doesn't. Padlock icon; the hint bar says "locked: hold L+R".
- **Sound meter:** the ES8311's microphone side (ADC) over I2S RX, using the
  clock setup already proven on this board in v0.6. It runs in its own task
  and only while the Sound screen keeps asking (it turns itself off after
  3 s). Level maths is pure and tested: DC removed, RMS → dBFS, a meter that
  rises fast and falls slowly, a peak hold. Shown as rough dB (an uncalibrated
  +110 offset) with everyday words.
- Simulator: backlight now dims the screenshots (so "dark" really is black),
  a pocket-mode demo, and a fake talking room for the meter.
- Unit tests 176 → 187.

### What broke / how we fixed it

- In pocket mode the bottom bar still offered "menu / poke / rec", which
  is wrong while locked. It now says how to unlock.

### On the real device: see docs/TEST-PLAN.md

- [x] Pocket mode works on the device (2026-10-07)
- [x] Sound meter works: shows "talking" when talking (first use of the microphone)
- [ ] Sound calibration vs. a phone app (quiet / talking): waiting on Steve's numbers

---

## Checkpoint 13: v1.0

Steve: "lets build anything else we had outstanding on this project now so
we're at a completed stage". The one feature still saying "coming soon" was
the Rig Dashboard, so it became real, along with the finishing pieces.

### What we did

- **Rig link, listen-only:** the Pi Zero box broadcasts a 29-byte Bluetooth
  beacon (company ID 0xFFFF, "JR" + GPS + counts). Jerry decodes it in the
  radio task and hands it to the main loop. Pairing remembers one box (saved),
  and other boxes' beacons are ignored.
- **`tools/pi/jerry_beacon.py`:** stdlib + BlueZ D-Bus (preinstalled on
  Raspberry Pi OS), GPS from gpsd, optional `/run/jerry_status.json` for
  the box's counts, `--fake-gps` and `--print` for testing, a systemd snippet.
- **GPS-tagged REC:** log lines get `lat,lon` (7 decimals, written with
  integers so nothing gets rounded) while the paired box has a fix under 10 s
  old. Old logs are converted once on boot.
- **Rig Dashboard screen:** found → pair → connected (fix, lat/lon, mph, box
  counts, battery, REC+GPS badge) / not heard lately.
- **Settings → Reset all** (asks first) and **docs/USER-GUIDE.md**.
- Unit tests 187 → 203, including the **exact bytes the Python script
  prints** decoding correctly on Jerry, so both ends are checked against
  each other.

### What broke / how we fixed it

- **Log upgrade loop (caught before shipping):** the log's lines end in
  "\r\n", so the header read back with a hidden "\r" and would never match.
  Every boot would have added two more empty columns. It's trimmed now.
- **Hand-typed bytes:** I first typed the "Python" beacon bytes into the test
  from memory and got them wrong. I re-ran the script for the real bytes,
  then miscounted the zeros pasting them in. The test caught both times.
- **Edit landed in the wrong place twice:** the new Rig screen left the old
  one behind (an edit matched the banner, not the section). And the line that
  hands rig beacons to the main loop landed inside the radio callback (two
  spots share a line). The simulator and the real compiler caught them.
- The box address showed backwards in the simulator (fake data written in the
  wrong order), "GPS fix 11 sats" ran into the signal bars, and the reset
  prompt said "erase". All fixed.

### On the real device: see docs/TEST-PLAN.md (v1.0 section)

- [x] v1.0 flashed and boots: About says v1.0.0 (2026-10-08)

### v1.0.1: sound meter reading about double

Steve: "sound db way off.. was almost double". The +110 offset was a guess
(the AIPI's microphone has no published calibration), and it was too high.

- Default offset lowered to +80 (about 30 dB less).
- **Settings → Sound cal:** -30 to +30 dB in 3 dB steps, applied live
  while editing, saved. Steve can match a phone app himself; no round trip.
- The Sound screen shows **raw** (the mic's level before calibration), so
  a photo next to a phone app is enough for me to fix the curve if quiet and
  loud can't both match with one offset.
- Unit test: calibration shifts the reading and never goes below 0. (I
  first added a "test" that only checked a copy of the maths, not the real
  code. Removed it.)

- [ ] Sound cal set against a phone app (quiet + talking both match?)

- [x] v1.0.1 flashed: About says v1.0.1 (2026-10-08)

---

## v1.1: the boot intro

Steve: "a 1 second or 2 second Jerry walking out of the woods and into a
jeep or something like that would be crazy cool", and "JERRY top left should
be JerryOS instead from now on".

### What we did

- **Intro (`src/intro.cpp`), about 2.6 s, drawn in code:** a sunrise sky, hills,
  pine woods and a dirt road. Little full-body Jerry (shaggy, arms and legs
  swinging, bushy brows) walks out from behind a tree to an olive open-top
  Jeep (roll bar, spare tyre, grille, headlight, fender flares, spoked
  wheels), hops into the driver's seat in an arc, and the Jeep speeds off in
  a dust cloud while "JerryOS" drops in with the version. Any button skips it.
- Screens can now be **full screen** (no status / hint bars) for things like this.
- Home's top-left tag now reads **JerryOS**.
- The simulator records the intro frame by frame into `docs/img/intro.gif`.

### What broke / how we fixed it

- First render: the title showed from the very start and the first frames
  jumped ahead. "Time minus 1750 ms" on an *unsigned* number wraps around to
  a huge value instead of going negative. Switched the intro clock to a
  signed number. The frame sheet caught it before it reached the device.

### On the real device

- [ ] Intro plays at power-on (smooth? too fast / slow?), any button skips it
- [ ] Home says JerryOS top-left

### v1.1.1: Steve's notes on the intro

1. "slow it down": the story now plays at 60 % speed (~4.3 s).
2. "add press to start": it ends on the title with a blinking **press any
   button**. While it waits, normal dimming applies, and Jerry keeps
   watching in the background.
3. "back tire of jeep looks like a wheel in the air": the spare is now a
   side-on, knobby black tyre on the tailgate. The road wheels got knobby
   tread lugs that turn as the Jeep drives off.

### v1.1.2: still road, and a funk groove to preview

- "road doesn't need to be moving": the road markings stay put now.
- "squatchy bootsy collins style intro music": after the "terrible" Squatch
  sounds in v0.6, this time **Steve hears it before it goes on the
  device**. `src/funk.cpp` writes the groove in code: a sawtooth "Space Bass"
  through an envelope filter (the Bootsy "bwow"), slap octave pops, ghost
  notes, a slide down at the end, a funk kick / snare / hat pattern, and an E9
  stab when "JerryOS" drops in. Two bars at 110 BPM (~5 s). It's rendered to
  `docs/sounds/intro_funk.wav` with `tools/sim/render_funk.sh` (the exact
  samples the device would play). I checked it as a spectrogram: the filter
  sweeps, pops, drum hits and the end slide are all there, and it doesn't
  clip. Unit-tested too (length, ends, never clips). **It's not wired to the
  speaker yet:** that waits for Steve's verdict. The ES8311 speaker driver
  is in git history (2b89171).

---

## v1.2: the groove goes on the device

Steve heard the preview: "ahh yeah that music was firrrrrrrrreeeeeeeeee put
it on the device i'm not gonna flash until it's jamming!" Previewing first
paid off this time.

### What we did

- **Speaker back, for the groove only:** `src/speaker.cpp` uses the ES8311
  DAC route that played "hello" in v0.6. It runs in its own task while the
  intro plays, then mutes, powers down the chip's analog side, switches the
  amplifier off and releases I2S. No hiss, no battery drain the rest of the
  time.
- **Shared audio-chip code:** `src/es8311.cpp` now holds the I2C helpers and
  the clock / format set-up used by both the microphone and the speaker,
  instead of two copies. The microphone waits while the groove is playing
  (they share the chip and the I2S port).
- **Intro:** starts the groove once per power-on. Skipping (any button) cuts
  it, and coming back from an alert doesn't restart the intro.
- **Settings → Intro tune:** on / off.

### On the real device

- [ ] The groove plays with the intro, in time, not distorted, loud enough
- [ ] A button press cuts it; Intro tune off = silent boot
- [ ] Sound meter still works afterwards
