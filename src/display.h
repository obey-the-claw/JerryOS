// display.h - LovyanGFX driver set up for the AIPI Lite screen.
//
// Why LovyanGFX: every panel quirk (colour order, inversion, offsets,
// rotation) is a plain setting here, and it can push pixels with DMA so
// animation stays smooth. TFT_eSPI needs the same settings edited inside the
// library folder, which is awkward to keep in git.
#pragma once

#define LGFX_USE_V1
#include <LovyanGFX.hpp>
#include "board_pins.h"

class JerryDisplay : public lgfx::LGFX_Device {
  lgfx::Panel_ST7735S panel_;
  lgfx::Bus_SPI bus_;
  lgfx::Light_PWM light_;

 public:
  JerryDisplay() {
    {
      auto cfg = bus_.config();
      cfg.spi_host = SPI2_HOST;
      cfg.spi_mode = 0;
      cfg.freq_write = 27000000;   // 27 MHz: verified working by community firmware
      cfg.freq_read = 16000000;
      cfg.spi_3wire = true;
      cfg.use_lock = true;
      cfg.dma_channel = SPI_DMA_CH_AUTO;
      cfg.pin_sclk = PIN_LCD_SCLK;
      cfg.pin_mosi = PIN_LCD_MOSI;
      cfg.pin_miso = -1;
      cfg.pin_dc = PIN_LCD_DC;
      bus_.config(cfg);
      panel_.setBus(&bus_);
    }
    {
      auto cfg = panel_.config();
      cfg.pin_cs = PIN_LCD_CS;
      cfg.pin_rst = PIN_LCD_RST;
      cfg.pin_busy = -1;
      cfg.panel_width = 128;
      cfg.panel_height = 128;
      cfg.memory_width = 128;
      cfg.memory_height = 128;
      cfg.offset_x = 0;
      cfg.offset_y = 0;
      cfg.offset_rotation = 0;
      cfg.readable = false;
      cfg.invert = false;      // community: OFF (two hardware-verified sources)
      cfg.rgb_order = true;    // community: BGR panel
      cfg.dlen_16bit = false;
      cfg.bus_shared = false;
      panel_.config(cfg);
    }
    {
      auto cfg = light_.config();
      cfg.pin_bl = PIN_LCD_BL;
      cfg.invert = false;
      cfg.freq = 5000;
      cfg.pwm_channel = 7;
      light_.config(cfg);
      panel_.setLight(&light_);
    }
    setPanel(&panel_);
  }
};
