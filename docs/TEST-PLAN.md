# Test plan: v0.10 → v0.12 (everything since Flock cameras)

One flash gives you all of it: **Followers, Shield, LED, Flock cameras,
hidden-camera hints, pocket mode, sound meter**.

## Flash (once)

1. GitHub Desktop: **Fetch origin** → **Pull origin**.
2. PowerShell in the repo folder:
   ```powershell
   powershell -ExecutionPolicy Bypass -File tools\flash_prebuilt.ps1 -Port COM7
   ```
   It picks `jerryos-cp12-pocket.bin`.
3. Settings → About says **v0.12.0**.

Menu order now: Tracker Watch · Followers · Shield · Cameras · Sound · BLE
Nearby · WiFi Scan · Rig Dashboard · Settings.

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

## ☀️ Tomorrow, out and about

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

## Report back

Just send something like: "pocket ✅, sound quiet 41 (phone 35) talking 66
(phone 60), cameras: Ring cam? showed, followers: my Garmin showed at 22 min,
Flock: drove past 2, got 1, battery 95 → 61 % in 3 h". Plus photos of
anything odd.
