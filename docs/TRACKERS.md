# How Tracker Watch works

Tracker Watch listens for Bluetooth broadcasts from item trackers (AirTags
and friends) and warns you if one seems to be **travelling with you**.

## Strictly passive

The ESP32's radio only **listens**. JerryOS uses a *passive* BLE scan: it
never sends scan requests, never connects to anything, never advertises,
and never jams or interferes. It hears exactly what any phone in the cab
already hears.

## What a tracker sounds like

Every BLE broadcast is a short list of records. Each tracker brand has a
recognisable fingerprint:

| Kind | Fingerprint | "Separated from owner" signal |
|---|---|---|
| **AirTag** | Apple manufacturer data (company `0x004C`), Find My type `0x12`, status byte bits 4–5 = `1` | Find My length byte `0x19` (the long, full-key broadcast) |
| **Find My** (3rd-party Find My accessories, AirPods) | as above, status bits = `2` (accessory) or `3` (AirPods) | same `0x19` |
| **Tile** | 16-bit service UUID `0xFEED` (or Tile's newer `0xFD84`), in a UUID list or as service data | no flag, but older Tiles never change address |
| **SmartTag** (Samsung) | service data `0xFD5A`, first byte `0b00010xxx` | low 3 bits `011` (overmature offline) or `010` (offline) |
| **Google** (Find Hub / Find My Device network) | service data `0xFEAA` with frame type `0x40` / `0x41` | `0x41` = "unwanted tracking protection" mode on |

Apple status bits `0` mean an iPhone/Mac (e.g. a lost phone). These are
**not** counted as item trackers.

A tracker that's **with its owner** (your passenger's keys, your own AirTag
next to your iPhone) broadcasts the "nearby" form. JerryOS still lists it
(marked `own`), but **never alerts on it**. Only separated trackers count.

## Why "persistence" instead of "same address"

Trackers change their Bluetooth address to protect the owner's privacy:
AirTags roughly every 15 minutes near their owner and every 24 hours when
separated; SmartTags every 15 minutes. So JerryOS doesn't rely on one
address. For each **kind** it keeps a strip of the last 64 minutes: was a
separated tracker of this kind heard in that minute?

- Gaps of up to **2 minutes** are forgiven (signals drop out).
- If a kind has been around continuously for the whole **alert window**
  (Settings → Alert after; default **30 min**), that's an alert. Something
  of that type has kept up with you the entire time, across stops and turns.
- A tag you drive past, or one at a single parking lot, drops out quickly
  and never builds up a streak.

**OK (hold R on the red row)** acknowledges an alert. That kind stays quiet
for one alert window; if it's *still* following after that, it alerts again.

## Limits (be honest about them)

- **It can't prove intent.** A passenger's lost-mode AirTag in a bag looks
  the same as a planted one. The alert means *"check your vehicle"*.
- **Sitting still for 30+ minutes** (camping, a long lunch) with a
  separated tracker nearby will also build a streak. Persistence works best
  while moving.
- A separated AirTag usually takes a while (often ~15 min or more) after
  leaving its owner before it switches to the "separated" broadcast.
- Tile has no separated flag, so your *own* Tile riding along will trigger
  after the window. Acknowledge it, or leave it out of the Jeep.
- Not yet covered: Chipolo (`0xFE33`) and Pebblebee (`0xFA25`) native
  protocols. Their Google/Apple-network versions *are* covered above.

## BLE Nearby labels

`src/ble_nearby.cpp` picks the best label from each broadcast, best first:

1. **Its own name** (AD type `0x09`/`0x08`), if it sends one: speakers, TVs,
   fitness bands, cars.
2. **A device type:** trackers (with lost/own), Apple's first data byte
   (`0x07` AirPods/Beats, `0x09` AirPlay, `0x10`/`0x0C`/`0x0F` other Apple
   gear), Microsoft data = Windows PC, service UUIDs `0xFE2C` Google Fast
   Pair, `0xFD69`/`0xFD5A` Samsung, `0xFE9F` Google.
3. **The maker,** from the Bluetooth SIG company ID (Apple, Samsung, Garmin...).
4. Otherwise **unknown.**

A device sends several kinds of broadcast, so the table keeps the best label
it has seen for each address. Up to 64 devices; the longest-silent one makes
room for a new one.

## Sources (thank you)

- **AirGuard** by TU Darmstadt's SEEMOO lab, the open-source anti-stalking
  app. Its device-type rules and connection-state parsing are the basis of
  `src/tracker_detect.cpp`: https://github.com/seemoo-lab/AirGuard
- Google, *Find Hub Network Accessory Specification* (frame types
  `0x40`/`0x41`): https://developers.google.com/nearby/fast-pair/specifications/extensions/fmdn
- Bluetooth SIG assigned 16-bit UUIDs (`0xFEED`/`0xFD84` Tile, `0xFD5A`
  Samsung, `0xFEAA` Google); reelyActive's BLE identifier reference:
  https://reelyactive.github.io/ble-identifier-reference.html
- Adam Catley's AirTag reverse-engineering notes (advertisement layout):
  https://adamcatley.com/AirTag.html

## Tested

`tools/sim/run_tests.sh` runs 60 checks: every fingerprint above, junk and
malformed broadcasts (including 20,000 random ones), and scenarios: a
rotating-address AirTag following for 40 min (alerts at 30), your own AirTag
for an hour (never alerts), a 5-minute drive-by (no alert), signal gaps,
acknowledge-and-re-alert, a 500-tag parking lot, and the `millis()` wrap
after 49 days. Plus the BLE Nearby labels (names, device types, makers,
long or garbled names, broken lengths) and its table (a crowd of 300).
