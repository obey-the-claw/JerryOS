// knock_detect.h - Knock Detector: listens for wood knocks and long howls.
//
// Squatchers knock on trees and listen for knocks back. A knock is a short,
// sharp bang that stands well above the background (sharp = its loudest
// 10 ms is much louder than the rest of the 100 ms block around it). A howl
// is a long, steady call: louder than the background for over a second with
// a steady pitch (measured by how often the wave crosses zero).
//
// It only reports "possible" knocks and howls. Wind, doors and dogs will
// fool it sometimes; it's a fun listening aid, not proof.
//
// Pure logic (unit-tested on a PC). The microphone feeds it from mic.cpp.
#pragma once
#include <stddef.h>
#include <stdint.h>

#include <atomic>

struct BlockFeatures {
  float dbfs;    // whole block loudness (dB below full scale)
  float peakDb;  // loudest 10 ms inside it
  float zcr;     // zero crossings per second (a rough pitch)
};
// Measure one block of mono samples.
BlockFeatures analyzeBlock(const int16_t *samples, size_t n, int rate);

enum class SoundKind : uint8_t { Knock, Howl };
struct SoundEvent {
  SoundKind kind;
  uint8_t aboveDb;  // how far above the background it was
  uint8_t inARow;   // knocks: 1, 2, 3... within a couple of seconds of each other
  uint32_t ms;
};

class KnockDetector {
 public:
  static constexpr float KNOCK_DB = 18;      // above the background
  static constexpr float SHARP_DB = 5;       // peak 10 ms over the block's average
  static constexpr float HOWL_DB = 10;
  static constexpr int HOWL_BLOCKS = 12;     // 1.2 s of 100 ms blocks
  static constexpr uint32_t ROW_MS = 2500;   // knocks this close count as one sequence

  void add(const BlockFeatures &f, uint32_t now);  // one ~100 ms block
  bool take(SoundEvent &e);                         // next event, if any
  uint32_t knocks() const { return knocks_; }
  uint32_t howls() const { return howls_; }
  float background() const { return bg_; }
  bool ready() const { return blocks_ >= WARMUP; }

 private:
  static constexpr int WARMUP = 10;
  void push(const SoundEvent &e);
  float bg_ = -90;
  int blocks_ = 0;
  uint32_t lastKnockMs_ = 0;
  uint8_t row_ = 0;
  bool anyKnock_ = false;
  int howlRun_ = 0;
  bool howlFired_ = false;
  float zSum_ = 0, zSq_ = 0;
  std::atomic<uint32_t> knocks_{0}, howls_{0};
  // Single producer (the mic task) / single consumer (the screen) ring.
  SoundEvent ev_[8] = {};
  std::atomic<uint8_t> head_{0}, tail_{0};
};
