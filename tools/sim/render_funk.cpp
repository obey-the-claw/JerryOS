// Renders Jerry's boot groove (src/funk.cpp) to a .wav, so it can be heard on
// a PC before it goes on the device. Same code, same samples.
#include <math.h>
#include <stdio.h>

#include <vector>

#include "funk.h"

static void put32(FILE *f, uint32_t v) { fwrite(&v, 4, 1, f); }
static void put16(FILE *f, uint16_t v) { fwrite(&v, 2, 1, f); }

int main(int argc, char **argv) {
  const char *path = argc > 1 ? argv[1] : "intro_funk.wav";
  FunkIntro f;
  f.start();
  std::vector<int16_t> pcm;
  int16_t buf[256];
  for (int n; (n = f.render(buf, 256)) > 0;) pcm.insert(pcm.end(), buf, buf + n);
  FILE *o = fopen(path, "wb");
  if (!o) return 1;
  const uint32_t bytes = pcm.size() * 2, rate = FunkIntro::RATE;
  fwrite("RIFF", 1, 4, o); put32(o, 36 + bytes); fwrite("WAVEfmt ", 1, 8, o);
  put32(o, 16); put16(o, 1); put16(o, 1); put32(o, rate); put32(o, rate * 2);
  put16(o, 2); put16(o, 16); fwrite("data", 1, 4, o); put32(o, bytes);
  fwrite(pcm.data(), 2, pcm.size(), o);
  fclose(o);
  int peak = 0;
  double sum = 0;
  for (int16_t x : pcm) {
    peak = x > peak ? x : -x > peak ? -x : peak;
    sum += (double)x * x;
  }
  printf("%s: %.2f s, peak %d%%, rms %.0f\n", path, pcm.size() / (double)rate, peak * 100 / 32767,
         sqrt(sum / pcm.size()));
  return 0;
}
