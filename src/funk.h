// funk.h - Jerry's boot grooves. Each intro has its own:
//   0 Jeep Jam     Bootsy "Space Bass" (envelope-filter "bwow"), slap pops
//   1 Bluff Creek  slow, sneaky half-time creeper with a wah guitar
//   2 Selfie       a bouncy strut: slap octaves, claps, scratch guitar
//   3 Cruisin'     four-on-the-floor disco funk, octave bass, open hats
// All synthesized in code on a 16th-note grid (no sound files), 4-6 s each.
//
// Pure code (no hardware): the PC renders the exact same samples to .wav
// files for previewing (tools/sim/render_funk.sh) before they go on the device.
#pragma once
#include <stddef.h>
#include <stdint.h>

class FunkIntro {
 public:
  static constexpr int RATE = 16000;  // samples per second (what the AIPI plays)
  static constexpr int GROOVES = 4;

  static const char *name(int groove);       // "Jeep Jam", ...
  static uint32_t lengthSamples(int groove);  // how long a groove plays

  void start(int groove = 0);
  bool playing() const { return playing_; }
  // Fill up to n mono samples; returns how many (0 = the groove is over).
  int render(int16_t *out, int n);

 private:
  float next();
  int groove_ = 0;
  float chordHz_[3] = {};  // worked out once in start(), not 16,000 times a second
  uint32_t pos_ = 0;
  bool playing_ = false;
  // bass voice
  float bassPhase_[2] = {}, svfLow_ = 0, svfBand_ = 0;
  // stab and guitar filters
  float stabLow_ = 0, stabBand_ = 0, gtrLow_ = 0, gtrBand_ = 0;
  // drums
  float hatPrev_ = 0;
  uint32_t rng_ = 0x1234567u;
  // output high-pass
  float hpIn_ = 0, hpOut_ = 0;
};
