// voice_clips.h - recorded voice clips for Jerry (GENERATED - don't edit).
//
// Made by tools/make_voice.py from the .wav files in voice/. With no clips,
// Jerry uses his built-in Sasquatch sounds (squatch_voice.cpp).
#pragma once
#include <stdint.h>

struct VoiceClip {
  uint8_t sound;       // audio::Sound number
  const uint8_t *pcm;  // 16 kHz, mono, 8-bit (128 = silence)
  uint32_t len;
};

static const VoiceClip kVoiceClips[1] = {{255, nullptr, 0}};
static const int kVoiceClipCount = 0;
