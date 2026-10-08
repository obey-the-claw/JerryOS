// speaker.cpp - see speaker.h (device only; the PC simulator uses
// tools/sim/speaker_sim.cpp).
//
// ESP32 --I2S--> ES8311 DAC --> amplifier (GPIO 9) --> speaker.

#include "speaker.h"

#include <Arduino.h>
#include <driver/i2s.h>

#include <atomic>

#include "board_pins.h"
#include "es8311.h"
#include "funk.h"

namespace speaker {
namespace {

constexpr i2s_port_t PORT = I2S_NUM_0;
constexpr uint8_t VOLUME = 0xB8;  // DAC volume: 0xBF = 0 dB, 0.5 dB steps (-3.5 dB)

std::atomic<bool> playing{false}, stopNow{false};

bool startI2S() {
  i2s_config_t cfg = {};
  cfg.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX);
  cfg.sample_rate = FunkIntro::RATE;
  cfg.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
  cfg.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;
  cfg.communication_format = I2S_COMM_FORMAT_STAND_I2S;
  cfg.dma_buf_count = 6;
  cfg.dma_buf_len = 256;
  cfg.tx_desc_auto_clear = true;  // silence if we ever run late
  cfg.mclk_multiple = I2S_MCLK_MULTIPLE_256;
  if (i2s_driver_install(PORT, &cfg, 0, nullptr) != ESP_OK) return false;
  i2s_pin_config_t pins = {};
  pins.mck_io_num = PIN_I2S_MCLK;
  pins.bck_io_num = PIN_I2S_BCLK;
  pins.ws_io_num = PIN_I2S_WS;
  pins.data_out_num = PIN_I2S_DOUT;
  pins.data_in_num = I2S_PIN_NO_CHANGE;
  if (i2s_set_pin(PORT, &pins) != ESP_OK) {
    i2s_driver_uninstall(PORT);
    return false;
  }
  return true;
}

void task(void *) {
  static FunkIntro groove;  // big-ish state: keep it off the task's stack
  static int16_t mono[256], stereo[512];
  if (es8311::begin() && startI2S()) {
    bool ok = es8311::initClocks();
    ok &= es8311::write(0x32, VOLUME);
    ok &= es8311::update(0x31, 0x9F, 0x00);  // unmute the DAC
    if (!ok) Serial.println(F("[speaker] some audio chip settings didn't take"));
    digitalWrite(PIN_SPK_AMP_EN, HIGH);      // amplifier on
    delay(30);
    groove.start();
    Serial.println(F("[speaker] playing the boot groove"));
    for (int n; !stopNow && (n = groove.render(mono, 256)) > 0;) {
      for (int i = 0; i < n; i++) stereo[2 * i] = stereo[2 * i + 1] = mono[i];
      size_t written;
      i2s_write(PORT, stereo, n * 4, &written, portMAX_DELAY);
    }
    i2s_zero_dma_buffer(PORT);
    delay(60);
    digitalWrite(PIN_SPK_AMP_EN, LOW);       // amplifier off
    es8311::update(0x31, 0x9F, 0x60);        // mute
    es8311::write(0x0D, 0xFC);               // analog off
    i2s_driver_uninstall(PORT);
  } else {
    Serial.println(F("[speaker] audio chip / I2S not available - no groove"));
  }
  playing = false;
  vTaskDelete(nullptr);
}

}  // namespace

void playIntro() {
  if (playing) return;
  pinMode(PIN_SPK_AMP_EN, OUTPUT);
  digitalWrite(PIN_SPK_AMP_EN, LOW);
  playing = true;
  stopNow = false;
  if (xTaskCreatePinnedToCore(task, "speaker", 4096, nullptr, 3, nullptr, 1) != pdPASS)
    playing = false;
}

void stop() { stopNow = true; }
bool busy() { return playing; }

}  // namespace speaker
