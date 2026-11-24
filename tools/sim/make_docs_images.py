"""Refresh the README / web-installer pictures from the simulator's output.

    tools/sim/build_sim.sh <LovyanGFX>     (first: makes tools/sim/out/)
    python3 tools/sim/make_docs_images.py

Writes docs/img/screens.png (dashes + the fun stuff), docs/img/modes.png (the
menu modes), docs/img/intros.png (the four title cards), docs/img/intro.gif,
docs/img/favicon.png, and copies the intro / radar / stamp animations into
docs/previews/.
"""
import glob
import os
import re
import shutil

from PIL import Image, ImageDraw

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
OUT = os.path.join(REPO, "tools/sim/out")
SHOTS = os.path.join(OUT, "shots")


def shot(name):
    for f in os.listdir(SHOTS):
        if re.fullmatch(r"\d+_" + re.escape(name) + r"\.png", f):
            return os.path.join(SHOTS, f)
    raise SystemExit(f"no screenshot called {name} (run build_sim.sh first)")


def sheet(items, path, cols=4):
    w, h, pad, lh = 256, 272, 10, 22
    ims = [(Image.open(shot(n)).resize((w, h)), label) for n, label in items]
    rows = (len(ims) + cols - 1) // cols
    o = Image.new("RGB", (cols * (w + pad) + pad, rows * (h + lh + pad) + pad), (30, 24, 18))
    d = ImageDraw.Draw(o)
    for k, (im, label) in enumerate(ims):
        x, y = pad + (k % cols) * (w + pad), pad + (k // cols) * (h + lh + pad)
        o.paste(im, (x, y))
        d.text((x + 4, y + h + 5), label, fill=(232, 163, 23))
    o.save(path)
    print("wrote", os.path.relpath(path, REPO))


img = os.path.join(REPO, "docs/img")
sheet([("dash_radar_busy", "Radar dash"), ("dash_trail_later", "Trail Dash"),
       ("drive_mode", "Drive mode (rig says 40 mph)"), ("dash_allclear", "All Clear"),
       ("dash_allclear_later", "All Clear: heads up"), ("dash_trends_later", "Trends (last hour)"),
       ("dash_night_later", "Night Watch (dim red, LED off)"), ("home_poke", "Jerry (poke + a badge)"),
       ("listen_heard", "Listen: sound + knocks"), ("expedition_recap", "Expedition recap"),
       ("rate_pick_3", "Rate Gear"), ("rate_stamped", "5quatch Rated stamp"),
       ("badges_later", "Badges"), ("logbook", "Logbook"),
       ("clock_editing", "Settings: Clock"), ("settings_poweroff_ask", "Settings: Power off")],
      os.path.join(img, "screens.png"))
sheet([("home", "Home: Jerry + SAFE / BT / WiFi"), ("menu", "Menu"),
       ("tracker_alert", "Trackers: an alert"), ("follower_detail", "Followers: one that tagged along"),
       ("shield_after", "Shield: attacks caught"), ("cameras_after", "Cameras: Flock + hints"),
       ("nearby", "Nearby: Bluetooth + Wi-Fi"), ("nearby_detail_2", "Nearby: one network"),
       ("rig_dashboard", "Rig (Pi box)"), ("settings", "Settings"),
       ("diagnostics", "Diagnostics"), ("pocket_locked", "Pocket mode")],
      os.path.join(img, "modes.png"))

# the four intro title cards in a row
cards = [Image.open(shot(f"intro_{n}_end")).crop((0, 0, 384, 384))
         for n in ("jeep_jam", "bluff_creek", "selfie", "cruisin")]
o = Image.new("RGB", (4 * 384 + 30, 384), (30, 24, 18))
for k, c in enumerate(cards):
    o.paste(c, (k * 394, 0))
o.resize((o.width // 2, o.height // 2), Image.NEAREST).save(os.path.join(img, "intros.png"))
print("wrote docs/img/intros.png")
Image.open(shot("home")).crop((96, 60, 288, 252)).resize((64, 64), Image.LANCZOS).save(os.path.join(img, "favicon.png"))

prev = os.path.join(REPO, "docs/previews")
for name in glob.glob(os.path.join(OUT, "intro_*.gif")) + glob.glob(os.path.join(OUT, "intro_*.mp4")) + \
        glob.glob(os.path.join(OUT, "anim_*.gif")) + glob.glob(os.path.join(OUT, "anim_*.mp4")):
    shutil.copy(name, prev)
shutil.copy(os.path.join(OUT, "intro_jeep_jam.gif"), os.path.join(img, "intro.gif"))
print("copied the animations to docs/previews and docs/img/intro.gif")
