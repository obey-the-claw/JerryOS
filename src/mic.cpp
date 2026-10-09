// mic.cpp - see mic.h (device only; the PC simulator uses
// tools/sim/mic_sim.cpp).
//
// The audio chip set-up is shared with the speaker (es8311.cpp); this adds
// the microphone side: analog mic -> PGA -> ADC -> I2S into the ESP32.

#include "mic.h"

#include <Arduino.h>
#include <driver/i2s.h>

#include <atomic>

#include "board_pins.h"
#include "es8311.h"
#include "speaker.h"

namespace mic {
namespace {

constexpr int RATE = 16000;
constexpr i2s_port_t PORT = I2S_NUM_0;
constexpr uint32_t IDLE_OFF_MS = 3000;  // nobody asked for this long: switch off

std::atomic<uint32_t> lastUse{0};
std::atomic<bool> on{false}, found{false}, wantReset{false};
bool started = false;
LevelMeter levels;  // written by the mic task, read by the screen (floats: fine)
KnockDetector knocks;  // same: its event queue is safe between the two
SemaphoreHandle_t wake = nullptr;

bool startI2S() {
  i2s_config_t cfg = {};
  cfg.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX);
  cfg.sample_rate = RATE;
  cfg.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
  cfg.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;
  cfg.communication_format = I2S_COMM_FORMAT_STAND_I2S;
  cfg.dma_buf_count = 4;
  cfg.dma_buf_len = 256;
  cfg.mclk_multiple = I2S_MCLK_MULTIPLE_256;
  if (i2s_driver_install(PORT, &cfg, 0, nullptr) != ESP_OK) return false;
  i2s_pin_config_t pins = {};
  pins.mck_io_num = PIN_I2S_MCLK;
  pins.bck_io_num = PIN_I2S_BCLK;
  pins.ws_io_num = PIN_I2S_WS;
  pins.data_out_num = I2S_PIN_NO_CHANGE;
  pins.data_in_num = PIN_I2S_DIN;
  if (i2s_set_pin(PORT, &pins) != ESP_OK) {
    i2s_driver_uninstall(PORT);
    return false;
  }
  return true;
}

bool startCodec() {  // MCLK must already be running
  bool ok = es8311::initClocks();
  ok &= es8311::write(0x17, 0xC8);           // ADC volume
  ok &= es8311::write(0x14, 0x1A);           // analog microphone, PGA on
  ok &= es8311::write(0x16, 0x03);           // mic gain +18 dB
  ok &= es8311::update(0x31, 0x9F, 0x60);    // DAC (speaker side) muted
  return ok;
}

void stopAll() {
  i2s_driver_uninstall(PORT);
  es8311::write(0x0D, 0xFC);  // analog off
  on = false;
  Serial.println(F("[mic] off"));
}

void task(void *) {
  static int16_t buf[1600 * 2];  // 100 ms of stereo samples
  static int16_t mono[1600];
  for (;;) {
    xSemaphoreTake(wake, portMAX_DELAY);  // sleep until someone wants levels
    if (!startI2S()) {
      Serial.println(F("[mic] I2S setup failed"));
      continue;
    }
    if (!startCodec()) Serial.println(F("[mic] some audio chip settings didn't take"));
    on = true;
    Serial.println(F("[mic] on"));
    while (millis() - lastUse < IDLE_OFF_MS) {
      size_t got = 0;
      i2s_read(PORT, buf, sizeof buf, &got, pdMS_TO_TICKS(500));
      const size_t frames = got / 4;
      if (!frames) continue;
      // The microphone is on one side; use whichever side is louder.
      double l = 0, r = 0;
      for (size_t i = 0; i < frames; i++) {
        l += abs(buf[2 * i]);
        r += abs(buf[2 * i + 1]);
      }
      const int side = l >= r ? 0 : 1;
      for (size_t i = 0; i < frames; i++) mono[i] = buf[2 * i + side];
      if (wantReset.exchange(false)) levels.resetPeak();
      const BlockFeatures f = analyzeBlock(mono, frames, RATE);
      levels.add(f.dbfs);
      knocks.add(f, millis());
    }
    stopAll();
  }
}

}  // namespace

void use() {
  lastUse = millis();
  if (!started) {
    started = true;
    pinMode(PIN_SPK_AMP_EN, OUTPUT);  // the speaker stays off
    digitalWrite(PIN_SPK_AMP_EN, LOW);
    found = es8311::begin();
    Serial.printf("[mic] audio chip %s\n", found ? "found" : "NOT answering");
    wake = xSemaphoreCreateBinary();
    xTaskCreatePinnedToCore(task, "mic", 4096, nullptr, 1, nullptr, 1);
  }
  if (speaker::busy()) return;  // the boot groove is using the audio chip
  if (found && !on) xSemaphoreGive(wake);
}

bool running() { return on; }
bool chipFound() { return found; }
const LevelMeter &meter() { return levels; }
void resetPeak() { wantReset = true; }
void setCalibration(int db) { levels.setCalibration(db); }
bool takeSoundEvent(SoundEvent &e) { return knocks.take(e); }
uint32_t knockCount() { return knocks.knocks(); }
uint32_t howlCount() { return knocks.howls(); }
bool knockReady() { return knocks.ready(); }

}  // namespace mic
