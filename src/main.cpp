// JerryOS - Checkpoint 2: Hello World / hardware test
//
// Proves every piece of hardware v0 needs before we build on it:
//   - screen: a test card to check colours, orientation and edges
//   - buttons: live state, short/long press detection, hold timer
//   - power: battery voltage, USB present, charging
//   - serial log: chip facts at boot, a status line every 2 s, every button event
//
// Controls on this test screen:
//   RIGHT hold  -> toggle colour inversion (so we can compare both settings)
//   anything else just shows up on screen and in the log.

#include <Arduino.h>
#include <esp_system.h>
#include "board_pins.h"
#include "buttons.h"
#include "display.h"
#include "power.h"

static JerryDisplay lcd;
static LGFX_Sprite frame(&lcd);  // draw off-screen, then push in one go (no flicker)

static Button btnLeft(PIN_BTN_LEFT);
static Button btnRight(PIN_BTN_RIGHT);

static PowerStatus pwr;
static bool inverted = false;
static uint32_t leftShorts = 0, leftLongs = 0, rightShorts = 0, rightLongs = 0;
static uint32_t leftMaxHoldMs = 0;
static char lastEvent[24] = "none yet";

// Brand colours (see README).
static const uint16_t C_BARK = lgfx::color565(0x2a, 0x1a, 0x0e);
static const uint16_t C_FUR = lgfx::color565(0xa8, 0x73, 0x5f);
static const uint16_t C_GOLD = lgfx::color565(0xe8, 0xa3, 0x17);
static const uint16_t C_CREAM = lgfx::color565(0xf7, 0xf0, 0xe4);
static const uint16_t C_GRIN = lgfx::color565(0xf4, 0x51, 0x1e);

static const char *resetReasonName(esp_reset_reason_t r) {
  switch (r) {
    case ESP_RST_POWERON: return "power-on";
    case ESP_RST_SW: return "software";
    case ESP_RST_PANIC: return "crash";
    case ESP_RST_INT_WDT:
    case ESP_RST_TASK_WDT:
    case ESP_RST_WDT: return "watchdog";
    case ESP_RST_DEEPSLEEP: return "deep-sleep";
    case ESP_RST_BROWNOUT: return "brownout";
    default: return "other";
  }
}

static void logBootFacts() {
  Serial.println();
  Serial.println(F("=========================================="));
  Serial.println(F(" JerryOS checkpoint 2 - hardware test"));
  Serial.println(F("=========================================="));
  Serial.printf("Chip:        %s rev %d, %d cores @ %d MHz\n", ESP.getChipModel(),
                ESP.getChipRevision(), ESP.getChipCores(), ESP.getCpuFreqMHz());
  Serial.printf("Flash:       %u MB\n", ESP.getFlashChipSize() / (1024 * 1024));
  Serial.printf("PSRAM:       %u KB (%s)\n", ESP.getPsramSize() / 1024,
                psramFound() ? "found" : "NOT FOUND");
  Serial.printf("Free heap:   %u KB\n", ESP.getFreeHeap() / 1024);
  Serial.printf("Reset cause: %s\n", resetReasonName(esp_reset_reason()));
  Serial.printf("Buttons at boot (1 = up, 0 = pressed): L=%d R=%d\n",
                digitalRead(PIN_BTN_LEFT), digitalRead(PIN_BTN_RIGHT));
}

static void logStatus() {
  Serial.printf("[status] batt pin=%umV est=%.2fV %s | usb=%d charging=%d | "
                "L=%s R=%s | up %lus\n",
                pwr.pinMv, pwr.battV,
                pwr.batteryPresent ? (String(pwr.percent) + "%").c_str() : "no-batt?",
                pwr.usb, pwr.charging, btnLeft.isPressed() ? "DOWN" : "up",
                btnRight.isPressed() ? "DOWN" : "up", millis() / 1000);
}

static void onEvent(const char *name) {
  strncpy(lastEvent, name, sizeof(lastEvent) - 1);
  Serial.printf("[button] %s\n", name);
}

// ---------------------------------------------------------------------------
// Test card layout (128 x 128):
//   y  0-9   title bar
//   y 11-28  colour swatches: R G B W / brand colours
//   y 31-108 live readouts
//   y 117    hint bar
// A 1-pixel frame runs around the very edge, with a coloured corner marker in
// each corner. If any edge is missing or doubled, the panel offset is wrong.
// ---------------------------------------------------------------------------
static void drawSwatch(int x, int y, uint16_t col, const char *label, uint16_t textCol) {
  frame.fillRect(x, y, 20, 18, col);
  frame.setTextColor(textCol);
  frame.setCursor(x + 7, y + 5);
  frame.print(label);
}

