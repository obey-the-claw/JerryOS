#!/usr/bin/env python3
"""jerry_beacon.py - runs on the Pi Zero "Scanner" box and tells Jerry what it knows.

Every 2 seconds it broadcasts a small Bluetooth "status beacon" with:
  - the GPS fix (from gpsd: lat, lon, satellites, speed)
  - the box's own counts, if your scanner software writes them to
    /run/jerry_status.json, e.g. {"wifi": 45, "ble": 23, "alerts": 0, "battery": 87}
    (battery: percent, or leave it out when on mains power)

Jerry (the AIPI Lite) only LISTENS for this beacon; he never transmits. The
format is described in JerryOS src/rig_link.h. While the box has a GPS fix,
Jerry's Wi-Fi log (REC) gets latitude/longitude on every line.

Setup on Raspberry Pi OS (Bluetooth + Python are already there):
    sudo apt install gpsd            # if you haven't already, for the GPS
    sudo python3 jerry_beacon.py     # needs root to advertise over BlueZ

Try it without Bluetooth (prints the beacon bytes):
    python3 jerry_beacon.py --print

Then on Jerry: Menu -> Rig Dashboard -> "Rig box found!" -> hold R to pair.

To start it at boot, see the bottom of this file.
"""
import argparse
import json
import socket
import struct
import sys
import threading
import time

STATUS_FILE = "/run/jerry_status.json"
COMPANY_ID = 0xFFFF  # Bluetooth SIG: "for testing / internal use"
VERSION = 1
UPDATE_S = 2.0


# --- What we know -------------------------------------------------------------
class State:
    def __init__(self):
        self.lock = threading.Lock()
        self.fix = False
        self.lat = 0.0
        self.lon = 0.0
        self.sats = 0
        self.speed_kmh = 0.0
        self.seq = 0


def gps_reader(state):
    """Follow gpsd's JSON stream (localhost:2947). Reconnects if gpsd restarts."""
    while True:
        try:
            with socket.create_connection(("127.0.0.1", 2947), timeout=10) as s:
                s.sendall(b'?WATCH={"enable":true,"json":true}\n')
                buf = b""
                while True:
                    chunk = s.recv(4096)
                    if not chunk:
                        break
                    buf += chunk
                    while b"\n" in buf:
                        line, buf = buf.split(b"\n", 1)
                        try:
                            msg = json.loads(line)
                        except ValueError:
                            continue
                        with state.lock:
                            if msg.get("class") == "TPV":
                                state.fix = msg.get("mode", 0) >= 2 and "lat" in msg and "lon" in msg
                                if state.fix:
                                    state.lat = msg["lat"]
                                    state.lon = msg["lon"]
                                    state.speed_kmh = msg.get("speed", 0.0) * 3.6
                            elif msg.get("class") == "SKY":
                                used = msg.get("uSat")
                                if used is None:
                                    used = sum(1 for s_ in msg.get("satellites", []) if s_.get("used"))
                                state.sats = used
        except OSError:
            with state.lock:
                state.fix = False
            time.sleep(5)  # gpsd not running (yet)


def box_counts():
    try:
        with open(STATUS_FILE) as f:
            d = json.load(f)
        return (int(d.get("wifi", 0)), int(d.get("ble", 0)), int(d.get("alerts", 0)),
                int(d.get("battery", 255)))
    except (OSError, ValueError):
        return 0, 0, 0, 255


def clamp(v, lo, hi):
    return max(lo, min(hi, v))


def beacon_body(state):
    """The 22 bytes after the company ID (see src/rig_link.h)."""
    wifi, ble, alerts, battery = box_counts()
    with state.lock:
        state.seq = (state.seq + 1) & 0xFFFF
        lat = int(round(state.lat * 1e7)) if state.fix else 0
        lon = int(round(state.lon * 1e7)) if state.fix else 0
        return struct.pack(
            "<2sBBHiiBBHHBB", b"JR", VERSION, 1 if state.fix else 0, state.seq,
            clamp(lat, -900000000, 900000000), clamp(lon, -1800000000, 1800000000),
            clamp(state.sats, 0, 255), clamp(int(state.speed_kmh + 0.5), 0, 255),
            clamp(wifi, 0, 65535), clamp(ble, 0, 65535), clamp(alerts, 0, 255),
            clamp(battery, 0, 255))


