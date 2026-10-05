# Checkpoint 2: Hello World (hardware test)

The first JerryOS code on the device. It isn't Jerry yet. It's a test screen
that proves the screen, both buttons, the battery reading and the serial log
all work. Everything we build next stands on this.

Your original firmware is safe in `backup\`. `tools\restore_firmware.ps1`
puts it back any time.

🎬 = a good moment to film.

---

## Step 1: get the new files

Re-download the project ZIP from GitHub (**Code → Download ZIP**). Extract it
**on top of** `D:\claude\jerryos`, and choose **"Replace the files in the
destination"** when Windows asks. Your `backup\` files are safe: they aren't
in the ZIP, so nothing overwrites them.

You should now have `D:\claude\jerryos\firmware\jerryos-cp2-hello.bin`.

## Step 2 🎬: the "before" shot

Before you flash, film the stock AIPI firmware booting one last time. That's
your "before" for the video.

## Step 3 🎬: flash JerryOS

Device plugged in by USB, battery on. In PowerShell, in `D:\claude\jerryos`:

```powershell
powershell -ExecutionPolicy Bypass -File tools\flash_prebuilt.ps1 -Port COM7
```

Takes about 15 seconds. 🎬 **Money shot:** the screen goes dark, then the test
card appears. Film the device, not the laptop. (Optional: record the laptop
screen at the same time with **Win + Alt + R** and cut between them.)

> Can't connect? Same fix as Checkpoint 1: switch it fully off, take the
> battery off, plug USB back in, re-run.

## Step 4 🎬: check the test card

```
+------------------------------+  <- white line around the very edge
|R JerryOS hw test            G|  <- small red square top-left, green top-right
| [R][G][B][W][Y][O]           |  <- colour swatches with letters
| L up   s0 l0                 |
|   max hold 0.0s              |
| R up   s0 l0                 |
| last: none yet               |
| batt 4.05V 84%               |
| usb YES  chg no              |
| up 12s inv off               |
|B hold R: invert             Y|  <- blue bottom-left, yellow bottom-right
+------------------------------+
```

Check and note:

1. **Edges:** is the thin white line visible on **all four** sides? Any
   garbage pixels along an edge?
2. **Corners:** red top-left, green top-right, blue bottom-left, yellow
   bottom-right? Is the text readable, not mirrored or upside down?
3. **Colours:** does each swatch's colour match its letter? R = red,
   G = green, B = blue, W = white, Y = gold, O = orange.
4. **Inversion:** **hold the RIGHT button** about 1 second. Colours flip to
   "inv ON". Hold again to flip back. **Which one looks right?** (Normal
   colours, white looks white, black background.)

## Step 5 🎬: buttons

1. Tap LEFT a few times: the `s` (short) count goes up.
2. Hold LEFT 1+ second: the `l` (long) count goes up. While holding, it
   shows `held 2.3s` counting up.
3. Same for RIGHT (remember, holding RIGHT also flips colour inversion).
4. **Important test.** On USB power: hold LEFT for **5 seconds**. Does the
   device stay on? (Note the "max hold" number afterwards.)

## Step 6 🎬: power and battery

1. With USB plugged in: note the `batt` and `usb` / `chg` lines.
2. **Unplug USB** (battery attached). Does the device stay on? `usb` should
   change to `no`. Note the battery voltage.
3. **Now on battery only:** hold LEFT for 5 seconds. Does it turn off? (If it
   does, no harm. Press LEFT or plug in USB to wake it. We just need to know.)
4. Plug USB back in. Then (if you can easily) take the battery off while on
   USB. What does the `batt` line say?
5. Bonus: if you have a multimeter, measure the battery's voltage and compare
   it with the screen. That lets us calibrate the percentage.

## Step 7 🎬 (optional): the serial log

The device also "talks" over USB. To watch it (no install needed, esptool
already brought this tool along):

```powershell
py -m serial.tools.miniterm COM7 115200
```

You'll see the boot report (chip, flash, PSRAM, reset reason) and a status
line every 2 seconds. Each button press shows up instantly. Press
**Ctrl + ]** to quit. Opening the log may restart the device; that's normal.

## What to send Claude

- Answers to steps 4 to 6 (a photo of the screen helps a lot)
- If you ran step 7: copy the first ~15 lines of the serial log

---

## Building it yourself (needed from Checkpoint 3 on)

The `.bin` above was built from the source in `src/`. To build and flash it
yourself:

1. Install **VS Code**: https://code.visualstudio.com
2. In VS Code: Extensions (left sidebar, four squares) → search
   **PlatformIO IDE** → Install. Wait for it to finish (a few minutes the
   first time) and restart VS Code when asked.
3. **File → Open Folder** → `D:\claude\jerryos`.
4. Click the **→ (Upload)** arrow in the blue bar at the bottom. The first
   build downloads the ESP32 tools (~5 minutes); after that it takes seconds.

Good to film as a short time-lapse, not real time.
