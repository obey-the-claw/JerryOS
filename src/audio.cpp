// audio.cpp - see audio.h (device only; the PC simulator uses
// tools/sim/audio_sim.cpp).
//
// The ES8311 start-up sequence follows Espressif's own driver
// (github.com/espressif/esp-bsp, components/es8311): slave mode, MCLK from
// the ESP32 (256 x 16 kHz = 4.096 MHz), 16-bit I2S, DAC to the speaker.

#include "audio.h"

#include <Arduino.h>
#include <Wire.h>
#include <driver/i2s.h>
#include <freertos/queue.h>

#include <atomic>

#include "board_pins.h"

namespace audio {
namespace {

constexpr uint8_t ES8311_ADDR = 0x18;
constexpr int RATE = VoiceSynth::RATE;
constexpr i2s_port_t PORT = I2S_NUM_0;

QueueHandle_t queue = nullptr;
std::atomic<uint8_t> volume{2};
bool found = false;

// --- ES8311 over I2C ---
bool writeReg(uint8_t reg, uint8_t val) {
  Wire.beginTransmission(ES8311_ADDR);
  Wire.write(reg);
  Wire.write(val);
  return Wire.endTransmission() == 0;
}

int readReg(uint8_t reg) {
  Wire.beginTransmission(ES8311_ADDR);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return -1;
  if (Wire.requestFrom(ES8311_ADDR, (uint8_t)1) != 1) return -1;
  return Wire.read();
}

bool updateReg(uint8_t reg, uint8_t keepMask, uint8_t setBits) {
  const int v = readReg(reg);
  return v >= 0 && writeReg(reg, (uint8_t)((v & keepMask) | setBits));
}

// DAC volume register: 0xBF = 0 dB, each step 0.5 dB.
uint8_t volumeReg(uint8_t level) {
  switch (level) {
    case 1: return 0xA5;  // -13 dB
    case 2: return 0xB4;  // -5.5 dB
    default: return 0xBF; // 0 dB
  }
}

bool initCodec() {
  bool ok = writeReg(0x00, 0x1F);           // reset
  delay(20);
  ok &= writeReg(0x00, 0x00);
  ok &= writeReg(0x00, 0x80);               // power on
  ok &= writeReg(0x01, 0x3F);               // all clocks on, MCLK from the MCLK pin
  ok &= updateReg(0x06, 0xDF, 0x00);        // BCLK not inverted
  // Clock dividers for 4.096 MHz MCLK / 16 kHz (Espressif's coefficient table)
  ok &= updateReg(0x02, 0x07, 0x00);        // pre_div 1, pre_multi x1
  ok &= writeReg(0x03, 0x10);               // fs_mode single speed, adc_osr 0x10
  ok &= writeReg(0x04, 0x10);               // dac_osr 0x10
  ok &= writeReg(0x05, 0x00);               // adc_div 1, dac_div 1
  ok &= updateReg(0x06, 0xE0, 0x03);        // bclk_div 4
  ok &= updateReg(0x07, 0xC0, 0x00);        // lrck_h
  ok &= writeReg(0x08, 0xFF);               // lrck_l
  ok &= updateReg(0x00, 0xBF, 0x00);        // slave mode
  ok &= writeReg(0x09, 0x0C);               // I2S in: 16-bit
  ok &= writeReg(0x0A, 0x0C);               // I2S out: 16-bit
  ok &= writeReg(0x0D, 0x01);               // power up analog
  ok &= writeReg(0x0E, 0x02);               // PGA / ADC modulator
  ok &= writeReg(0x12, 0x00);               // power up DAC
  ok &= writeReg(0x13, 0x10);               // output driver on
  ok &= writeReg(0x1C, 0x6A);               // ADC equalizer bypass
  ok &= writeReg(0x37, 0x08);               // DAC equalizer bypass
  ok &= writeReg(0x32, volumeReg(volume));  // volume
  ok &= updateReg(0x31, 0x9F, 0x00);        // unmute
  return ok;
}

bool initI2S() {
  i2s_config_t cfg = {};
  cfg.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX);
  cfg.sample_rate = RATE;
  cfg.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
  cfg.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;
  cfg.communication_format = I2S_COMM_FORMAT_STAND_I2S;
  cfg.dma_buf_count = 4;
  cfg.dma_buf_len = 256;
  cfg.tx_desc_auto_clear = true;  // plays silence when we run out of sound
  cfg.mclk_multiple = I2S_MCLK_MULTIPLE_256;
  if (i2s_driver_install(PORT, &cfg, 0, nullptr) != ESP_OK) return false;
  i2s_pin_config_t pins = {};
  pins.mck_io_num = PIN_I2S_MCLK;
  pins.bck_io_num = PIN_I2S_BCLK;
  pins.ws_io_num = PIN_I2S_WS;
  pins.data_out_num = PIN_I2S_DOUT;
  pins.data_in_num = I2S_PIN_NO_CHANGE;
  return i2s_set_pin(PORT, &pins) == ESP_OK;
}

// --- Playing: the voice synth makes the samples, we send them to the chip ---
VoiceSynth synth;

void playSound(Sound s) {
  static int16_t mono[256];
  static int16_t stereo[256 * 2];
  synth.start(s);
  for (int tail = 0; tail < 2;) {
    int n = synth.render(mono, 256);
    if (n == 0) {  // finished: send a little silence so the end isn't cut off
      n = 256;
      for (int i = 0; i < n; i++) mono[i] = 0;
      tail++;
    }
    for (int i = 0; i < n; i++) stereo[2 * i] = stereo[2 * i + 1] = mono[i];
    size_t written;
    i2s_write(PORT, stereo, n * 4, &written, portMAX_DELAY);
  }
}

void task(void *) {
  bool ampOn = false;
  for (;;) {
    uint8_t id;
    if (xQueueReceive(queue, &id, pdMS_TO_TICKS(1500)) == pdTRUE) {
      if (!volume) continue;
      if (!ampOn) {
        digitalWrite(PIN_SPK_AMP_EN, HIGH);
        delay(40);  // let the amp wake up
        ampOn = true;
      }
      playSound((Sound)id);
    } else if (ampOn) {   // quiet for 1.5 s: amp off
      i2s_zero_dma_buffer(PORT);
      digitalWrite(PIN_SPK_AMP_EN, LOW);
      ampOn = false;
    }
  }
}

}  // namespace

