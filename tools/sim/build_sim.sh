#!/bin/bash
# Run JerryOS's real shell + screens on a Linux PC (or WSL) with simulated
# buttons, and save a screenshot after each step to tools/sim/out/.
#
# Usage:  tools/sim/build_sim.sh /path/to/LovyanGFX
#   (git clone --branch 1.2.7 https://github.com/lovyan03/LovyanGFX.git)
# Needs: g++, gcc, python3 with Pillow (pip install pillow) for the contact sheet.
set -e
SIM=$(cd "$(dirname "$0")" && pwd)
REPO=$(cd "$SIM/../.." && pwd)
L=${1:?give the path to a LovyanGFX checkout}/src
OUT=$SIM/out
rm -rf "$OUT" && mkdir -p "$OUT/src" "$OUT/obj" "$OUT/shots" "$OUT/intro"

# Firmware sources, with the real display driver swapped for an off-screen image.
cp "$REPO"/src/*.h "$REPO"/src/*.cpp "$REPO"/include/*.h "$OUT/src/"
rm "$OUT/src/main.cpp" "$OUT/src/ble_scanner.cpp" "$OUT/src/wifi_scanner.cpp" "$OUT/src/status_led_hw.cpp" "$OUT/src/mic.cpp" \
  "$OUT/src/speaker.cpp" "$OUT/src/es8311.cpp"
cp "$SIM/ble_scanner_sim.cpp" "$SIM/wifi_scanner_sim.cpp" "$SIM/status_led_sim.cpp" "$SIM/mic_sim.cpp" \
  "$SIM/speaker_sim.cpp" "$OUT/src/"
cp "$SIM/display_host.h" "$OUT/src/display.h"

# LovyanGFX's C parts (fonts, image decoders).
for f in "$L"/lgfx/Fonts/efont/*.c "$L"/lgfx/Fonts/IPA/*.c $(find "$L/lgfx/utility" -name '*.c'); do
  gcc -c -O1 -w -I "$L" "$f" -o "$OUT/obj/$(basename "$f").o"
done

g++ -std=gnu++17 -O1 -w -DLGFX_LINUX_FB -I "$SIM" -I "$OUT/src" -I "$L" \
  "$SIM/sim_main.cpp" "$OUT"/src/*.cpp \
  "$L"/lgfx/v1/LGFX_Sprite.cpp "$L"/lgfx/v1/lgfx_fonts.cpp "$L"/lgfx/v1/LGFXBase.cpp \
  "$L"/lgfx/v1/misc/*.cpp "$L"/lgfx/v1/platforms/framebuffer/*.cpp \
  "$L"/lgfx/v1/panel/Panel_HasBuffer.cpp "$L"/lgfx/v1/panel/Panel_FrameBufferBase.cpp \
  "$L"/lgfx/v1/panel/Panel_Device.cpp $(find "$L/lgfx/utility" -name '*.cpp') \
  "$OUT"/obj/*.o -o "$OUT/sim" -lpthread -lz

cd "$OUT" && ./sim
python3 - <<'PY'
from PIL import Image
import glob
fs = sorted(glob.glob('shots/*.ppm'))
ims = [Image.open(f) for f in fs]
for f, i in zip(fs, ims): i.save(f.replace('.ppm', '.png'))
cols, pad = 6, 8
w, h = ims[0].size
rows = (len(ims) + cols - 1) // cols
sheet = Image.new('RGB', (cols * (w + pad), rows * (h + pad)), (60, 60, 60))
for k, i in enumerate(ims): sheet.paste(i, ((k % cols) * (w + pad), (k // cols) * (h + pad)))
sheet.save('screens.png')
print(f"{len(fs)} screenshots -> tools/sim/out/screens.png")
frames = [Image.open(f).resize((384, 384), Image.NEAREST) for f in sorted(glob.glob('intro/*.ppm'))]
if frames:
    frames[0].save('intro.gif', save_all=True, append_images=frames[1:], duration=50, loop=0)
    print(f"{len(frames)} intro frames -> tools/sim/out/intro.gif")
PY
