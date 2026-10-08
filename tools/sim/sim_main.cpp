// Runs the real JerryOS shell + screens on a PC with simulated buttons,
// saving a screenshot after each step.
#include "display.h"
#include "screens.h"
#include "shell.h"
#include "status_led.h"
extern Rgb g_simLed;
uint32_t g_fake_ms = 1000;
int g_pin[64];
SerialStub Serial;
EspStub ESP;
static JerryDisplay lcd;
static Shell shell(lcd, homeScreen());
static int shot = 0;
static void snap(const char *name) {
  char path[96]; snprintf(path, sizeof path, "shots/%02d_%s.ppm", ++shot, name);
  FILE *f = fopen(path, "wb"); const int sc = 3, led = 8;  // + a strip showing the LED
  fprintf(f, "P6\n%d %d\n255\n", 128 * sc, (128 + led) * sc);
  for (int y = 0; y < 128 * sc; y++) for (int x = 0; x < 128 * sc; x++) {
    uint16_t c = lcd.readPixel(x / sc, y / sc);
    const int lit = lcd.brightness;  // what you'd actually see (backlight off = black)
    unsigned char rgb[3] = {(unsigned char)(((c >> 11) & 31) * 255 / 31 * lit / 255), (unsigned char)(((c >> 5) & 63) * 255 / 63 * lit / 255), (unsigned char)((c & 31) * 255 / 31 * lit / 255)};
    fwrite(rgb, 1, 3, f);
  }
  for (int y = 0; y < led * sc; y++) for (int x = 0; x < 128 * sc; x++) {
    const int dx = x - 64 * sc, dy = y - led * sc / 2;
    const bool dot = dx * dx + dy * dy <= 9 * sc * sc / 4 * 4;
    const auto boost = [](uint8_t v) { return (unsigned char)(v ? 90 + v * 165 / 60 > 255 ? 255 : 90 + v * 165 / 60 : 35); };
    unsigned char rgb[3] = {35, 35, 35};
    if (dot) { rgb[0] = boost(g_simLed.r); rgb[1] = boost(g_simLed.g); rgb[2] = boost(g_simLed.b); }
    fwrite(rgb, 1, 3, f);
  }
  fclose(f);
}
// Record the boot intro as numbered frames (build_sim.sh turns them into a GIF).
static void recordIntro(int ms, int frameMs) {
  static int frame = 0;
  for (int t = 0; t < ms; t += frameMs) {
    for (int k = 0; k < frameMs; k += 5) { g_fake_ms += 5; shell.loop(); }
    char path[64]; snprintf(path, sizeof path, "intro/f%03d.ppm", frame++);
    FILE *f = fopen(path, "wb");
    fprintf(f, "P6\n128 128\n255\n");
    for (int y = 0; y < 128; y++) for (int x = 0; x < 128; x++) {
      uint16_t c = lcd.readPixel(x, y);
      unsigned char rgb[3] = {(unsigned char)(((c >> 11) & 31) * 255 / 31), (unsigned char)(((c >> 5) & 63) * 255 / 63), (unsigned char)((c & 31) * 255 / 31)};
      fwrite(rgb, 1, 3, f);
    }
    fclose(f);
  }
}
static void run(int ms, int step = 5) { for (int t = 0; t < ms; t += step) { g_fake_ms += step; shell.loop(); } }
static void tap(int pin) { g_pin[pin] = 0; run(150); g_pin[pin] = 1; run(200); }
static void hold(int pin) { g_pin[pin] = 0; run(900); g_pin[pin] = 1; run(200); }
int main() {
  for (int i = 0; i < 64; i++) g_pin[i] = 1;   // buttons up, USB present
  g_pin[PIN_CHARGING] = 1;                      // not charging
  shell.begin(); snap("intro_start");
  recordIntro(2800, 50);  // the whole intro, 20 frames a second
  run(600); snap("home");
  tap(PIN_BTN_RIGHT); run(100); snap("home_poke");
  run(2600);
  hold(PIN_BTN_RIGHT); run(300); snap("home_rec");
  run(2600); snap("home_recording");
  hold(PIN_BTN_RIGHT); run(300); snap("home_rec_stop");
  run(2600);
  tap(PIN_BTN_LEFT); snap("menu");
  // Menu: Tracker Watch, Followers, Shield, Cameras, Sound, BLE Nearby, WiFi Scan, Rig, Settings
  tap(PIN_BTN_RIGHT); snap("menu_next");
  hold(PIN_BTN_RIGHT); run(300); snap("followers_empty");
  tap(PIN_BTN_LEFT);
  tap(PIN_BTN_RIGHT); hold(PIN_BTN_RIGHT); run(300); snap("shield_quiet");
  tap(PIN_BTN_LEFT);
  tap(PIN_BTN_RIGHT); hold(PIN_BTN_RIGHT); run(300); snap("cameras_none");
  tap(PIN_BTN_LEFT);
  tap(PIN_BTN_RIGHT); hold(PIN_BTN_RIGHT); run(3000); snap("sound");
  tap(PIN_BTN_LEFT);
  tap(PIN_BTN_RIGHT); hold(PIN_BTN_RIGHT); run(2100); snap("nearby");
  tap(PIN_BTN_RIGHT); tap(PIN_BTN_RIGHT); snap("nearby_next");
  hold(PIN_BTN_RIGHT); run(1100); snap("nearby_detail");
  tap(PIN_BTN_LEFT); tap(PIN_BTN_LEFT);
  tap(PIN_BTN_RIGHT); hold(PIN_BTN_RIGHT); run(2100); snap("wifi");
  tap(PIN_BTN_RIGHT); tap(PIN_BTN_RIGHT); tap(PIN_BTN_RIGHT); tap(PIN_BTN_RIGHT); snap("wifi_next");
  hold(PIN_BTN_RIGHT); run(1100); snap("wifi_detail");
  tap(PIN_BTN_LEFT); tap(PIN_BTN_LEFT);
  tap(PIN_BTN_RIGHT); hold(PIN_BTN_RIGHT); run(2500); snap("rig_found");
  hold(PIN_BTN_RIGHT); run(300); snap("rig_paired_toast");
  run(5000); snap("rig_dashboard");
  tap(PIN_BTN_LEFT); snap("menu_back");
  tap(PIN_BTN_RIGHT); hold(PIN_BTN_RIGHT); snap("settings");
  hold(PIN_BTN_RIGHT); snap("settings_edit");
  tap(PIN_BTN_RIGHT); snap("settings_edit_next");
  hold(PIN_BTN_RIGHT); snap("settings_saved");
  tap(PIN_BTN_RIGHT); tap(PIN_BTN_RIGHT); snap("settings_led");
  tap(PIN_BTN_RIGHT); snap("settings_pocket");
  tap(PIN_BTN_RIGHT); snap("settings_soundcal");
  tap(PIN_BTN_RIGHT); tap(PIN_BTN_RIGHT); snap("settings_wifilog");
  hold(PIN_BTN_RIGHT); snap("settings_erase_ask");
  tap(PIN_BTN_LEFT); tap(PIN_BTN_RIGHT); snap("settings_mine");
  tap(PIN_BTN_RIGHT); snap("settings_reset");
  hold(PIN_BTN_RIGHT); snap("settings_reset_ask");
  tap(PIN_BTN_LEFT); tap(PIN_BTN_RIGHT); snap("settings_diag_sel");
  hold(PIN_BTN_RIGHT); snap("diagnostics");
  tap(PIN_BTN_LEFT); tap(PIN_BTN_RIGHT); hold(PIN_BTN_RIGHT); snap("about");
  tap(PIN_BTN_RIGHT); snap("about_more");
  hold(PIN_BTN_LEFT); snap("home_via_hold_left");
  hold(PIN_BTN_LEFT); run(125000); snap("home_sleepy");
  // Hidden-camera check at home (menu cursor is on Settings: 4 taps wrap to Cameras).
  tap(PIN_BTN_LEFT); for (int i = 0; i < 4; i++) tap(PIN_BTN_RIGHT);
  hold(PIN_BTN_RIGHT); run(300); snap("cameras_home");
  tap(PIN_BTN_LEFT); for (int i = 0; i < 5; i++) tap(PIN_BTN_RIGHT);  // back to Settings
  hold(PIN_BTN_LEFT);
  // Tracker Watch: open it from the menu (cursor is on Settings; wrap to Tracker).
  tap(PIN_BTN_LEFT); tap(PIN_BTN_RIGHT); hold(PIN_BTN_RIGHT); snap("tracker_watching");
  hold(PIN_BTN_RIGHT); snap("tracker_airtag_list");
  tap(PIN_BTN_LEFT); hold(PIN_BTN_LEFT);
  // ...time passes. From minute 8 you "drive": the speaker and a mystery
  // gadget ride along (Followers alert), the lost AirTag too (tracker alert).
  // Minute 10 a Flipper sprays pop-ups, 12 a kick-off flood, 15 an evil twin,
  // 17-19 a Flock camera by the road.
  const uint32_t end = g_fake_ms + 27 * 60 * 1000;
  while (g_fake_ms < 17 * 60000) run(1000, 40);   // drive until the Flock camera...
  while (!shell.toastShowing() && g_fake_ms < 19 * 60000) run(200, 40);
  run(1500, 40); snap("toast_flock");              // ...and catch the message
  run(end - g_fake_ms, 40);
  snap("tracker_alert_autoopen");
  hold(PIN_BTN_LEFT); snap("home_alert");
  tap(PIN_BTN_LEFT); hold(PIN_BTN_RIGHT); snap("tracker_alert");
  hold(PIN_BTN_RIGHT); snap("tracker_acknowledged");
  tap(PIN_BTN_LEFT); tap(PIN_BTN_RIGHT); hold(PIN_BTN_RIGHT); snap("followers");
  hold(PIN_BTN_RIGHT); snap("follower_detail");
  hold(PIN_BTN_RIGHT); snap("followers_after_mine");
  tap(PIN_BTN_LEFT); tap(PIN_BTN_RIGHT); hold(PIN_BTN_RIGHT); snap("shield_after");
  tap(PIN_BTN_LEFT); tap(PIN_BTN_RIGHT); hold(PIN_BTN_RIGHT); snap("cameras_after");
  hold(PIN_BTN_LEFT); run(500); snap("home_end");
  // Pocket mode: hold both buttons 1 s.
  g_pin[PIN_BTN_LEFT] = 0; g_pin[PIN_BTN_RIGHT] = 0; run(1200);
  g_pin[PIN_BTN_LEFT] = 1; g_pin[PIN_BTN_RIGHT] = 1; run(300); snap("pocket_locked");
  run(4000); snap("pocket_dark");
  tap(PIN_BTN_RIGHT); run(300); snap("pocket_peek");
  g_pin[PIN_BTN_LEFT] = 0; g_pin[PIN_BTN_RIGHT] = 0; run(1200);
  g_pin[PIN_BTN_LEFT] = 1; g_pin[PIN_BTN_RIGHT] = 1; run(300); snap("pocket_unlocked");
  printf("brightness now %u, depth ok\n", lcd.brightness);
  return 0;
}
