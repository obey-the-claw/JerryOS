// dashes.h - the home screen's dashes. Tap R on the home screen flips to the
// next one; Jerry remembers the last one (Settings -> Dash picks it too).
//
//   Jerry       his face + SAFE / BT / WiFi pills (the original home screen)
//   Radar       a sweeping radar: every Bluetooth device is a blip, closer =
//               nearer the middle; trackers / followers red, cameras gold
//   Trail Dash  a Jeep instrument cluster: speed (with the rig) or Bluetooth,
//               Wi-Fi and sound gauges, battery as the fuel gauge, warning lamps
//   All Clear   one big word (ALL CLEAR / HEADS UP / ALERT) + the counts
//   Trends      the last 10 minutes of Bluetooth, Wi-Fi and sound as graphs
//   Night Watch dim, all red, full screen: easy on night vision in the woods
#pragma once
#include <LovyanGFX.hpp>

class Shell;

enum class Dash : uint8_t { Jerry, Radar, Trail, AllClear, Trends, Night, COUNT };
constexpr int DASH_COUNT = (int)Dash::COUNT;

const char *dashName(Dash d);   // "Radar"
const char *dashShort(Dash d);  // "radar" (Settings)

// Draw one of the dashes (not Jerry: the home screen draws him). Night Watch
// draws over the whole screen; the rest into the content area.
void drawDash(Dash d, Shell &shell, lgfx::LovyanGFX &g, int y0, int h);
// Does this dash use the microphone (so the home screen keeps it on)?
bool dashWantsMic(Dash d);
