# JerryOS v1.6 user guide

Jerry is a **personal, on-the-spot monitor**. He watches for trackers and
devices following you, radio attacks, Flock licence-plate cameras and hidden
cameras, and shows the Bluetooth and Wi-Fi around you.
**He only ever listens. He never transmits,** and he only saves anything
when you press REC.

## Power on

One of four intros plays, each with its own funk groove: **Jeep Jam**,
**Bluff Creek**, **Selfie** or **Cruisin'** (random by default; Settings →
Intro). Then **press any button** to start. Any button also skips the intro
and cuts the music. The grooves and the Rate Gear stamp are the only sounds
Jerry ever makes.

## Buttons

| Press | Does |
|---|---|
| **Tap L** | Menu / back |
| **Tap R** | Next (on home: the next dash) |
| **Hold R** | Select / the action shown in the bottom bar |
| **Hold L** | Home, from anywhere |
| **Hold L 5 s** | Power off (on battery; it warns you at 2 s, let go to cancel) |
| **Hold L + R (1 s)** | Pocket mode: lock / unlock |

The bottom bar always says what the buttons do. An outlined **R** means tap,
a filled **R** means hold.

## Home screen: the dashes

**Tap R** on the home screen flips to the next dash. Jerry remembers the one
you were on. **Hold R** is still
**REC** on every dash.

