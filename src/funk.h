// funk.h - Jerry's boot groove: a Bootsy-style "Space Bass" line (sawtooth
// bass through an envelope filter: that "bwow" auto-wah), slap octave pops
// and ghost notes, over a funk drum pattern. Two bars at 110 BPM (~4.4 s),
// timed to the boot intro, with a stab when "JerryOS" drops in.
//
// Pure code (no hardware): the PC renders the exact same samples to a .wav
// for previewing (tools/sim/render_funk.sh) before it goes on the device.
#pragma once
#include <stddef.h>
#include <stdint.h>

class FunkIntro {
 public:
  static constexpr int RATE = 16000;  // samples per second (what the AIPI plays)

  void start();
  bool playing() const { return playing_; }
  // Fill up to n mono samples; returns how many (0 = the groove is over).
  int render(int16_t *out, int n);
  static uint32_t lengthSamples();

 private:
  float next();
  uint32_t pos_ = 0;
  bool playing_ = false;
  // bass voice
  float bassPhase_[2] = {}, svfLow_ = 0, svfBand_ = 0;
  // stab voice
  float stabPhase_[3] = {}, stabLow_ = 0, stabBand_ = 0;
  // drums
  float kickPhase_ = 0, hatPrev_ = 0;
  // output high-pass
  float hpIn_ = 0, hpOut_ = 0;
  uint32_t rng_ = 0x1234567u;
};
