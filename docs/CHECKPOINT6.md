# Checkpoint 6: The personal monitor (home screen, sound, finishing touches)

JerryOS is now what it really is: a **personal, on-the-spot monitor** you
carry. It watches for trackers following you, shows the Bluetooth and Wi-Fi
around you, and **saves only when you tell it to.** Without GPS, a log
can't say *where* anyway, so recording is a choice, not the default. It's
also the handheld starting point for the Pi Zero box (Scanner v0).

![JerryOS v0.6](img/cp6-monitor.png)
*Simulator: home → poke → REC → recording → stop → WiFi Scan, Settings
(Sound volume), Diagnostics (audio + beep test).*

🎬 = a good moment to film.

## Flash it

1. GitHub Desktop: **Fetch origin** → **Pull origin**.
2. PowerShell in the repo folder:
   ```powershell
   powershell -ExecutionPolicy Bypass -File tools\flash_prebuilt.ps1 -Port COM7
   ```
   (Picks `jerryos-cp6-monitor.bin` automatically.)
3. You should hear Jerry **whoop** at boot. (If not, see
   *If there's no sound* below.)

## What's new

### Home screen: everything at a glance

```
 JERRY            ● 95% [##]     <- red dot = recording the Wi-Fi log
     ( Gear approved! )          <- Jerry talks when you poke him
          (Jerry)
 [TAG ok] [BT 18] [WiFi 12]      <- trackers | Bluetooth nearby | Wi-Fi nearby
 [L]menu [R]poke [R]rec
```

- **TAG ok** (green) = no lost trackers around · **TAG ?** (gold) = a lost
  tracker is nearby, watching it · **TAG !** (red) = followed.
- **Tap R** = poke: Jerry grins, chirps and says something (some lines use
  live counts: "18 gadgets nearby").
- **Hold R = REC**: start saving Wi-Fi networks. The Wi-Fi pill turns red
  and counts what's been saved (`REC 34`), a red dot blinks at the top on
  every screen, and Jerry's eyes sweep while he records. **Hold R again** =
  stop ("Saved 34 networks").

### Recording is opt-in

- Scanning still runs all the time (you see networks live on WiFi Scan), but
  **nothing is saved until you press REC.** Every power-on starts with REC off.
- Each REC press starts a new **session** number in the log, so each
  recording (a drive, a walk, a campsite) can be told apart in Excel.
- Pressing REC saves everything in range straight away, then each new
  network as it appears. Each network is saved once per session.
- WiFi Scan's top line shows `REC 34` in red while recording; a network's
  detail page says `saved (REC)` or `not saved`.

### Sound: Jerry's Sasquatch voice

Jerry doesn't beep, he **sounds like a Squatch** (v0.6.1). Every sound is
made live on the chip from the classic Bigfoot repertoire: the rising-and-
falling **whoop call**, **wood knocks** and low **grunts / huffs**.

| When | Jerry |
|---|---|
| Power on | a big **whoop**, then knock-knock |
| Poke, settings saved | grunty chuckle: "huh-huh-hmm" |
| REC on | **knock knock** + an "uh-huh" grunt |
| REC off | "hmmph" + one knock |
| **Tracker following you** | **three urgent whoops**, repeated every minute until you hold R = ok |
| Battery hits 15% | a tired, falling moan (once) |
| Log erased | two low huffs |

🎧 **Hear them on the laptop first:** `docs/sounds/jerry_*.wav` (double-click
to play).

**Settings → Sound**: off / low / med / high. While editing, each tap plays
a knock at the new volume.

**Want him to actually talk?** Record your own lines ("Recording WiFi!",
"Gear approved!") as `.wav` files in the `voice/` folder; see
[voice/README.md](../voice/README.md). Moments with a recording play it;
the rest keep the Squatch sounds.

### Battery saver (screen)

On battery, the screen **dims after 1 minute** without a button press and
**turns off after 5 minutes**. Scanning, tracker watching and recording keep
running. **Any button wakes it** (that press only wakes; it doesn't also
do something). A tracker alert wakes it too. On USB the screen stays on.

### Also

- **Diagnostics** shows `audio ok` and the volume. **Hold R = beep** to test
  the speaker.
- **About** says what Jerry is: a personal monitor, listen-only, saves only
  when you REC.
- Turning **WiFi scan** off in Settings also stops a recording.

## Tests

1. 🎬 **Boot sound:** power on, hear Jerry's whoop + knock-knock.
2. 🎬 **Poke:** tap R on home a few times. Chirp + speech bubble each time.
3. **Pills:** BT and WiFi counts look sensible; TAG says ok.
4. 🎬 **REC:** hold R → "Recording WiFi...", red pill + red dot. Walk around a
   bit. Hold R → "Saved N networks". Pull the log
   (`py tools\pull_wifi_log.py COM7`): the newest lines share one session
   number.
5. **Volume:** Settings → Sound → hold R, tap R through off/low/med/high
   (ticks get louder), hold R to save. Set it to **off**: poke is silent.
   Set it back.
6. **Beep test:** Settings → Diagnostics → hold R.
7. **Screen saver:** unplug USB, don't touch anything. ~1 min: dims. ~5 min:
   off. Press a button: it comes back, and the press didn't do anything else.
8. **Nothing else broke:** Tracker Watch, BLE Nearby, WiFi Scan.

## If there's no sound

The audio chip setup follows Espressif's own driver, but this is the first
time it runs on the AIPI Lite. If it's silent:

- **Diagnostics says `audio NONE`:** the chip didn't answer. Send me the boot
  lines from the serial log (`py -m serial.tools.miniterm COM7 115200`, then
  press the reset/power). Look for `[audio] ES8311 id ...`.
- **Says `audio ok` but silent:** check Settings → Sound isn't `off`, then try
  Diagnostics → hold R. Tell me what you hear (nothing / click / buzz).

Everything else works fine without sound.