| Dash | What it shows |
|---|---|
| **Jerry** | His face (grins when you flip back to him, shocked on an alert, sleepy when idle or low on battery, eyes sweep while recording) + the **SAFE / WATCH / ALERT**, **BT** and **WiFi** pills |
| **Radar** | A sweeping radar. Every Bluetooth device is a green blip: the closer to the middle, the closer it is (by signal strength; the direction is just a fixed spot per device, not a real bearing). Trackers gold, lost trackers red, followers red with a ring, cameras gold squares |
| **Trail Dash** | A Jeep instrument cluster: big gauge = speed in mph with the rig box (otherwise Bluetooth devices), small gauges = Wi-Fi networks and sound dB, the fuel gauge = battery, warning lamps GPS / REC / !, the time and sunrise / sunset when the rig box has sent them |
| **All Clear** | One big word: **ALL CLEAR** (green, and how long it's been quiet), **HEADS UP** (amber) or **ALERT!** (flashing red), with the counts of lost trackers, followers, Flock cameras and attacks |
| **Trends** | Graphs of the last hour: Bluetooth, Wi-Fi and sound (sound only fills in while a dash that uses the microphone is up) |
| **Night Watch** | Full screen, dim, all red, and the LED goes dark (except in pocket mode), so it won't ruin your night vision: the time (big) once the rig box has sent it, otherwise the Bluetooth count, plus the status, counts and next sunrise / dark |

**Drive mode:** with the rig box paired, when it reports 10+ mph for 5 s Jerry
switches to the Trail Dash and the top bar says **DRIVE**. Stopped for a
minute, he goes back to the dash you had. Settings → Drive mode turns it off.

**Top bar:** screen name ("JerryOS" on home), blinking red **!** = an alert
you haven't looked at, padlock = pocket mode, red dot = REC, battery.

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
| **Trackers** | AirTag / Find My / Tile / SmartTag / Google trackers away from their owner. Alerts if one stays with you for the alert window (default 30 min). Hold R on a red row = "ok". |
| **Followers** | Any Bluetooth device that stays with you while you move (uses Wi-Fi changes as "you moved"). 4+ places over 20+ min = follower. Hold R → hold R = "mine" (never flag again). |
| **Shield** | Wi-Fi kick-off floods, Bluetooth pop-up spam (Flipper), evil-twin / many-names fake networks. |
| **Cameras** | Flock licence-plate cameras (pop-up + LED) and quiet hidden-camera hints (camera-brand Wi-Fi, spy-cam hotspots). |
| **Nearby** | Every Bluetooth device (**B**) and Wi-Fi network (**W**) heard, strongest first, in one list. Hold R = details + live signal. The top line shows the counts and the REC log size. |
| **Listen** | The microphone: a rough **dB meter** on top, and the **Knock Detector** below: **wood knocks** (short, sharp bangs well above the background, "x2 / x3" for knocks in a row) and **howls** (over a second of steady, louder sound with a steady pitch), with a 12 s level trace. Knock on a tree and listen for a reply. It says "possible": wind, doors and dogs can fool it. Tap R = clear the list, hold R = reset the peak (the line across the trace). |
| **Logbook** | Your **Badges** (14 to earn: Flock Spotter, Knock Knock, Road Tripper...) on the top row, then a recap of each REC session: how long, networks saved, miles and top speed (with the rig box), most Bluetooth devices at once, lost trackers, new followers, Flock cameras passed, knocks. The recap card also pops up when you stop REC; the last 8 are kept. Hold R = open. |
| **Rate Gear** | The **5quatch Rated** stamp for your gear reviews: tap R picks 1-5 squatch prints, hold R stamps it on screen with a horn sting (or a sad wah-wah for 1-2). |
| **Rig** | Your Pi Zero box: GPS, speed, its counts and battery (see [tools/pi](../tools/pi/README.md)). Hold R = pair / forget. |
| **Settings** | See below. |

## Settings

| Setting | Choices |
|---|---|
| Brightness | 20-100 % |
| LED | all / alerts / off |
| Pocket | dark (screen stays black when locked) / lit |
| Alert after | tracker alert window: 10-60 min |
| Drive mode | auto (Trail Dash while the rig says you're driving) / off |
| Intro | which intro plays at power-on: random (a different one each time), jeep, creek, selfie, cruise. Saving one plays it for you |
| Intro tune | off / low / med / high: how loud the funk groove at power-on (and the Rate Gear stamp) is |
| Sound cal | -30 to +30 dB in 3 dB steps: match the Listen meter to a phone app (changes live while editing) |
| WiFi scan | on / off (off also stops REC, Shield's Wi-Fi checks, Followers) |
| Clock | set the time and date by hand when you don't have the rig box: tap R = +1, hold R = next part (hour, minutes, month, day), the last hold saves. Jerry has no battery-backed clock, so it lasts until he restarts; the rig box's clock takes over when it's around |
| REC logs | how much is saved (Wi-Fi + Bluetooth); hold R twice = erase both |
| My devices | how many you've marked "mine"; hold R twice = clear |
| Power off | hold R twice = switch off (on battery; on USB Jerry stays on and says so). Same as holding L for 5 s |
| Reset all | hold R twice = factory reset (settings, "mine" list, rig pairing, logs) and restart |
| Diagnostics | battery (including **time left**), radios, heap, uptime, version |
| About | version and credits |

The home dash is remembered by itself (tap R on home), so there's no setting for it.

Hold R on a setting to edit it, tap R to change, hold R to save, tap L to undo.

## Saving and getting the logs

1. Hold R on home → **REC**. Jerry saves every Wi-Fi network **and every
   Bluetooth device** he hears (once each per recording), with GPS when the
   rig box is paired. Hold R again → stop, and the Expedition recap pops up.
2. Get them onto your computer, either way:
   - **Easiest:** open the [web installer](https://obey-the-claw.github.io/JerryOS/) in Chrome or Edge, plug
     Jerry in and click **Download my logs**.
   - Or with Python: `py tools\pull_wifi_log.py COM7` (add `--clear` to
     also erase them on Jerry). They land in `logs\`.
3. Three CSV files, for Excel or Google Sheets:
   - **wifi**: session, uptime_s, bssid, ssid, auth, channel, rssi, lat, lon
   - **ble**: session, uptime_s, address, label (what it is), kind (device /
     tracker / lost tracker / follower / camera), rssi, lat, lon
   - **trips**: the Expedition recaps (trip, day of year, start minute,
     seconds, networks, meters, top km/h, peak BT, trackers, followers,
     Flock, knocks)

## Battery

On battery the screen dims after 1 min and goes off after 5 min (any button
wakes it; that press does nothing else). Alerts wake it, except in dark
pocket mode, where the LED tells you. On USB it stays on.

**Time left:** after about 10 minutes on battery Jerry works out how fast
it's going down and shows the time left on the Trail Dash's fuel gauge
(where the F was) and in Diagnostics.

**Switching off:** hold L for 5 s, or Settings → Power off. On USB the
board can't switch off (USB keeps it powered).

## Flashing a new version

**Easiest:** the [web installer](https://obey-the-claw.github.io/JerryOS/) in Chrome or Edge always has the
newest JerryOS: click **Install JerryOS** and leave "Erase device" unticked.

Or get the latest copy of this project (GitHub Desktop: Fetch → Pull, or
`git pull`, or download the ZIP again), then flash the newest file in
`firmware/`. Your settings, badges and logs survive an update.

- Windows, in PowerShell in the project folder:
  `powershell -ExecutionPolicy Bypass -File tools\flash_prebuilt.ps1`
  (add `-Port COM7` or whichever port your AIPI Lite is on).
- macOS / Linux:
  `esptool --chip esp32s3 --port <port> write-flash 0x0 firmware/<newest>.bin`

Full install steps, including the backup, are in the [README](../README.md#install).

## Honest limits

- 2.4 GHz Wi-Fi only; no GPS or clock of its own (the rig box adds both; the
  time keeps counting after the box leaves, until Jerry restarts).
- Phones and AirPods change their Bluetooth address every ~15 min, which is
  privacy working as intended. Followers ignores them on purpose.
- Camera hints are hints. A camera that records to a card is radio-silent.
- The sound meter isn't factory calibrated: set **Sound cal** once against a
  phone sound-meter app.
