// funk.cpp - see funk.h.
//
// Built from a 16th-note grid: each sample works out which step it's in,
// which notes / drums are sounding, and how far into them it is.

#include "funk.h"

#include <math.h>

namespace {

constexpr float PI2 = 6.28318530718f;
constexpr float BPM = 110;
constexpr float STEP_S = 60.0f / BPM / 4;  // one 16th note
constexpr int BARS = 2;
constexpr int STEPS = 16 * BARS;
constexpr float TAIL_S = 0.6f;             // let the last hit ring
constexpr float HP_A = 0.9733f;            // one-pole high-pass at ~70 Hz

float midiHz(int note) { return 440.0f * powf(2.0f, (note - 69) / 12.0f); }

// Bass line, E minor pentatonic. step, length (16ths), note, kind
// kind: 0 = normal, 1 = ghost (muted thump), 2 = slap pop (octave up, bright),
//       3 = slide down at the end ("bwoooow")
struct Note { uint8_t step, len, note, kind; };
const Note kBass[] = {
    // bar 1
    {0, 3, 40, 0}, {3, 1, 40, 1}, {4, 1, 52, 2}, {6, 1, 50, 0}, {7, 1, 52, 2},
    {8, 2, 43, 0}, {10, 1, 45, 0}, {11, 1, 46, 0}, {12, 2, 47, 0}, {14, 1, 50, 0}, {15, 1, 52, 2},
    // bar 2
    {16, 3, 40, 0}, {19, 1, 40, 1}, {20, 1, 52, 2}, {22, 1, 52, 2}, {23, 1, 50, 0},
    {24, 2, 45, 0}, {26, 1, 43, 0}, {27, 1, 40, 1}, {28, 4, 52, 3},
};

// Drums per 16th: K = kick, S = snare, h = closed hat, H = accented hat.
const char kKick[] = "K.....K...K.....K.....K..K......";
const char kSnare[] = "....S.......S.......S.......S...";
const char kHat[] = "H.h.H.h.H.h.HhhhH.h.H.h.H.h.Hhhh";

// Horn-ish stab (E9: E G# D F#) when the title drops in: bar 2, beat 2.
constexpr int STAB_STEP = 20;
const int kStab[] = {64, 68, 74, 78};

}  // namespace

uint32_t FunkIntro::lengthSamples() { return (uint32_t)((STEPS * STEP_S + TAIL_S) * RATE); }

void FunkIntro::start() {
  *this = FunkIntro();
  playing_ = true;
}

