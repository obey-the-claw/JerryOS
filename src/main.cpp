// JerryOS - Jerry the Squatch's pocket / dash sidekick for the AIPI Lite.
//
// Checkpoint 3: the JerryOS shell. Home screen with Jerry, menu, hint bar,
// status bar and mode switching. Jerry is placeholder art drawn in code.
//
// Where things live:
//   include/board_pins.h  every pin
//   src/shell.*           screen stack, buttons, status + hint bars
//   src/screens.cpp       home, menu and each mode
//   src/jerry_face.*      Jerry and his moods
//   src/settings.h        saved settings
//   src/power.h           power hold, battery, USB, charging
//   src/theme.h           brand colours, layout, version

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
