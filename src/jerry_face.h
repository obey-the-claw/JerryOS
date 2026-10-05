// jerry_face.h - Jerry the Squatch, Chief Gear Inspector.
//
// Placeholder art drawn entirely in code, so the UI works before the real
// PNG art exists. At Checkpoint 5 the drawing inside JerryFace gets swapped
// for Steve's art; the moods and the API stay the same, so no screen has to
// change.
//
// Moods are what Jerry *feels*; blinking and glancing are animation layered
// on top by update().
#pragma once
#include <LovyanGFX.hpp>

enum class Mood : uint8_t {
  Idle,      // relaxed, glancing around
  Scanning,  // thinking / looking for something: eyes sweep side to side
  Alert,     // tracker found: wide eyes, open mouth
  Approved,  // all clear: happy squint, big grin
  Sleepy,    // low battery or bored: droopy lids, floating z's
};

const char *moodName(Mood m);

class JerryFace {
 public:
  void setMood(Mood m);
  Mood mood() const { return flashUntil_ ? flashMood_ : mood_; }

  // Show a mood briefly (e.g. Approved when poked), then go back.
  void flash(Mood m, uint32_t ms);

  // Advance blink / glance / sweep animation. Call every frame.
  void update(uint32_t now);

  // Draw centred at (cx, cy). scale 1.0 = head about 104 px across.
  // shoulders = also draw his big shaggy neck and shoulders below the head
  // (home screen). Off for the small heads next to text.
  void draw(lgfx::LovyanGFX &g, int cx, int cy, float scale, bool shoulders = false) const;

 private:
  void pickNextGlance(uint32_t now);

  Mood mood_ = Mood::Idle;
  Mood flashMood_ = Mood::Idle;
  uint32_t flashUntil_ = 0;

  // Animation state (pupil offset is in face units, -1..1).
  float lookX_ = -0.6f, lookY_ = 0.5f;      // current (default: down-left)
  float targetX_ = -0.6f, targetY_ = 0.5f;  // where the eyes are heading
  uint32_t nextGlance_ = 0;
  uint32_t nextBlink_ = 0;
  uint32_t blinkEnd_ = 0;
  bool doubleBlink_ = false;
  uint32_t now_ = 0;
};