float FunkIntro::next() {
  const float t = (float)pos_ / RATE;
  const int step = (int)(t / STEP_S);
  const float inStep = t - step * STEP_S;
  float mix = 0;

  // --- Bass: two slightly detuned saws -> resonant low-pass with a fast
  // "envelope filter" sweep on each note (the Bootsy wah).
  const Note *cur = nullptr;
  for (const Note &n : kBass)
    if (step >= n.step && step < n.step + n.len) cur = &n;
  if (cur) {
    const float noteT = t - cur->step * STEP_S;
    const float noteLen = cur->len * STEP_S * (cur->kind == 1 ? 0.35f : 0.92f);
    float hz = midiHz(cur->note);
    if (cur->kind == 3) hz *= powf(2.0f, -noteT / (cur->len * STEP_S) * 1.0f);  // slide down an octave
    float amp = noteT < 0.004f ? noteT / 0.004f : expf(-(noteT) / 0.45f);
    if (noteT > noteLen) amp *= expf(-(noteT - noteLen) / 0.012f);              // quick release
    if (cur->kind == 1) amp *= 0.45f;                                          // ghost note
    if (cur->kind == 2) amp *= 0.85f;
    float saw = 0;
    for (int o = 0; o < 2; o++) {
      bassPhase_[o] += hz * (o ? 1.006f : 1.0f) / RATE;
      bassPhase_[o] -= floorf(bassPhase_[o]);
      saw += 2 * bassPhase_[o] - 1;
    }
    saw *= 0.5f;
    // envelope filter: cutoff jumps up on the attack and falls back
    const float sweep = expf(-noteT / (cur->kind == 2 ? 0.05f : 0.09f));
    const float base = cur->kind == 1 ? 180 : 220;
    const float peak = cur->kind == 2 ? 3200 : cur->kind == 1 ? 500 : 2100;
    const float cutoff = base + (peak - base) * sweep;
    const float f = 2 * sinf(3.1416f * cutoff / RATE);
    const float q = 0.22f;  // low damping = resonant "quack"
    svfLow_ += f * svfBand_;
    const float high = saw - svfLow_ - q * svfBand_;
    svfBand_ += f * high;
    mix += svfLow_ * amp * 0.9f;
  } else {
    svfLow_ *= 0.995f;
    svfBand_ *= 0.995f;
  }

  // --- Stab: E9 chord, brassy filtered saws, one short hit.
  const float stabT = t - STAB_STEP * STEP_S;
  if (stabT >= 0 && stabT < 0.5f) {
    float s = 0;
    for (int k = 0; k < 3; k++) {
      stabPhase_[k] += midiHz(kStab[k + 1]) / RATE;
      stabPhase_[k] -= floorf(stabPhase_[k]);
      s += 2 * stabPhase_[k] - 1;
    }
    const float cutoff = 900 + 2600 * expf(-stabT / 0.07f);
    const float f = 2 * sinf(3.1416f * cutoff / RATE);
    stabLow_ += f * stabBand_;
    stabBand_ += f * (s / 3 - stabLow_ - 0.5f * stabBand_);
    const float env = (stabT < 0.006f ? stabT / 0.006f : 1) * expf(-stabT / 0.16f);
    mix += stabLow_ * env * 0.55f;
  }

  // --- Drums
  if (step < STEPS) {
    if (kKick[step] == 'K') {  // a thump: sine falling from 120 Hz to 48 Hz
      const float hz = 48 + 72 * expf(-inStep / 0.025f);
      kickPhase_ += hz / RATE;
      kickPhase_ -= floorf(kickPhase_);
      mix += sinf(PI2 * kickPhase_) * expf(-inStep / 0.14f) * 0.75f;
    } else {
      kickPhase_ = 0;
    }
    rng_ ^= rng_ << 13;
    rng_ ^= rng_ >> 17;
    rng_ ^= rng_ << 5;
    const float noise = (int32_t)rng_ / 2147483648.0f;
    if (kSnare[step] == 'S')  // crack: noise + a little body
      mix += (noise * 0.7f + 0.3f * sinf(PI2 * 190 * inStep)) * expf(-inStep / 0.08f) * 0.55f;
    if (kHat[step] == 'h' || kHat[step] == 'H') {  // tick: high-passed noise
      const float hp = noise - hatPrev_;
      mix += hp * expf(-inStep / 0.018f) * (kHat[step] == 'H' ? 0.22f : 0.12f);
    }
    hatPrev_ = noise;
  }

  // Trim the deep rumble the little speaker can't play anyway (it only
  // strains the amp), then a lighter saturation than v1.2: punchy, not squashed.
  const float hp = HP_A * (hpOut_ + mix - hpIn_);
  hpIn_ = mix;
  hpOut_ = hp;
  return tanhf(hp * 1.1f) * 0.8f;
}

int FunkIntro::render(int16_t *out, int n) {
  if (!playing_) return 0;
  const uint32_t len = lengthSamples();
  int k = 0;
  for (; k < n && pos_ < len; k++, pos_++) {
    float v = next();
    const uint32_t left = len - pos_;
    if (left < RATE / 20) v *= (float)left / (RATE / 20);  // fade out the last 50 ms
    out[k] = (int16_t)(v * 32000);
  }
  if (pos_ >= len) playing_ = false;
  return k;
}
