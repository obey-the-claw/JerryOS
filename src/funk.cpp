// funk.cpp - see funk.h.
//
// Each groove is data: tempo, a bass line, and one character per 16th note for
// each instrument. Every sample works out which step it's in, which notes and
// hits are sounding, and how long ago they started.

#include "funk.h"

#include <math.h>
#include <string.h>

namespace {

constexpr float PI2 = 6.28318530718f;
constexpr float HP_A = 0.9733f;  // output one-pole high-pass at ~70 Hz

float midiHz(int note) { return 440.0f * powf(2.0f, (note - 69) / 12.0f); }
float saw(float hz, float t) {  // a sawtooth started at t = 0
  const float p = hz * t;
  return 2 * (p - floorf(p)) - 1;
}

// Bass note: step, length (16ths), MIDI note, kind
// kind: 0 = normal, 1 = ghost (muted thump), 2 = slap pop (octave up, bright),
//       3 = slide down an octave ("bwoooow")
struct Note { uint8_t step, len, note, kind; };

// One groove. Drum / guitar / stab patterns have one character per 16th:
//   kick  K           snare  S hit, g ghost, C handclap
//   hat   h closed, H accent, O open     gtr  x chicken-scratch, X wah chord
//   stab  X long horn hit, x short
struct Groove {
  const char *name;
  float bpm;
  int bars;
  float swing;  // how late the off 16ths land (fraction of a 16th)
  const Note *bass;
  int nBass;
  float bassPeak, bassDecay;  // envelope-filter top (Hz), note decay (s)
  const char *kick, *snare, *hat, *gtr, *stab;
  uint8_t chord[3];  // stab + guitar chord (MIDI)
  float gain;        // balances the grooves' loudness
};

// --- 0. Jeep Jam: E minor pentatonic Space Bass (v1.2's groove) -----------
const Note kJeepBass[] = {
    {0, 3, 40, 0}, {3, 1, 40, 1}, {4, 1, 52, 2}, {6, 1, 50, 0}, {7, 1, 52, 2},
    {8, 2, 43, 0}, {10, 1, 45, 0}, {11, 1, 46, 0}, {12, 2, 47, 0}, {14, 1, 50, 0}, {15, 1, 52, 2},
    {16, 3, 40, 0}, {19, 1, 40, 1}, {20, 1, 52, 2}, {22, 1, 52, 2}, {23, 1, 50, 0},
    {24, 2, 45, 0}, {26, 1, 43, 0}, {27, 1, 40, 1}, {28, 4, 52, 3},
};

// --- 1. Bluff Creek: creeping chromatic bass, half-time, wah scratches ----
const Note kCreekBass[] = {
    {0, 2, 40, 0}, {3, 1, 40, 1}, {4, 1, 43, 0}, {6, 1, 44, 0}, {7, 2, 45, 0},
    {10, 1, 40, 1}, {11, 1, 47, 0}, {12, 2, 46, 0}, {14, 1, 45, 0}, {15, 1, 43, 0},
    {16, 2, 40, 0}, {19, 1, 40, 1}, {20, 1, 43, 0}, {22, 1, 44, 0}, {23, 2, 45, 0},
    {26, 1, 52, 2}, {27, 1, 50, 0}, {28, 4, 47, 3},
};

// --- 2. Selfie: A, slap octaves popping all over ---------------------------
const Note kSelfieBass[] = {
    {0, 1, 45, 0}, {1, 1, 57, 2}, {2, 1, 45, 1}, {3, 1, 57, 2}, {4, 2, 55, 0}, {6, 1, 52, 0},
    {7, 1, 55, 2}, {8, 1, 45, 0}, {9, 1, 57, 2}, {10, 1, 45, 1}, {11, 1, 57, 2}, {12, 1, 60, 2},
    {13, 1, 57, 2}, {14, 1, 55, 0}, {15, 1, 52, 0},
    {16, 1, 45, 0}, {17, 1, 57, 2}, {18, 1, 45, 1}, {19, 1, 57, 2}, {20, 2, 55, 0}, {22, 1, 52, 0},
    {23, 1, 55, 2}, {24, 1, 48, 0}, {25, 1, 60, 2}, {26, 1, 50, 0}, {27, 1, 62, 2}, {28, 4, 57, 3},
};

// --- 3. Cruisin': G minor disco octaves ------------------------------------
const Note kCruiseBass[] = {
    {0, 1, 43, 0}, {2, 1, 55, 2}, {4, 1, 43, 0}, {6, 1, 55, 2},
    {8, 1, 41, 0}, {10, 1, 53, 2}, {12, 1, 46, 0}, {13, 1, 46, 1}, {14, 1, 58, 2}, {15, 1, 57, 0},
    {16, 1, 43, 0}, {18, 1, 55, 2}, {20, 1, 43, 0}, {22, 1, 55, 2},
    {24, 1, 48, 0}, {26, 1, 60, 2}, {27, 1, 58, 0}, {28, 4, 55, 3},
};

#define N(a) (int)(sizeof(a) / sizeof((a)[0]))
const Groove kGrooves[FunkIntro::GROOVES] = {
    {"Jeep Jam", 110, 2, 0, kJeepBass, N(kJeepBass), 2100, 0.45f,
     "K.....K...K.....K.....K..K......",
     "....S.......S.......S.......S...",
     "H.h.H.h.H.h.HhhhH.h.H.h.H.h.Hhhh",
     nullptr,
     "....................X...........",
     {68, 74, 78}, 1.0f},  // E9 (G# D F#) when "JerryOS" drops in
    {"Bluff Creek", 80, 2, 0.22f, kCreekBass, N(kCreekBass), 1500, 0.35f,
     "K.........K.....K..K......K.....",
     "..g.....S..g....g.......S..g.g..",
     "h.h.h.h.h.h.h.O.h.h.h.h.h.h.hhO.",
     "..x...x...x...X...x...x.........",
     "....................X...........",
     {55, 59, 66}, 1.15f},  // Em9-ish (G B F#): spooky
    {"Selfie", 100, 2, 0.12f, kSelfieBass, N(kSelfieBass), 2400, 0.3f,
     "K..K..K...K.K...K..K..K...K.....",
     "....C..g....C.g.....C..g....C...",
     "hhHhhhHhhhHhhhHhhhHhhhHhhhHhhhOh",
     ".x.x...x.x.x...x.x.x...x........",
     "............................X...",
     {61, 64, 67}, 1.0f},  // A7 (C# E G) for the grin
    {"Cruisin'", 112, 2, 0, kCruiseBass, N(kCruiseBass), 1900, 0.3f,
     "K...K...K...K...K...K...K...K...",
     "....C.......C.......C.......C...",
     "hhOhhhOhhhOhhhOhhhOhhhOhhhOhhhOh",
     "..x...x...x...x...x...x...x.....",
     "X.......................X.......",
     {58, 62, 69}, 0.95f},  // Gm9 (Bb D A) string stab
};

const Groove &G(int i) { return kGrooves[i >= 0 && i < FunkIntro::GROOVES ? i : 0]; }
constexpr float TAIL_S = 0.6f;  // let the last hit ring

float stepLen(const Groove &g) { return 60.0f / g.bpm / 4; }
int steps(const Groove &g) { return 16 * g.bars; }
float stepStart(const Groove &g, int s) { return (s + ((s & 1) ? g.swing : 0)) * stepLen(g); }

// The latest hit in `pat` at or before `step` (looking back up to `back`
// steps): its character, and how long ago it started.
char lastHit(const Groove &g, const char *pat, int step, float t, int back, float &ago) {
  if (!pat) return 0;
  for (int s = step < steps(g) ? step : steps(g) - 1, k = 0; s >= 0 && k <= back; s--, k++)
    if (pat[s] != '.') {
      ago = t - stepStart(g, s);
      return pat[s];
    }
  return 0;
}

}  // namespace

