"""Copy Jerry's logs from the AIPI Lite to this PC as CSV files.

    py tools\\pull_wifi_log.py COM7            save them in logs\\
    py tools\\pull_wifi_log.py COM7 --clear    ...then erase the REC logs on the device

(No Python? The web page does the same in Chrome / Edge: see the README.)

Three files, named with the date and time:
  wifi-<date>.csv   Wi-Fi networks saved while REC was on
      session   recording number (each REC press on Jerry starts a new one)
      uptime_s  seconds since power-on when the network was saved
      bssid     the access point's hardware address
      ssid      network name ("" = hidden)
      auth      security: open, WEP, WPA2, WPA3...
      channel   2.4 GHz channel 1-13
      rssi      signal strength in dBm (-40 very close, -90 far)
      lat, lon  where you were (from the rig box's GPS); empty without a fix
  ble-<date>.csv    Bluetooth devices heard while REC was on (JerryOS 1.6+)
      session, uptime_s, address, label (what it is), kind (device / tracker /
      lost tracker / follower / camera), rssi, lat, lon
  trips-<date>.csv  the Expedition recaps (JerryOS 1.4+)

Needs pyserial, which comes with esptool (py -m pip install esptool).
Close any serial monitor first: only one program can use the COM port.
"""
import argparse
import datetime
import pathlib
import sys
import time

try:
    import serial
except ImportError:
    sys.exit("pyserial is missing. Run:  py -m pip install pyserial")

# command, start marker, end marker, file prefix, what one row is
LOGS = [
    ("wifi dump", "=== JERRYOS WIFI LOG BEGIN ===", "=== JERRYOS WIFI LOG END", "wifi", "networks"),
    ("ble dump", "=== JERRYOS BLE LOG BEGIN ===", "=== JERRYOS BLE LOG END", "ble", "Bluetooth devices"),
    ("trips", "=== JERRYOS TRIPS BEGIN ===", "=== JERRYOS TRIPS END", "trips", "expeditions"),
]


def open_port(port):
    s = serial.Serial()
    s.port = port
    s.baudrate = 115200
    s.timeout = 0.5
    s.dtr = False  # leave these alone so opening the port doesn't reboot the device
    s.rts = False
    s.open()
    return s


def pull(s, command, begin, end):
    """Send one command, return the lines between its markers (None = no answer)."""
    s.reset_input_buffer()
    s.write(b"\n" + command.encode() + b"\n")
    lines, inside, deadline = [], False, time.time() + 15
    while time.time() < deadline:
        raw = s.readline()
        if not raw:
            continue
        line = raw.decode("utf-8", errors="replace").rstrip("\r\n")
        if line.startswith(begin):
            inside, lines = True, []
            deadline = time.time() + 600  # big logs take a while
        elif line.startswith(end):
            if inside:
                return lines
        elif inside and not line.startswith("["):  # skip Jerry's own status messages
            lines.append(line)
    return None


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("port", help="e.g. COM7")
    ap.add_argument("--clear", action="store_true", help="erase the device's REC logs after saving them")
    args = ap.parse_args()

    out_dir = pathlib.Path(__file__).resolve().parent.parent / "logs"
    out_dir.mkdir(exist_ok=True)
    stamp = f"{datetime.datetime.now():%Y%m%d-%H%M}"
    print(f"Asking Jerry on {args.port} for his logs...")
    got_any = False
    try:
        with open_port(args.port) as s:
            time.sleep(0.3)
            for command, begin, end, prefix, what in LOGS:
                lines = pull(s, command, begin, end)
                if lines is None:
                    print(f"  {prefix}: no answer (older JerryOS?) - skipped")
                    continue
                got_any = True
                out = out_dir / f"{prefix}-{stamp}.csv"
                out.write_text("\n".join(lines) + "\n", encoding="utf-8")
                print(f"  saved {max(0, len(lines) - 1)} {what} to {out}")
            if args.clear and got_any:
                s.write(b"\nwifi clear\n")
                time.sleep(0.5)
                s.write(b"\nble clear\n")
                time.sleep(1.0)
                print("Erased the REC logs on the device (your copies above are safe).")
    except serial.SerialException as e:
        sys.exit(f"Could not open {args.port}: {e}\n(Is a serial monitor still open? Close it and try again.)")
    if not got_any:
        sys.exit("No answer. Is JerryOS running, and is it the right COM port?")


if __name__ == "__main__":
    main()
