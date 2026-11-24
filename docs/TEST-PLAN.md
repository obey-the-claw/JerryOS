# Test plan: JerryOS

## v1.4 (cp18): dashes, Knock Detector, Expeditions, Rate Gear, Badges

Flash `jerryos-cp18-dashes.bin` (flash_prebuilt picks it). About says **v1.4.0**.

- [ ] Boot: the intro shows **IG @5quatchRated** under the version
- [x] Home: tap R flips Jerry → Radar → Trail Dash → All Clear → Trends →
      Night Watch → Jerry (name pops up each time; landing on Jerry = poke).
      Steve: "thats amazing"
- [ ] Restart: Jerry comes back on the dash you left him on
- [ ] Radar: blips sweep and fade; walk a Bluetooth gadget closer, its blip moves in
- [ ] Trail Dash: BT gauge, Wi-Fi + dB gauges move, fuel gauge = battery
- [ ] Night Watch: dim and red, readable in a dark room
- [ ] Knock Detector: knock on a table / door: "KNOCK!", two quick knocks = "x2"
- [ ] Hold R on home (REC), wait a minute, hold R again: the recap card pops up;
      Menu → Expeditions lists it
- [ ] Rate Gear: tap R to 3, hold R: stamp + horn sting; try 1 (sad wah-wah)
- [ ] Badges: Gear Inspector earned (toast) after the stamp
- [ ] With the Pi rig box (new `jerry_beacon.py`): the time on Trail Dash /
      Night Watch, sunrise / sunset, Drive mode when you drive off

---


One flash gives you everything: **Followers, Shield, LED, Flock cameras,
hidden-camera hints, pocket mode, sound meter, Rig Dashboard, GPS-tagged
logs, reset**. Already confirmed on the device: pocket mode, sound meter
(shows "talking"), LED green, Shield "all quiet", Followers / Cameras at home.

## Flash (once)

1. GitHub Desktop: **Fetch origin** → **Pull origin**.
2. PowerShell in the repo folder:
   ```powershell
   powershell -ExecutionPolicy Bypass -File tools\flash_prebuilt.ps1 -Port COM7
   ```
   It picks `jerryos-cp13-v1.bin`.
3. Settings → About says **v1.0.0**.

Menu order: Tracker Watch · Followers · Shield · Cameras · Sound · BLE
Nearby · WiFi Scan · Rig Dashboard · Settings. Full guide: [USER-GUIDE.md](USER-GUIDE.md).

Write down anything odd as you go (screen + what it said). A photo is perfect.

---

## 🌙 Tonight, home office

**Pocket mode**
1. Hold **both** buttons ~1 s → "Pocket mode: locked", padlock, then the
   screen goes black (LED keeps going).
2. Press one button → the screen peeks for 4 s, "Locked: hold both". Nothing
   else happens.
3. Hold both again → "Unlocked". Buttons work normally.
4. Settings → **Pocket → lit**, lock again: the screen should now follow the
   normal dimming instead of going black. Set it back to **dark**.
5. Make sure single long presses still work when *not* locked (hold L = home,
   hold R = REC on home).

**Sound meter**
6. Menu → **Sound**. Quiet room: a low number, "quiet". Talk: it goes up to
   "talking". Clap: it jumps and the peak line moves. Hold R → peak resets.
7. 🎬 Open a phone sound-meter app next to Jerry. Note both numbers for a
   quiet room and for talking (so I can calibrate).
8. Leave the Sound screen. Nothing else changes (the mic switches off by itself).
9. If it says **"microphone not found"** or the number never moves, tell me.

**Cameras (hidden-camera hints)**
10. Menu → **Cameras**, wait **1 minute**. Own Ring / Wyze / other brand
    camera? It should show "Ring cam?" etc. Walk closer → bars go up.
    No cameras → empty list (or weak neighbours' doorbells).

**Shield / LED quick checks**
11. Shield says "all quiet". LED breathes green.
12. Optional evil-twin test: phone hotspot **with no password**, named
    exactly like your home Wi-Fi → Shield "Fake networks NOW", LED blinks red
    until you look, then amber. Turn the hotspot off.

**Nothing else broke**
13. Tracker Watch, BLE Nearby, WiFi Scan still show their usual numbers.
    Home pills look normal. REC still works (hold R on home).

---

**v1.0 additions (home)**
14. **Log upgrade:** pull the Wi-Fi log (`py tools\pull_wifi_log.py COM7`).
    The header now ends in `lat,lon`, and older lines have two empty columns.
15. **Reset all:** only if you don't mind setting things up again. Settings →
    Reset all → hold R ("sure?") → hold R. Jerry restarts with defaults
    (brightness 80 %, LED all, "My devices" 0, log empty).
16. **Rig box (when the Pi is ready):** follow [tools/pi/README.md](../tools/pi/README.md)
    (`--fake-gps` works without a GPS). Menu → Rig Dashboard → "Rig box
    found!" → hold R. You should see the fix, lat/lon and mph. Then REC for
    a minute and pull the log: the new lines have lat/lon.

## ☀️ Out and about (over the next few days)

Before you leave: **Wi-Fi scan on**, battery charged. Pocket mode is fine
(the LED tells you if something needs a look).

1. 🎬 **Followers (30+ min drive or walk through town):** Menu → Followers
   shows "moved N times" climbing. Your own gear that doesn't change
   address (watch, car, speaker) shows up after ~20 min → hold R, hold R =
   **mine**. Your phone and AirPods shouldn't show.
2. 🎬 **Flock cameras:** check [deflock.me](https://deflock.me) for cameras
   on your route. Passing one → gold **"Flock camera nearby!"** + LED amber.
   Menu → Cameras shows it. Tell me if you pass a known one and get nothing.
3. **Shield in public:** it should stay "all quiet" in shops, cafés, parking
   lots. If it trips, note the row + detail line + where you were.
4. **Cameras in shops / streets:** lots of "Ring cam?", "Hikvision cam?"
   hints is normal (doorbells, store cameras). Tell me if anything looks
   clearly wrong (e.g. your own phone listed as a camera).
5. **Sound:** check a loud place (traffic, a restaurant) against the phone app.
6. **Battery:** note the % when you leave and when you get back, and how
   long you were out. All radios are busy now, so I want to see real
   battery life.
7. **Sound calibration (v1.0.1):** with a phone sound-meter app next to
   Jerry, talk at a steady volume. Settings → **Sound cal** → hold R, tap R
   (3 dB per tap, live) until Jerry's Sound screen matches the phone, hold R
   to save. Then check a quiet room. If quiet and talking can't both
   match, send me a photo of the Sound screen (it shows "raw") with the
   phone's number for each.

## Report back

Just send something like: "pocket ✅, sound quiet 41 (phone 35) talking 66
(phone 60), cameras: Ring cam? showed, followers: my Garmin showed at 22 min,
Flock: drove past 2, got 1, battery 95 → 61 % in 3 h". Plus photos of
anything odd.
