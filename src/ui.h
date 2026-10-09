// ui.h - small drawing helpers and status checks shared by the screens and
// the home dashes.
#pragma once
#include <LovyanGFX.hpp>

class Shell;

namespace ui {

void text(lgfx::LovyanGFX &g, int x, int y, uint16_t col, const char *s);
void centered(lgfx::LovyanGFX &g, int y, uint16_t col, const char *s);
// A vertical list with a gold highlight bar on the selected row. Shows 5
// rows at a time and scrolls to keep the selected one on screen.
void drawList(lgfx::LovyanGFX &g, int y0, const char *const *labels, const char *const *values,
              int count, int cursor, bool editing);

// Safety at a glance, the same everywhere (home pills, dashes, LED):
//   SAFE  nothing going on
//   WATCH something worth a look (lost tracker, follower, attack, Flock camera...)
//   ALERT an alert is waiting
enum class Safety : uint8_t { Safe, Watch, Alert };
Safety safety(Shell &shell, uint32_t now);

// A lost tracker heard in the last 2 minutes, of any kind.
int lostTrackersNow(Shell &shell, uint32_t now);

}  // namespace ui
