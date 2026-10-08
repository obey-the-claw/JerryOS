# JerryOS v1.0 user guide

Jerry is a **personal, on-the-spot monitor**. He watches for trackers and
devices following you, radio attacks, Flock licence-plate cameras and hidden
cameras, and shows the Bluetooth and Wi-Fi around you.
**He only ever listens. He never transmits,** and he only saves anything
when you press REC.

## Power on

Jerry walks out of the woods, hops in his Jeep and drives off to his funk
groove (a Bootsy-style "Space Bass" line), and "JerryOS" drops in (about
4 s). Then **press any button** to start. Any button also skips the intro
and cuts the music. That groove is the only sound Jerry ever makes.

## Buttons

| Press | Does |
|---|---|
| **Tap L** | Menu / back |
| **Tap R** | Next / poke Jerry |
| **Hold R** | Select / the action shown in the bottom bar |
| **Hold L** | Home, from anywhere |
| **Hold L + R (1 s)** | Pocket mode: lock / unlock |

The bottom bar always says what the buttons do. An outlined **R** means tap,
a filled **R** means hold.

## Home screen

- **Jerry:** grins when poked, gets shocked on an alert, sleepy when idle or
  low on battery, eyes sweep while recording.
- **Pills:** **SAFE** / **WATCH** / **ALERT** · **BT** = Bluetooth devices
  nearby · **WiFi** = networks nearby (or **REC n** while recording).
- **Tap R:** poke (Jerry says something). **Hold R:** start / stop
  **REC** (saves Wi-Fi networks, with GPS when the rig box is paired).
- **Top bar:** screen name ("JerryOS" on home), blinking red **!** = an alert you haven't
  looked at, padlock = pocket mode, red dot = REC, battery.

## The colour LED

| LED | Meaning |
|---|---|
| 🔴 red, fast blink | Look at Jerry: new tracker / follower / attack alert |
| 🟠 amber, slow pulse | Watching: lost tracker, follower, attack still going, or a Flock camera nearby |
| 🔵 blue blip every 3 s | Recording (REC) |
| 🟢 soft green breath | All clear (LED set to "all") |

## Menu

| Mode | What it does |
|---|---|
| **Tracker Watch** | AirTag / Find My / Tile / SmartTag / Google trackers away from their owner. Alerts if one stays with you for the alert window (default 30 min). Hold R on a red row = "ok". |
| **Followers** | Any Bluetooth device that stays with you while you move (uses Wi-Fi changes as "you moved"). 4+ places over 20+ min = follower. Hold R → hold R = "mine" (never flag again). |
| **Shield** | Wi-Fi kick-off floods, Bluetooth pop-up spam (Flipper), evil-twin / many-names fake networks. |
| **Cameras** | Flock licence-plate cameras (pop-up + LED) and quiet hidden-camera hints (camera-brand Wi-Fi, spy-cam hotspots). |
| **Sound** | Rough dB meter from the built-in microphone (on only while this screen is open). Hold R = reset peak. "raw" = the mic's level before calibration. |
| **BLE Nearby** | Every Bluetooth device heard, closest first. Hold R = details + live signal. |
| **WiFi Scan** | Every Wi-Fi network heard, closest first. Hold R = details. |
| **Rig Dashboard** | Your Pi Zero box: GPS, speed, its counts and battery (see [tools/pi](../tools/pi/README.md)). Hold R = pair / forget. |
| **Settings** | See below. |

## Settings

| Setting | Choices |
|---|---|
| Brightness | 20-100 % |
| Alert after | tracker alert window: 10-60 min |
| LED | all / alerts / off |
| Pocket | dark (screen stays black when locked) / lit |
| Sound cal | -30 to +30 dB in 3 dB steps: match the Sound meter to a phone app (changes live while editing) |
| Intro | which intro plays at power-on: random (a different one each time), jeep, creek, selfie, cruise. Saving one plays it for you |
| Intro tune | off / low / med / high: how loud the funk groove at power-on is |
| WiFi scan | on / off (off also stops REC, Shield's Wi-Fi checks, Followers) |
| WiFi log | size; hold R twice = erase |
| My devices | how many you've marked "mine"; hold R twice = clear |
| Reset all | hold R twice = factory reset (settings, "mine" list, rig pairing, Wi-Fi log) and restart |
| Diagnostics | battery, radios, heap, uptime, version |
| About | version and credits |

Hold R on a setting to edit it, tap R to change, hold R to save, tap L to undo.

## Saving and getting the log

1. Hold R on home → REC. Hold R again → stop.
2. Plug into the laptop, close any serial monitor, then:
   `py tools\pull_wifi_log.py COM7` (add `--clear` to also erase it on Jerry).
3. Open `logs\wifi-<date>.csv` in Excel. Columns: session, uptime_s, bssid,
   ssid, auth, channel, rssi, lat, lon.

## Battery saver

On battery the screen dims after 1 min and goes off after 5 min (any button
wakes it; that press does nothing else). Alerts wake it, except in dark
pocket mode, where the LED tells you. On USB it stays on.

## Flashing a new version

GitHub Desktop: Fetch → Pull. Then in PowerShell, in the repo folder:
`powershell -ExecutionPolicy Bypass -File tools\flash_prebuilt.ps1 -Port COM7`

## Honest limits

- 2.4 GHz Wi-Fi only; no GPS of its own (the rig box adds it); no clock.
- Phones and AirPods change their Bluetooth address every ~15 min, which is
  privacy working as intended. Followers ignores them on purpose.
- Camera hints are hints. A camera that records to a card is radio-silent.
- The sound meter isn't factory calibrated: set **Sound cal** once against a
  phone sound-meter app.
