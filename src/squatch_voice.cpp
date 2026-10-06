// squatch_voice.cpp - see squatch_voice.h.
//
// The speaker is tiny and can't really play deep notes, so every sound is
// built with plenty of overtones: the ear "fills in" the low pitch from them.

#include "squatch_voice.h"

#include <math.h>

#include "voice_clips.h"

namespace audio {
namespace {

constexpr float TWO_PI = 6.28318530718f;

}  // namespace

const char *soundName(Sound s) {
  static const char *const names[] = {"boot", "happy", "rec_on", "rec_off",
                                      "alert", "low_batt", "tick", "erase"};
  const int i = (int)s;
  return i < (int)Sound::COUNT ? names[i] : "?";
}

// Each sound is a short list of segments.
#define W(ms, a, b, c, amp) {Kind::Whoop, ms, a, b, c, amp}
#define G(ms, a, b, amp) {Kind::Grunt, ms, a, a, b, amp}
#define K(amp) {Kind::Knock, 110, 0, 0, 0, amp}
#define R(ms) {Kind::Rest, ms, 0, 0, 0, 0}

void VoiceSynth::start(Sound s) {
  static const Segment kBoot[] = {W(950, 380, 760, 430, 0.9f), R(140), K(1), R(170), K(1)};
  static const Segment kHappy[] = {G(120, 165, 150, 0.9f), R(70), G(110, 185, 165, 0.9f), R(60),
                                   G(190, 150, 125, 0.85f)};
  static const Segment kRecOn[] = {K(1), R(150), K(1), R(140), G(150, 150, 170, 0.8f)};
  static const Segment kRecOff[] = {G(260, 150, 105, 0.9f), R(120), K(0.9f)};
  static const Segment kAlert[] = {W(480, 520, 1150, 760, 1), R(110), W(480, 520, 1150, 760, 1),
                                   R(110), W(620, 520, 1200, 650, 1)};
  static const Segment kLowBatt[] = {W(1200, 360, 400, 210, 0.75f), R(80), G(300, 120, 85, 0.7f)};
  static const Segment kTick[] = {K(0.8f)};
  static const Segment kErase[] = {G(240, 130, 110, 0.85f), R(110), G(320, 110, 80, 0.85f)};
  static const struct { const Segment *s; int n; } kTable[] = {
      {kBoot, 5}, {kHappy, 5}, {kRecOn, 5}, {kRecOff, 3},
      {kAlert, 5}, {kLowBatt, 3}, {kTick, 1}, {kErase, 3},
  };

  // A recorded clip wins, if there is one. Several clips take turns.
  static uint8_t turn[(int)Sound::COUNT] = {};
  clip_ = nullptr;
  int matches = 0;
  for (int i = 0; i < kVoiceClipCount; i++)
    if (kVoiceClips[i].sound == (uint8_t)s) matches++;
  if (matches) {
    int want = turn[(int)s]++ % matches;
    for (int i = 0; i < kVoiceClipCount; i++)
      if (kVoiceClips[i].sound == (uint8_t)s && want-- == 0) {
        clip_ = kVoiceClips[i].pcm;
        clipLen_ = kVoiceClips[i].len;
        clipPos_ = 0;
      }
    playing_ = true;
    return;
  }

  const int i = (int)s < (int)Sound::COUNT ? (int)s : (int)Sound::Tick;
  segs_ = kTable[i].s;
  segCount_ = kTable[i].n;
  segIndex_ = 0;
  playing_ = nextSegment();
}

#undef W
#undef G
#undef K
#undef R

bool VoiceSynth::nextSegment() {
  if (segIndex_ >= segCount_) return false;
  seg_ = segs_[segIndex_++];
  pos_ = 0;
  len_ = RATE * seg_.ms / 1000;
  phase_ = 0;
  return true;
}

float VoiceSynth::noise() {  // quick pseudo-random number, -1..1
  rng_ ^= rng_ << 13;
  rng_ ^= rng_ >> 17;
  rng_ ^= rng_ << 5;
  return (int32_t)rng_ / 2147483648.0f;
}

float VoiceSynth::sample() {
  const float t = (float)pos_ / len_;           // 0..1 through the segment
  const float sec = (float)pos_ / RATE;
  switch (seg_.kind) {
    case Kind::Rest:
      return 0;

    case Kind::Whoop: {
      // Pitch glides up to the peak (first 40%), then falls away.
      float f;
      if (t < 0.4f) {
        const float u = t / 0.4f;
        f = seg_.f0 + (seg_.f1 - seg_.f0) * u * u * (3 - 2 * u);
      } else {
        const float u = (t - 0.4f) / 0.6f;
        f = seg_.f1 + (seg_.f2 - seg_.f1) * u * u * (3 - 2 * u);
      }
      f *= 1 + 0.025f * sinf(TWO_PI * 5.5f * sec);  // throaty wobble
      phase_ += f / RATE;
      if (phase_ >= 1) phase_ -= 1;
      const float p = TWO_PI * phase_;
      const float v = sinf(p) + 0.45f * sinf(2 * p) + 0.22f * sinf(3 * p) + 0.08f * noise();
      // Swell in, hold, fade out.
      const float env = (t < 0.12f ? t / 0.12f : 1) * (t > 0.7f ? (1 - t) / 0.3f : 1);
      return v * env * 0.6f;
    }

    case Kind::Grunt: {
      // A buzzy sawtooth at a low pitch, roughened and softened: "hmph".
      const float f = seg_.f0 + (seg_.f2 - seg_.f0) * t;
      phase_ += f / RATE;
      if (phase_ >= 1) phase_ -= 1;
      rough_ += 0.02f * (noise() - rough_);          // slow random wobble
      float v = (2 * phase_ - 1) * (1 + 1.5f * rough_);
      lp_ += 0.35f * (v - lp_);                      // take the harsh edge off
      v = lp_ + 0.12f * noise();
      const float env = (t < 0.08f ? t / 0.08f : 1) * (1 - t) * (1 - t) * 1.6f;
      return v * (env > 1 ? 1 : env) * 0.8f;
    }

    case Kind::Knock: {
      // Wood knock: a hollow thump (two decaying tones) plus a sharp click.
      const float body = sinf(TWO_PI * 190 * sec) * expf(-sec / 0.030f) +
                         0.7f * sinf(TWO_PI * 470 * sec) * expf(-sec / 0.016f) +
                         0.35f * sinf(TWO_PI * 1150 * sec) * expf(-sec / 0.006f);
      const float click = sec < 0.004f ? noise() * (1 - sec / 0.004f) : 0;
      return (body * 0.45f + click * 0.4f) * (1 - t);
    }
  }
  return 0;
}

int VoiceSynth::render(int16_t *out, int n) {
  if (!playing_) return 0;
  int k = 0;
  if (clip_) {  // recorded clip: 8-bit samples centred on 128
    while (k < n && clipPos_ < clipLen_) out[k++] = (int16_t)(((int)clip_[clipPos_++] - 128) * 256);
    if (clipPos_ >= clipLen_) playing_ = false;
    return k;
  }
  while (k < n) {
    if (pos_ >= len_ && !nextSegment()) {
      playing_ = false;
      break;
    }
    const float v = sample() * seg_.amp;
    pos_++;
    const float c = v > 1 ? 1 : v < -1 ? -1 : v;
    out[k++] = (int16_t)(c * 31000);
  }
  return k;
}

}  // namespace audio