const char *FunkIntro::name(int groove) { return G(groove).name; }

uint32_t FunkIntro::lengthSamples(int groove) {
  const Groove &g = G(groove);
  return (uint32_t)((steps(g) * stepLen(g) + TAIL_S) * RATE);
}

void FunkIntro::start(int groove) {
  *this = FunkIntro();
  groove_ = groove >= 0 && groove < GROOVES ? groove : 0;
  for (int k = 0; k < 3; k++) chordHz_[k] = midiHz(G(groove_).chord[k]);
  playing_ = true;
}

float FunkIntro::next() {
  const Groove &g = G(groove_);
  const float t = (float)pos_ / RATE;
  int step = (int)(t / stepLen(g));
  if (step > 0 && t < stepStart(g, step)) step--;  // a swung step that hasn't landed yet
  float mix = 0;

  // --- Bass: two slightly detuned saws -> resonant low-pass with a fast
  // "envelope filter" sweep on each note (the Bootsy wah).
  const Note *cur = nullptr;
  for (int i = 0; i < g.nBass; i++) {
    const Note &n = g.bass[i];
    if (t >= stepStart(g, n.step) && t < stepStart(g, n.step + n.len)) cur = &n;
  }
  if (cur) {
    const float noteT = t - stepStart(g, cur->step);
    const float span = cur->len * stepLen(g);
    const float noteLen = span * (cur->kind == 1 ? 0.35f : 0.92f);
    float hz = midiHz(cur->note);
    if (cur->kind == 3) hz *= powf(2.0f, -noteT / span);  // slide down an octave
    float amp = noteT < 0.004f ? noteT / 0.004f : expf(-noteT / g.bassDecay);
    if (noteT > noteLen) amp *= expf(-(noteT - noteLen) / 0.012f);  // quick release
    if (cur->kind == 1) amp *= 0.45f;                               // ghost note
    if (cur->kind == 2) amp *= 0.85f;
    float s = 0;
    for (int o = 0; o < 2; o++) {
      bassPhase_[o] += hz * (o ? 1.006f : 1.0f) / RATE;
      bassPhase_[o] -= floorf(bassPhase_[o]);
      s += 2 * bassPhase_[o] - 1;
    }
    s *= 0.5f;
    // envelope filter: cutoff jumps up on the attack and falls back
    const float sweep = expf(-noteT / (cur->kind == 2 ? 0.05f : 0.09f));
    const float base = cur->kind == 1 ? 180 : 220;
    const float peak = cur->kind == 2 ? 3200 : cur->kind == 1 ? 500 : g.bassPeak;
    const float cutoff = base + (peak - base) * sweep;
    const float f = 2 * sinf(3.1416f * cutoff / RATE);
    svfLow_ += f * svfBand_;
    const float high = s - svfLow_ - 0.22f * svfBand_;  // low damping = resonant "quack"
    svfBand_ += f * high;
    mix += svfLow_ * amp * 0.9f;
  } else {
    svfLow_ *= 0.995f;
    svfBand_ *= 0.995f;
  }

  // --- Stab: brassy filtered saw chord.
  float ago;
  if (char c = lastHit(g, g.stab, step, t, 8, ago)) {
    if (ago < 0.6f) {
      float s = 0;
      for (int k = 0; k < 3; k++) s += saw(chordHz_[k], ago);
      const float cutoff = 900 + 2600 * expf(-ago / 0.07f);
      const float f = 2 * sinf(3.1416f * cutoff / RATE);
      stabLow_ += f * stabBand_;
      stabBand_ += f * (s / 3 - stabLow_ - 0.5f * stabBand_);
      const float env = (ago < 0.006f ? ago / 0.006f : 1) * expf(-ago / (c == 'X' ? 0.16f : 0.05f));
      mix += stabLow_ * env * 0.55f;
    }
  }

  // noise for the drums and the guitar scratch
  rng_ ^= rng_ << 13;
  rng_ ^= rng_ >> 17;
  rng_ ^= rng_ << 5;
  const float noise = (int32_t)rng_ / 2147483648.0f;
  const float hiss = noise - hatPrev_;  // high-passed noise
  hatPrev_ = noise;

  // --- Guitar: a chord through a wah (band-pass) filter. x = muted
  // chicken-scratch "chk", X = a ringing chord with the pedal rocked open.
  if (char c = lastHit(g, g.gtr, step, t, 6, ago)) {
    if (ago < 0.5f) {
      float s = 0;
      for (int k = 0; k < 3; k++) s += saw(chordHz_[k] * (k ? 0.5f : 0.5015f), ago);  // an octave down
      s /= 3;
      float center, env;
      if (c == 'X') {
        center = 500 + 1900 * sinf(3.1416f * (ago < 0.4f ? ago / 0.4f : 1));  // pedal down and up
        env = (ago < 0.004f ? ago / 0.004f : 1) * expf(-ago / 0.25f);
      } else {
        center = 700 + 1800 * expf(-ago / 0.03f);
        s = s * 0.55f + noise * 0.45f;  // muted strings: more "chk" than note
        env = (ago < 0.002f ? ago / 0.002f : 1) * expf(-ago / 0.03f);
      }
      const float f = 2 * sinf(3.1416f * center / RATE);
      gtrLow_ += f * gtrBand_;
      gtrBand_ += f * (s - gtrLow_ - 0.35f * gtrBand_);
      mix += gtrBand_ * env * 0.5f;
    }
  }

  // --- Drums
  if (lastHit(g, g.kick, step, t, 4, ago) && ago < 0.5f) {  // thump: sine 120 Hz -> 48 Hz
    const float phase = 48 * ago + 72 * 0.025f * (1 - expf(-ago / 0.025f));
    mix += sinf(PI2 * phase) * expf(-ago / 0.14f) * 0.75f;
  }
  if (char c = lastHit(g, g.snare, step, t, 4, ago)) {
    if (c == 'S')  // crack: noise + a little body
      mix += (noise * 0.7f + 0.3f * sinf(PI2 * 190 * ago)) * expf(-ago / 0.08f) * 0.55f;
    else if (c == 'g')  // ghost: a quiet tap
      mix += noise * expf(-ago / 0.03f) * 0.14f;
    else if (c == 'C') {  // handclap: three quick slaps, then the room
      const float env = ago < 0.024f ? expf(-fmodf(ago, 0.008f) / 0.0025f) : expf(-(ago - 0.024f) / 0.08f);
      mix += (noise * 0.5f + hiss * 0.5f) * env * 0.5f;
    }
  }
  if (char c = lastHit(g, g.hat, step, t, 3, ago)) {  // tick: high-passed noise
    if (c == 'O')
      mix += hiss * expf(-ago / 0.1f) * 0.14f;
    else
      mix += hiss * expf(-ago / 0.018f) * (c == 'H' ? 0.22f : 0.12f);
  }

  // Trim the deep rumble the little speaker can't play anyway (it only
  // strains the amp), then gentle saturation: punchy, not squashed.
  mix *= g.gain;
  const float hp = HP_A * (hpOut_ + mix - hpIn_);
  hpIn_ = mix;
  hpOut_ = hp;
  return tanhf(hp * 1.1f) * 0.8f;
}

int FunkIntro::render(int16_t *out, int n) {
  if (!playing_) return 0;
  const uint32_t len = lengthSamples(groove_);
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
