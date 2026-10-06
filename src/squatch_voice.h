// squatch_voice.h - Jerry's voice: Sasquatch sounds made from math, plus
// optional recorded voice clips.
//
// The classic Bigfoot repertoire, synthesized (no recordings needed):
//   whoop  - the rising-then-falling "whoooo-OOP" call
//   knock  - wood knock, a hollow thump
//   grunt  - low, buzzy huff / chuckle
//
// If a recorded clip exists for a sound (voice/*.wav -> tools/make_voice.py
// -> src/voice_clips.h), the clip plays instead. Several clips for the same
// sound take turns.
//
// Pure code (no hardware), so the PC simulator can render the same sounds
// to .wav files: tools/sim/render_voice.sh.
#pragma once
#include <stddef.h>
#include <stdint.h>

namespace audio {

enum class Sound : uint8_t {
  Boot,      // hello: a whoop + two knocks
  Happy,     // poke / saved: "huh-huh-hmm" chuckle
  RecOn,     // logging started: knock knock + grunt
  RecOff,    // logging stopped: "hmmph" + knock
  Alert,     // tracker following you: three urgent whoops
  LowBatt,   // tired, falling moan
  Tick,      // volume preview: one knock
  Erase,     // log erased: two low huffs
  COUNT
};

const char *soundName(Sound s);  // "boot", "happy", "rec_on"... (= voice clip file names)

class VoiceSynth {
 public:
  static constexpr int RATE = 16000;  // samples per second

  void start(Sound s);
  bool playing() const { return playing_; }
  // Fill up to n mono samples. Returns how many were written; 0 = finished.
  int render(int16_t *out, int n);

 private:
  enum class Kind : uint8_t { Rest, Whoop, Grunt, Knock };
  struct Segment {
    Kind kind;
    uint16_t ms;
    float f0, f1, f2;  // pitch: start, peak, end (Hz)
    float amp;         // 0..1
  };

  bool nextSegment();
  float sample();
  float noise();

  const Segment *segs_ = nullptr;
  int segCount_ = 0, segIndex_ = 0;
  const uint8_t *clip_ = nullptr;  // recorded clip, if one is playing
  uint32_t clipLen_ = 0, clipPos_ = 0;
  Segment seg_{};
  int pos_ = 0, len_ = 0;
  float phase_ = 0, lp_ = 0, rough_ = 0;
  uint32_t rng_ = 0x5A17C4u;
  bool playing_ = false;
};

}  // namespace audio