// Print one line of text in the readout area and move down a row.
static int lineY = 0;
static void line(const char *fmt, ...) {
  char buf[32];
  va_list args;
  va_start(args, fmt);
  vsnprintf(buf, sizeof(buf), fmt, args);
  va_end(args);
  frame.setCursor(4, lineY);
  frame.print(buf);
  lineY += 10;
}

static void drawFrame() {
  frame.fillScreen(TFT_BLACK);
  frame.setTextSize(1);
  frame.setFont(&fonts::Font0);  // classic 6x8 pixel font

  // Title
  frame.fillRect(0, 0, 128, 10, C_BARK);
  frame.setTextColor(C_GOLD);
  frame.setCursor(4, 1);
  frame.print("JerryOS hw test");

  // Colour swatches: the letter should match the colour you see.
  drawSwatch(4, 11, TFT_RED, "R", TFT_WHITE);
  drawSwatch(24, 11, TFT_GREEN, "G", TFT_BLACK);
  drawSwatch(44, 11, TFT_BLUE, "B", TFT_WHITE);
  drawSwatch(64, 11, TFT_WHITE, "W", TFT_BLACK);
  drawSwatch(84, 11, C_GOLD, "Y", TFT_BLACK);
  drawSwatch(104, 11, C_GRIN, "O", TFT_WHITE);

  // Live readouts
  frame.setTextColor(C_CREAM);
  lineY = 32;
  line("L %-4s s%lu l%lu", btnLeft.isPressed() ? "DOWN" : "up", leftShorts, leftLongs);
  if (btnLeft.isPressed()) line("  held %.1fs", btnLeft.heldMs() / 1000.0f);
  else line("  max hold %.1fs", leftMaxHoldMs / 1000.0f);
  line("R %-4s s%lu l%lu", btnRight.isPressed() ? "DOWN" : "up", rightShorts, rightLongs);
  line("last: %s", lastEvent);
  if (pwr.batteryPresent) line("batt %.2fV %d%%", pwr.battV, pwr.percent);
  else line("batt: none? (USB)");
  line("usb %s  chg %s", pwr.usb ? "YES" : "no", pwr.charging ? "YES" : "no");
  line("up %lus inv %s", millis() / 1000, inverted ? "ON" : "off");

  // Hint bar
  frame.fillRect(0, 116, 128, 12, C_FUR);
  frame.setTextColor(TFT_BLACK);
  frame.setCursor(4, 118);
  frame.print("hold R: invert");

  // Edge frame + corner markers (drawn last so nothing covers them)
  frame.drawRect(0, 0, 128, 128, TFT_WHITE);
  frame.fillRect(0, 0, 4, 4, TFT_RED);        // top-left
  frame.fillRect(124, 0, 4, 4, TFT_GREEN);    // top-right
  frame.fillRect(0, 124, 4, 4, TFT_BLUE);     // bottom-left
  frame.fillRect(124, 124, 4, 4, TFT_YELLOW); // bottom-right

  frame.pushSprite(0, 0);
}

void setup() {
  power::holdOn();  // first, before anything slow
  power::begin();
  btnLeft.begin();
  btnRight.begin();

  Serial.begin(115200);
  delay(300);  // give USB a moment so the first lines aren't lost
  logBootFacts();

  lcd.init();
  lcd.setRotation(1);  // community-verified orientation (MADCTL 0x68 with BGR)
  lcd.setBrightness(200);
  frame.setColorDepth(16);
  if (!frame.createSprite(128, 128)) Serial.println(F("!! sprite allocation failed"));

  pwr = power::read();
  logStatus();
  Serial.println(F("Ready. Press some buttons!"));
}

void loop() {
  static uint32_t lastDraw = 0, lastPower = 0, lastLog = 0;
  const uint32_t now = millis();

  switch (btnLeft.update()) {
    case ButtonEvent::Short: leftShorts++; onEvent("LEFT short"); break;
    case ButtonEvent::Long: leftLongs++; onEvent("LEFT long"); break;
    default: break;
  }
  switch (btnRight.update()) {
    case ButtonEvent::Short: rightShorts++; onEvent("RIGHT short"); break;
    case ButtonEvent::Long:
      rightLongs++;
      inverted = !inverted;
      lcd.invertDisplay(inverted);
      onEvent(inverted ? "RIGHT long: inv ON" : "RIGHT long: inv off");
      break;
    default: break;
  }
  if (btnLeft.isPressed()) leftMaxHoldMs = max(leftMaxHoldMs, btnLeft.heldMs());

  if (now - lastPower >= 500) { lastPower = now; pwr = power::read(); }
  if (now - lastLog >= 2000) { lastLog = now; logStatus(); }
  if (now - lastDraw >= 50) { lastDraw = now; drawFrame(); }  // ~20 fps
}
