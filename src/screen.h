// screen.h - what every JerryOS screen (home, menu, each mode) looks like.
//
// The shell owns the buttons, status bar and hint bar. A screen only says:
//   - its short name for the status bar     (tag)
//   - what the buttons do right now         (hints)
//   - how to react to a press               (onLeftShort / onRightShort / onRightLong)
//   - how to draw its content area          (draw)
//
// LEFT long-press is handled by the shell (always "go home"), so screens
// never need to think about it.
//
// Adding a mode later (e.g. the real Rig Dashboard) = write one Screen
// subclass and list it in the menu in screens.cpp. Nothing else changes.
#pragma once
#include <LovyanGFX.hpp>

class Shell;

// Labels for the hint bar. nullptr = don't show that button.
struct Hints {
  const char *tapL;   // LEFT short press
  const char *tapR;   // RIGHT short press
  const char *holdR;  // RIGHT long press
};

class Screen {
 public:
  virtual ~Screen() = default;

  virtual const char *tag() const = 0;  // e.g. "HOME", shown top-left
  virtual Hints hints() const = 0;

  virtual void enter(Shell &) {}        // called each time the screen is shown
  virtual void onLeftShort(Shell &shell);  // default: go back
  virtual void onRightShort(Shell &) {}
  virtual void onRightLong(Shell &) {}

  virtual void update(Shell &, uint32_t /*now*/) {}
  // true = draw over the whole screen, without the status and hint bars.
  virtual bool fullScreen() const { return false; }
  // Draw into the content area: full width, from y0, h pixels tall.
  virtual void draw(Shell &shell, lgfx::LovyanGFX &g, int y0, int h) = 0;
};
