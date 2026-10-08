// mic.cpp - see mic.h (device only; the PC simulator uses
// tools/sim/mic_sim.cpp).
//
// ES8311 set-up follows Espressif's own driver (github.com/espressif/esp-bsp,
// components/es8311): slave mode, MCLK from the ESP32 (256 x 16 kHz), 16-bit
// I2S, analog microphone into the ADC. The same clock settings were proven on
// this device in v0.6 (when Jerry still had a speaker voice).

#include "mic.h"

#include <Arduino.h>
#include <Wire.h>
#include <driver/i2s.h>

#include <atomic>

#include "board_pins.h"

namespace mic {
namespace {

constexpr uint8_t ES8311_ADDR = 0x18;
constexpr int RATE = 16000;
constexpr i2s_port_t PORT = I2S_NUM_0;
constexpr uint32_t IDLE_OFF_MS = 3000;  // nobody asked for this long: switch off

std::atomic<uint32_t> lastUse{0};
std::atomic<bool> on{false}, found{false}, wantReset{false};
bool started = false;
LevelMeter levels;  // written by the mic task, read by the screen (floats: fine)
SemaphoreHandle_t wake = nullptr;

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

bool updateReg(uint8_t reg, uint8_t keep, uint8_t set) {
  const int v = readReg(reg);
  return v >= 0 && writeReg(reg, (uint8_t)((v & keep) | set));
}

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
  bool ok = writeReg(0x00, 0x1F);       // reset
  delay(20);
  ok &= writeReg(0x00, 0x00);
  ok &= writeReg(0x00, 0x80);           // power on
  ok &= writeReg(0x01, 0x3F);           // clocks on, MCLK from the MCLK pin
  ok &= updateReg(0x06, 0xDF, 0x00);    // BCLK not inverted
  ok &= updateReg(0x02, 0x07, 0x00);    // 4.096 MHz MCLK / 16 kHz dividers
  ok &= writeReg(0x03, 0x10);
  ok &= writeReg(0x04, 0x10);
  ok &= writeReg(0x05, 0x00);
  ok &= updateReg(0x06, 0xE0, 0x03);
  ok &= updateReg(0x07, 0xC0, 0x00);
  ok &= writeReg(0x08, 0xFF);
  ok &= updateReg(0x00, 0xBF, 0x00);    // slave mode
  ok &= writeReg(0x09, 0x0C);           // 16-bit in
  ok &= writeReg(0x0A, 0x0C);           // 16-bit out (the microphone data)
  ok &= writeReg(0x0D, 0x01);           // power up analog
  ok &= writeReg(0x0E, 0x02);           // PGA + ADC modulator on
  ok &= writeReg(0x12, 0x00);
  ok &= writeReg(0x13, 0x10);
  ok &= writeReg(0x1C, 0x6A);           // ADC: no equalizer, remove DC
  ok &= writeReg(0x37, 0x08);
  ok &= writeReg(0x17, 0xC8);           // ADC volume
  ok &= writeReg(0x14, 0x1A);           // analog microphone, PGA on
  ok &= writeReg(0x16, 0x03);           // mic gain +18 dB
  ok &= updateReg(0x31, 0x9F, 0x60);    // DAC (speaker side) muted
  return ok;
}

void stopAll() {
  i2s_driver_uninstall(PORT);
  writeReg(0x0D, 0xFC);  // analog off
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
      levels.add(blockDbfs(mono, frames));
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
    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL, 100000);
    found = readReg(0xFD) >= 0;  // chip ID register answers?
    Serial.printf("[mic] audio chip %s\n", found ? "found" : "NOT answering");
    wake = xSemaphoreCreateBinary();
    xTaskCreatePinnedToCore(task, "mic", 4096, nullptr, 1, nullptr, 1);
  }
  if (found && !on) xSemaphoreGive(wake);
}

bool running() { return on; }
bool chipFound() { return found; }
const LevelMeter &meter() { return levels; }
void resetPeak() { wantReset = true; }
void setCalibration(int db) { levels.setCalibration(db); }

}  // namespace mic
