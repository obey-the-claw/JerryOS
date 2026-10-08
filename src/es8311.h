// es8311.h - the AIPI Lite's ES8311 audio chip (I2C address 0x18), shared by
// the microphone (sound meter) and the speaker (boot groove).
//
// Set-up follows Espressif's own driver (github.com/espressif/esp-bsp,
// components/es8311): slave mode, MCLK from the ESP32 (256 x 16 kHz =
// 4.096 MHz), 16-bit I2S. Proven on this board in v0.6 (speaker) and v0.12
// (microphone). Only one of them uses the chip at a time.
#pragma once
#include <stdint.h>

namespace es8311 {

bool begin();                         // start I2C; true = the chip answers
bool write(uint8_t reg, uint8_t val);
int read(uint8_t reg);                // -1 = no answer
bool update(uint8_t reg, uint8_t keepMask, uint8_t setBits);
bool initClocks();                    // reset + clocks + 16-bit I2S + analog power (MCLK must run)

}  // namespace es8311
