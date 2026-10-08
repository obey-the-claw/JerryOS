# Pi Zero rig box → Jerry

`jerry_beacon.py` runs on the Pi Zero (W / 2 W) "Scanner" box. Every 2 s it
broadcasts a small Bluetooth beacon with the box's **GPS fix** and its own
counts. Jerry just listens for it (he never transmits) and shows it on the
**Rig Dashboard**. While the box has a fix, Jerry's **REC** log lines get
**lat / lon**.

## Set up (once)

On the Pi (Raspberry Pi OS, Bluetooth on):

```bash
sudo apt install gpsd           # the GPS service (skip if you have it)
# point gpsd at your GPS, e.g. in /etc/default/gpsd: DEVICES="/dev/ttyACM0"
sudo python3 jerry_beacon.py    # needs root to advertise
```

No GPS yet? Test with a pretend fix:

```bash
sudo python3 jerry_beacon.py --fake-gps 33.5123,-112.1234
python3 jerry_beacon.py --print # just print the beacon bytes, no Bluetooth
```

Then on Jerry: **Menu → Rig Dashboard** → "Rig box found!" → **hold R to
pair**. From then on Jerry trusts only that box (Hold R again = forget).

## The box's own counts (optional)

Have your scanner software write `/run/jerry_status.json`, for example:

```json
{"wifi": 45, "ble": 23, "alerts": 0, "battery": 87}
```

(Leave out `battery` when it runs on mains/12 V.) Jerry shows these on the
dashboard. Without the file, they show as 0.

## Start at boot

See the systemd snippet at the bottom of `jerry_beacon.py`.

## The beacon format

Documented in `src/rig_link.h`: company ID 0xFFFF (Bluetooth SIG's "testing /
private use" ID), then 22 bytes: "JR", version, flags, sequence, lat/lon ×1e7,
satellites, speed, counts, alerts, battery. The PC tests check that the bytes
this script makes decode correctly on Jerry.