bool begin() {
  pinMode(PIN_SPK_AMP_EN, OUTPUT);
  digitalWrite(PIN_SPK_AMP_EN, LOW);

  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL, 100000);
  const int id1 = readReg(0xFD), id2 = readReg(0xFE);  // chip ID: 0x83 0x11
  Serial.printf("[audio] ES8311 id %02x %02x\n", id1 & 0xFF, id2 & 0xFF);
  if (id1 < 0) {
    Serial.println(F("[audio] audio chip not answering - sound off"));
    return false;
  }
  if (!initI2S()) {  // MCLK must be running before the chip is set up
    Serial.println(F("[audio] I2S setup failed - sound off"));
    return false;
  }
  if (!initCodec()) Serial.println(F("[audio] some audio chip settings didn't take"));
  queue = xQueueCreate(4, sizeof(uint8_t));
  xTaskCreatePinnedToCore(task, "audio", 4096, nullptr, 2, nullptr, 1);
  found = true;
  Serial.println(F("[audio] ready"));
  return true;
}

bool ready() { return found; }

void setVolume(uint8_t level) {
  volume = level > 3 ? 3 : level;
  if (found && volume) writeReg(0x32, volumeReg(volume));
}

void play(Sound s) {
  if (!found || !volume) return;
  const uint8_t id = (uint8_t)s;
  xQueueSend(queue, &id, 0);  // queue full = skip it, never block the screen
}

}  // namespace audio
