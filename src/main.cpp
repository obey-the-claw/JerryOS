// JerryOS - Jerry the Squatch's pocket / dash sidekick for the AIPI Lite.
//
// v1.0: a personal, on-the-spot monitor. Listen-only: trackers, followers,
// radio attacks, Flock / hidden cameras, Bluetooth + Wi-Fi around you, a
// sound meter, and the Pi Zero rig box. Saves only when you press REC.
// How to use it: docs/USER-GUIDE.md.
//
// Where things live:
//   include/board_pins.h   every pin
//   src/shell.*            screen stack, buttons, pocket mode, bars, LED, alerts
//   src/screens.cpp        home, menu and every mode
//   src/jerry_face.*       Jerry and his moods
//   src/settings.h         saved settings
//   src/power.h            power hold, battery, USB, charging
//   src/theme.h            brand colours, layout, version
//   radios:  ble_scanner.cpp (Bluetooth), wifi_scanner.cpp (Wi-Fi + log), mic.cpp
//   brains:  tracker_*, follow_watch, attack_watch, camera_watch, ble_nearby,
//            wifi_nets, rig_link, sound_level, status_led  (all unit-tested on a PC)

#include <Arduino.h>

#include "display.h"
#include "screens.h"
#include "shell.h"
#include "theme.h"

static JerryDisplay lcd;
static Shell shell(lcd, homeScreen());

void setup() {
  power::holdOn();  // before anything else: stay powered on battery
  Serial.begin(115200);
  delay(300);
  Serial.println();
  Serial.printf("JerryOS %s | %s rev %d | PSRAM %u KB\n", JERRYOS_VERSION, ESP.getChipModel(),
                ESP.getChipRevision(), ESP.getPsramSize() / 1024);
  shell.begin();
}

void loop() { shell.loop(); }