# --- Bluetooth (BlueZ over D-Bus) ---------------------------------------------
def run_bluez(state):
    import dbus
    import dbus.mainloop.glib
    import dbus.service
    from gi.repository import GLib

    dbus.mainloop.glib.DBusGMainLoop(set_as_default=True)
    bus = dbus.SystemBus()
    adapter = None
    om = dbus.Interface(bus.get_object("org.bluez", "/"), "org.freedesktop.DBus.ObjectManager")
    for path, ifaces in om.GetManagedObjects().items():
        if "org.bluez.LEAdvertisingManager1" in ifaces:
            adapter = path
            break
    if not adapter:
        sys.exit("No Bluetooth LE adapter found (is Bluetooth on? `bluetoothctl power on`)")
    props = dbus.Interface(bus.get_object("org.bluez", adapter), "org.freedesktop.DBus.Properties")
    props.Set("org.bluez.Adapter1", "Powered", dbus.Boolean(True))
    manager = dbus.Interface(bus.get_object("org.bluez", adapter), "org.bluez.LEAdvertisingManager1")

    class Advert(dbus.service.Object):
        PATH = "/org/jerryos/beacon0"

        def __init__(self):
            self.body = beacon_body(state)
            super().__init__(bus, self.PATH)

        def props(self):
            return {
                "Type": "broadcast",  # not connectable: just a status shout
                "ManufacturerData": dbus.Dictionary(
                    {dbus.UInt16(COMPANY_ID): dbus.Array(self.body, signature="y")}, signature="qv"),
            }

        @dbus.service.method("org.freedesktop.DBus.Properties", in_signature="s", out_signature="a{sv}")
        def GetAll(self, interface):
            return self.props()

        @dbus.service.method("org.bluez.LEAdvertisement1")
        def Release(self):
            pass

    advert = Advert()

    def refresh():
        # BlueZ copies the data when an advert is registered, so to change it we
        # unregister, update and register again.
        try:
            manager.UnregisterAdvertisement(Advert.PATH)
        except dbus.DBusException:
            pass
        advert.body = beacon_body(state)
        manager.RegisterAdvertisement(Advert.PATH, {}, reply_handler=lambda: None,
                                      error_handler=lambda e: print("advert error:", e))
        return True

    refresh()
    GLib.timeout_add(int(UPDATE_S * 1000), refresh)
    print(f"Broadcasting Jerry's rig beacon every {UPDATE_S:.0f} s on {adapter}. Ctrl+C to stop.")
    try:
        GLib.MainLoop().run()
    finally:
        try:
            manager.UnregisterAdvertisement(Advert.PATH)
        except dbus.DBusException:
            pass


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--print", action="store_true", help="print beacons instead of broadcasting")
    ap.add_argument("--fake-gps", metavar="LAT,LON", help="pretend to have this fix (testing without a GPS)")
    args = ap.parse_args()

    state = State()
    if args.fake_gps:
        lat, lon = (float(v) for v in args.fake_gps.split(","))
        state.fix, state.lat, state.lon, state.sats = True, lat, lon, 9
    else:
        threading.Thread(target=gps_reader, args=(state,), daemon=True).start()

    if args.print:
        while True:
            body = beacon_body(state)
            print(struct.pack("<H", COMPANY_ID).hex(" "), "|", body.hex(" "))
            time.sleep(UPDATE_S)
    else:
        run_bluez(state)


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        pass

# --- Start at boot (systemd) -------------------------------------------------
# sudo cp jerry_beacon.py /usr/local/bin/ && sudo chmod +x /usr/local/bin/jerry_beacon.py
# sudo tee /etc/systemd/system/jerry-beacon.service <<'EOF'
# [Unit]
# Description=Jerry rig beacon
# After=bluetooth.service gpsd.service
# [Service]
# ExecStart=/usr/bin/python3 /usr/local/bin/jerry_beacon.py
# Restart=always
# [Install]
# WantedBy=multi-user.target
# EOF
# sudo systemctl enable --now jerry-beacon
