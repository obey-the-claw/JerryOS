// board_pins.h - every AIPI Lite pin JerryOS uses, in one place.
//
// Sources and confidence levels: docs/HARDWARE.md. If a pin turns out wrong
// on real hardware, this is the only file that should need to change.
#pragma once

// --- Display: ST7735-family 128x128 TFT on SPI ---
constexpr int PIN_LCD_SCLK = 16;
constexpr int PIN_LCD_MOSI = 17;
constexpr int PIN_LCD_CS   = 15;
constexpr int PIN_LCD_DC   = 7;
constexpr int PIN_LCD_RST  = 18;
constexpr int PIN_LCD_BL   = 3;    // backlight, PWM dimmable

// --- Buttons (both read 0 when pressed) ---
constexpr int PIN_BTN_LEFT  = 1;   // also the stock power button
constexpr int PIN_BTN_RIGHT = 42;

// --- Power ---
constexpr int PIN_POWER_HOLD = 10; // drive HIGH at boot or the device turns off on battery
constexpr int PIN_BATT_ADC   = 2;  // battery voltage through a divider
constexpr int PIN_USB_SENSE  = 8;  // 1 = USB power present
constexpr int PIN_CHARGING   = 21; // 0 = charger is charging

// Battery divider multiplier. Community value; calibrate with a multimeter.
constexpr float BATT_DIVIDER = 2.5f;

// --- Audio: ES8311 chip (I2C address 0x18) -> amp -> speaker ---
// JerryOS is silent (v0.6.2): only the amp pin is used, to keep it OFF.
// A working ES8311 driver is in git history (commit 2b89171, src/audio.cpp).
constexpr int PIN_I2C_SDA    = 5;
constexpr int PIN_I2C_SCL    = 4;
constexpr int PIN_I2S_MCLK   = 6;
constexpr int PIN_I2S_BCLK   = 14;
constexpr int PIN_I2S_WS     = 12;
constexpr int PIN_I2S_DOUT   = 11;  // ESP32 -> speaker
constexpr int PIN_I2S_DIN    = 13;  // microphone -> ESP32 (not used)
constexpr int PIN_SPK_AMP_EN = 9;   // HIGH = amplifier on
