"""Copy Jerry's Wi-Fi log from the AIPI Lite to this PC as a CSV file.

    py tools\\pull_wifi_log.py COM7            save to logs\\wifi-<date>-<time>.csv
    py tools\\pull_wifi_log.py COM7 --clear    ...then erase the log on the device

Open the CSV in Excel / Google Sheets. Columns:
    session   recording number (each REC press on Jerry starts a new one)
    uptime_s  seconds since power-on when the network was saved
    bssid     the access point's hardware address
    ssid      network name ("" = hidden)
    auth      security: open, WEP, WPA2, WPA3...
    channel   2.4 GHz channel 1-13
    rssi      signal strength in dBm (-40 very close, -90 far)
    lat, lon  where you were (from the rig box's GPS); empty without a fix

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

BEGIN = "=== JERRYOS WIFI LOG BEGIN ==="
END = "=== JERRYOS WIFI LOG END"


def open_port(port):
    s = serial.Serial()
    s.port = port
    s.baudrate = 115200
    s.timeout = 0.5
    s.dtr = False  # leave these alone so opening the port doesn't reboot the device
    s.rts = False
    s.open()
    return s


def pull(port):
    with open_port(port) as s:
        time.sleep(0.3)
        s.reset_input_buffer()
        s.write(b"\nwifi dump\n")
        lines, inside, deadline = [], False, time.time() + 60
        while time.time() < deadline:
            raw = s.readline()
            if not raw:
                continue
            line = raw.decode("utf-8", errors="replace").rstrip("\r\n")
            if line.startswith(BEGIN):
                inside, lines = True, []
                deadline = time.time() + 600  # big logs take a while
            elif line.startswith(END):
                if inside:
                    return lines
            elif inside and not line.startswith("["):  # skip Jerry's own status messages
                lines.append(line)
    return None


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("port", help="e.g. COM7")
    ap.add_argument("--clear", action="store_true", help="erase the device's log after saving it")
    args = ap.parse_args()

    print(f"Asking Jerry on {args.port} for the Wi-Fi log...")
    try:
        lines = pull(args.port)
    except serial.SerialException as e:
        sys.exit(f"Could not open {args.port}: {e}\n(Is a serial monitor still open? Close it and try again.)")
    if lines is None:
        sys.exit("No answer. Is JerryOS v0.5 or newer running, and is it the right COM port?")

    out_dir = pathlib.Path(__file__).resolve().parent.parent / "logs"
    out_dir.mkdir(exist_ok=True)
    out = out_dir / f"wifi-{datetime.datetime.now():%Y%m%d-%H%M}.csv"
    out.write_text("\n".join(lines) + "\n", encoding="utf-8")
    rows = max(0, len(lines) - 1)
    print(f"Saved {rows} networks to {out}")

    if args.clear:
        if rows == 0:
            print("Log was empty; nothing to erase.")
            return
        with open_port(args.port) as s:
            time.sleep(0.3)
            s.write(b"\nwifi clear\n")
            time.sleep(1.0)
        print("Erased the log on the device (your copy above is safe).")


if __name__ == "__main__":
    main()
