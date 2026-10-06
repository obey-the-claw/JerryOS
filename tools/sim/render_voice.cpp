// Renders each of Jerry's sounds to a .wav file, so you can hear them on a
// PC before flashing. Uses the exact same code as the device.
#include <stdio.h>

#include <vector>

#include "squatch_voice.h"

static void put32(FILE *f, uint32_t v) { fwrite(&v, 4, 1, f); }
static void put16(FILE *f, uint16_t v) { fwrite(&v, 2, 1, f); }

int main(int argc, char **argv) {
  const char *dir = argc > 1 ? argv[1] : ".";
  for (int i = 0; i < (int)audio::Sound::COUNT; i++) {
    const auto s = (audio::Sound)i;
    audio::VoiceSynth v;
    v.start(s);
    std::vector<int16_t> pcm;
    int16_t buf[256];
    for (int n; (n = v.render(buf, 256)) > 0;) pcm.insert(pcm.end(), buf, buf + n);
    char path[256];
    snprintf(path, sizeof path, "%s/jerry_%s.wav", dir, audio::soundName(s));
    FILE *f = fopen(path, "wb");
    if (!f) return 1;
    const uint32_t bytes = pcm.size() * 2, rate = audio::VoiceSynth::RATE;
    fwrite("RIFF", 1, 4, f); put32(f, 36 + bytes); fwrite("WAVEfmt ", 1, 8, f);
    put32(f, 16); put16(f, 1); put16(f, 1); put32(f, rate); put32(f, rate * 2);
    put16(f, 2); put16(f, 16); fwrite("data", 1, 4, f); put32(f, bytes);
    fwrite(pcm.data(), 2, pcm.size(), f);
    fclose(f);
    int peak = 0;
    for (int16_t x : pcm) peak = x > peak ? x : -x > peak ? -x : peak;
    printf("%-22s %5.2f s  peak %3d%%\n", path, pcm.size() / (double)rate, peak * 100 / 32767);
  }
  return 0;
}
