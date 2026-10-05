#pragma once
// Host stand-in: the "screen" is just an off-screen image we can save.
#define LGFX_USE_V1
#include <LovyanGFX.hpp>
#include "board_pins.h"
class JerryDisplay : public LGFX_Sprite {
 public:
  bool init() { setColorDepth(16); return createSprite(128, 128) != nullptr; }
  void setBrightness(uint8_t b) { brightness = b; }
  uint8_t brightness = 0;
};
