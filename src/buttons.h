// buttons.h - debounced buttons with short-press / long-press events.
//
// How it works, in plain terms:
//  - A button counts as "pressed" only after it has read the same for
//    DEBOUNCE_MS (metal contacts bounce for a few ms when pushed).
//  - Let go before LONG_MS  -> one Short event (fired on release).
//  - Keep holding past LONG_MS -> one Long event (fired while still held),
//    and no Short event when you finally let go.
#pragma once
#include <Arduino.h>

enum class ButtonEvent { None, Short, Long };

class Button {
 public:
  static constexpr uint32_t DEBOUNCE_MS = 30;
  static constexpr uint32_t LONG_MS = 700;

  explicit Button(int pin) : pin_(pin) {}

  void begin() { pinMode(pin_, INPUT_PULLUP); }

  // Call often (every loop). Returns at most one event per call.
  ButtonEvent update() {
    const uint32_t now = millis();
    const bool raw = digitalRead(pin_) == LOW;  // active-low

    if (raw != lastRaw_) {
      lastRaw_ = raw;
      lastChange_ = now;
    }
    if (now - lastChange_ < DEBOUNCE_MS || raw == pressed_) {
      if (pressed_ && !longFired_ && now - pressStart_ >= LONG_MS) {
        longFired_ = true;
        return ButtonEvent::Long;
      }
      return ButtonEvent::None;
    }

    pressed_ = raw;  // stable state changed
    if (pressed_) {
      pressStart_ = now;
      longFired_ = false;
      return ButtonEvent::None;
    }
    return longFired_ ? ButtonEvent::None : ButtonEvent::Short;
  }

  bool isPressed() const { return pressed_; }
  uint32_t heldMs() const { return pressed_ ? millis() - pressStart_ : 0; }
  bool rawLevelLow() const { return digitalRead(pin_) == LOW; }

 private:
  int pin_;
  bool lastRaw_ = false;
  bool pressed_ = false;
  bool longFired_ = false;
  uint32_t lastChange_ = 0;
  uint32_t pressStart_ = 0;
};
