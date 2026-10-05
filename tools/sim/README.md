# JerryOS simulator

Runs the **real** JerryOS shell and screens on a PC (Linux or WSL), with
fake buttons, and saves a screenshot after each step. We use it to check new
screens and Jerry's faces *before* flashing the device.

```bash
git clone --branch 1.2.7 https://github.com/lovyan03/LovyanGFX.git /tmp/LovyanGFX
tools/sim/build_sim.sh /tmp/LovyanGFX
# -> tools/sim/out/screens.png (contact sheet) + out/shots/*.png
```

- `sim_main.cpp`: the button script (tap/hold LEFT/RIGHT, wait, snapshot).
  Edit it to walk through new screens.
- `Arduino.h`, `Preferences.h`: tiny stand-ins for the Arduino APIs JerryOS uses.
- `display_host.h`: replaces the real screen driver with an off-screen image.

Set `SIMLOG=1` to also see the serial log JerryOS would print.
