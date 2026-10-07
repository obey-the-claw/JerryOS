// theme.h - brand colours, screen layout and version, in one place.
#pragma once
#include <LovyanGFX.hpp>

#define JERRYOS_VERSION "0.12.0"

namespace theme {

// 5quatch Rated brand colours.
constexpr uint16_t BARK = lgfx::color565(0x2a, 0x1a, 0x0e);   // background
constexpr uint16_t FUR = lgfx::color565(0xa8, 0x73, 0x5f);    // Jerry's fur
constexpr uint16_t GOLD = lgfx::color565(0xe8, 0xa3, 0x17);   // highlights
constexpr uint16_t CREAM = lgfx::color565(0xf7, 0xf0, 0xe4);  // text
constexpr uint16_t GRIN = lgfx::color565(0xf4, 0x51, 0x1e);   // Jerry's grin

// Supporting shades.
constexpr uint16_t FUR_DARK = lgfx::color565(0x7a, 0x4e, 0x3c);
constexpr uint16_t FUR_LIGHT = lgfx::color565(0xc9, 0x9b, 0x84);
constexpr uint16_t NOSE = lgfx::color565(0x5a, 0x33, 0x22);
constexpr uint16_t INK = lgfx::color565(0x10, 0x0a, 0x06);    // brows, pupils, outlines
constexpr uint16_t BAR = lgfx::color565(0x45, 0x2c, 0x1a);    // status / hint bars
constexpr uint16_t DIM = lgfx::color565(0x9a, 0x8a, 0x78);    // secondary text
constexpr uint16_t ALERT = lgfx::color565(0xe5, 0x2b, 0x2b);
constexpr uint16_t GOOD = lgfx::color565(0x4c, 0xc2, 0x5a);

// Layout (128 x 128 screen).
constexpr int W = 128;
constexpr int H = 128;
constexpr int STATUS_H = 12;               // top: mode indicator + battery
constexpr int HINT_H = 12;                 // bottom: what the buttons do
constexpr int CONTENT_Y = STATUS_H;        // the area each screen draws in
constexpr int CONTENT_H = H - STATUS_H - HINT_H;

}  // namespace theme
