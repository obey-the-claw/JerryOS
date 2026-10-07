// sound_level.h - turns microphone samples into a calm, readable level.
//
// Pure logic (unit-tested on a PC). The microphone itself is in mic.cpp.
//
// The AIPI's microphone isn't calibrated, so the "dB" shown is a ROUGH
// estimate: SPL_OFFSET maps the chip's digital level to everyday dB. It can
// be tuned by comparing against a sound-meter app on a phone.
#pragma once
#include <stddef.h>
#include <stdint.h>

// Loudness of a block of samples, in dB below full scale (0 = maximum,
// -90 = silence). The average (DC offset) is removed first.
float blockDbfs(const int16_t *samples, size_t n);

// Everyday words for a rough dB level: "quiet", "talking", "loud"...
const char *loudnessName(int db);

class LevelMeter {
 public:
  static constexpr float SPL_OFFSET = 110.0f;  // rough dB = dBFS + this

  void add(float dbfs);   // one ~100 ms block
  int db() const { return (int)(smooth_ + SPL_OFFSET + 0.5f); }  // calm, rough dB
  int peak() const { return (int)(peak_ + SPL_OFFSET + 0.5f); }  // loudest since reset
  float dbfs() const { return smooth_; }
  void resetPeak() { peak_ = smooth_; }
  bool hasData() const { return blocks_ > 0; }

 private:
  float smooth_ = -90, peak_ = -90;
  uint32_t blocks_ = 0;
};
