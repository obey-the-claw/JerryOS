// sound_level.h - turns microphone samples into a calm, readable level.
//
// Pure logic (unit-tested on a PC). The microphone itself is in mic.cpp.
//
// The AIPI's microphone isn't calibrated, so the "dB" shown is a ROUGH
// estimate: SPL_OFFSET maps the chip's digital level to everyday dB, and
// Settings -> Sound cal adds a correction you set against a phone app.
// (v1.0 used +110, which read about double on Steve's device; v1.0.1 uses +80.)
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
  static constexpr float SPL_OFFSET = 80.0f;  // rough dB = dBFS + this + calibration

  void add(float dbfs);   // one ~100 ms block
  void setCalibration(int db) { cal_ = db; }  // Settings -> Sound cal
  int db() const { return toDb(smooth_); }    // calm, rough dB
  int peak() const { return toDb(peak_); }    // loudest since reset
  float dbfs() const { return smooth_; }
  void resetPeak() { peak_ = smooth_; }
  bool hasData() const { return blocks_ > 0; }

 private:
  int toDb(float dbfs) const {
    const int d = (int)(dbfs + SPL_OFFSET + cal_ + (dbfs + SPL_OFFSET + cal_ >= 0 ? 0.5f : -0.5f));
    return d < 0 ? 0 : d;
  }
  float smooth_ = -90, peak_ = -90;
  int cal_ = 0;
  uint32_t blocks_ = 0;
};
