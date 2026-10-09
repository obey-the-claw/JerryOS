// ui.cpp - see ui.h.

#include "ui.h"

#include "shell.h"
#include "theme.h"

using namespace theme;

namespace ui {

void text(lgfx::LovyanGFX &g, int x, int y, uint16_t col, const char *s) {
  g.setTextColor(col);
  g.setCursor(x, y);
  g.print(s);
}

void centered(lgfx::LovyanGFX &g, int y, uint16_t col, const char *s) {
  text(g, (W - g.textWidth(s)) / 2, y, col, s);
}

void drawList(lgfx::LovyanGFX &g, int y0, const char *const *labels, const char *const *values,
              int count, int cursor, bool editing) {
  const int rowH = 18, visible = (CONTENT_H - 4) / rowH;
  const int top = cursor < visible ? 0 : cursor - visible + 1;
  if (top + visible < count) g.fillTriangle(W / 2 - 3, y0 + CONTENT_H - 3, W / 2 + 3,
                                            y0 + CONTENT_H - 3, W / 2, y0 + CONTENT_H, DIM);
  for (int i = top; i < count && i < top + visible; i++) {
    const int y = y0 + 4 + (i - top) * rowH;
    const bool sel = i == cursor;
    if (sel) g.fillRoundRect(3, y, W - 6, rowH - 3, 3, editing ? CREAM : GOLD);
    const uint16_t fg = sel ? BARK : CREAM;
    text(g, 8, y + 4, fg, labels[i]);
    if (values && values[i]) {
      const char *v = values[i];
      const int vx = W - 8 - g.textWidth(v);
      if (sel && editing) {  // arrows show the value can be changed
        text(g, vx - 8, y + 4, fg, "<");
        text(g, vx, y + 4, fg, v);
      } else {
        text(g, vx, y + 4, sel ? BARK : GOLD, v);
      }
    }
  }
}

Safety safety(Shell &shell, uint32_t now) {
  if (shell.anyAlert()) return Safety::Alert;
  if (shell.tracker().anySeparatedNow(now) || shell.follow().flaggedCount() || shell.attacks().anyActive() ||
      shell.cameras().nearbyFlock(now) || shell.follow().candidates(now) >= 3)
    return Safety::Watch;
  return Safety::Safe;
}

int lostTrackersNow(Shell &shell, uint32_t now) {
  int n = 0;
  const TrackedDevice *list[TrackerWatch::MAX_DEVICES];
  for (int k = 0; k < TRACKER_KINDS; k++) {
    const int c = shell.tracker().listDevices((TrackerKind)k, now, list, TrackerWatch::MAX_DEVICES);
    for (int i = 0; i < c; i++)
      if (list[i]->separated && now - list[i]->lastSeenMs <= TrackerWatch::RECENT_MS) n++;
  }
  return n;
}

}  // namespace ui
