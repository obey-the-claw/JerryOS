// es8311.cpp - see es8311.h.

#include "es8311.h"

#include <Arduino.h>
#include <Wire.h>

#include "board_pins.h"

namespace es8311 {
namespace {
constexpr uint8_t ADDR = 0x18;
bool started = false;
}  // namespace

bool begin() {
  if (!started) {
    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL, 100000);
    started = true;
  }
  return read(0xFD) >= 0;  // chip ID register answers?
}

bool write(uint8_t reg, uint8_t val) {
  Wire.beginTransmission(ADDR);
  Wire.write(reg);
  Wire.write(val);
  return Wire.endTransmission() == 0;
}

int read(uint8_t reg) {
  Wire.beginTransmission(ADDR);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return -1;
  if (Wire.requestFrom(ADDR, (uint8_t)1) != 1) return -1;
  return Wire.read();
}

bool update(uint8_t reg, uint8_t keep, uint8_t set) {
  const int v = read(reg);
  return v >= 0 && write(reg, (uint8_t)((v & keep) | set));
}

bool initClocks() {
  bool ok = write(0x00, 0x1F);       // reset
  delay(20);
  ok &= write(0x00, 0x00);
  ok &= write(0x00, 0x80);           // power on
  ok &= write(0x01, 0x3F);           // clocks on, MCLK from the MCLK pin
  ok &= update(0x06, 0xDF, 0x00);    // BCLK not inverted
  ok &= update(0x02, 0x07, 0x00);    // 4.096 MHz MCLK / 16 kHz dividers
  ok &= write(0x03, 0x10);
  ok &= write(0x04, 0x10);
  ok &= write(0x05, 0x00);
  ok &= update(0x06, 0xE0, 0x03);
  ok &= update(0x07, 0xC0, 0x00);
  ok &= write(0x08, 0xFF);
  ok &= update(0x00, 0xBF, 0x00);    // slave mode
  ok &= write(0x09, 0x0C);           // 16-bit in (speaker data)
  ok &= write(0x0A, 0x0C);           // 16-bit out (microphone data)
  ok &= write(0x0D, 0x01);           // power up analog
  ok &= write(0x0E, 0x02);           // PGA + ADC modulator
  ok &= write(0x12, 0x00);           // DAC power
  ok &= write(0x13, 0x10);           // output driver
  ok &= write(0x1C, 0x6A);           // ADC: no equalizer, remove DC
  ok &= write(0x37, 0x08);           // DAC: no equalizer
  return ok;
}

}  // namespace es8311
