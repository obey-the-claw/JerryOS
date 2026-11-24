// Runs the real JerryOS shell + screens on a PC with simulated buttons,
// saving a screenshot after each step.
#include "display.h"
#include "intro.h"
#include "screens.h"
#include "shell.h"
#include "status_led.h"
#include "wifi_scanner.h"
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
static void recordIntro(const char *dir, int ms, int frameMs) {
  int frame = 0;
  for (int t = 0; t < ms; t += frameMs) {
    for (int k = 0; k < frameMs; k += 5) { g_fake_ms += 5; shell.loop(); }
    char path[64]; snprintf(path, sizeof path, "%s/f%03d.ppm", dir, frame++);
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
  run(1500); snap("intro_boot");  // whichever one "random" picked
  tap(PIN_BTN_RIGHT); run(600); snap("home");
  // Each intro from the top: the whole story + "press any button", 20 frames a second.
  static const char *const dirs[INTRO_COUNT] = {"intro_jeep_jam", "intro_bluff_creek", "intro_selfie", "intro_cruisin"};
  static const int ms[INTRO_COUNT] = {5600, 7200, 6200, 5400};
  for (int i = 0; i < INTRO_COUNT; i++) {
    replayIntro(i); shell.open(introScreen());
    recordIntro(dirs[i], ms[i], 50);
    char name[40]; snprintf(name, sizeof name, "%s_end", dirs[i]); snap(name);
    tap(PIN_BTN_RIGHT); run(300);
  }
  // The dashes: tap R flips to the next one (and back round to Jerry = a poke).
  tap(PIN_BTN_RIGHT); run(100); snap("dash_radar_label");
  run(2600); snap("dash_radar");
  tap(PIN_BTN_RIGHT); run(1500); snap("dash_trail");
  tap(PIN_BTN_RIGHT); run(1500); snap("dash_allclear");
  tap(PIN_BTN_RIGHT); run(1500); snap("dash_trends");
  tap(PIN_BTN_RIGHT); run(1500); snap("dash_night");
  tap(PIN_BTN_RIGHT); run(100); snap("home_poke");
  run(2600);
  hold(PIN_BTN_RIGHT); run(300); snap("home_rec");
  run(2600); snap("home_recording");
  hold(PIN_BTN_RIGHT); run(300); snap("expedition_recap");  // REC stopped: the recap card
  tap(PIN_BTN_LEFT); run(2600); snap("home_rec_stop");
  tap(PIN_BTN_LEFT); snap("menu");
  // Menu: Trackers, Followers, Shield, Cameras, Nearby, Listen, Logbook, Rate Gear, Rig, Settings
  tap(PIN_BTN_RIGHT); snap("menu_next");
  hold(PIN_BTN_RIGHT); run(300); snap("followers_empty");
  tap(PIN_BTN_LEFT);
  tap(PIN_BTN_RIGHT); hold(PIN_BTN_RIGHT); run(300); snap("shield_quiet");
  tap(PIN_BTN_LEFT);
  tap(PIN_BTN_RIGHT); hold(PIN_BTN_RIGHT); run(300); snap("cameras_none");
  tap(PIN_BTN_LEFT);
  tap(PIN_BTN_RIGHT); hold(PIN_BTN_RIGHT); run(2100); snap("nearby");   // Bluetooth + Wi-Fi
  tap(PIN_BTN_RIGHT); tap(PIN_BTN_RIGHT); snap("nearby_next");
  hold(PIN_BTN_RIGHT); run(1100); snap("nearby_detail");
  tap(PIN_BTN_LEFT);
  for (int i = 0; i < 5; i++) tap(PIN_BTN_RIGHT);
  hold(PIN_BTN_RIGHT); run(1100); snap("nearby_detail_2");
  tap(PIN_BTN_LEFT); tap(PIN_BTN_LEFT);
  tap(PIN_BTN_RIGHT); hold(PIN_BTN_RIGHT); run(800); snap("listen_start");  // sound + knocks
  run(9000); snap("listen_heard");
  tap(PIN_BTN_LEFT);
  tap(PIN_BTN_RIGHT); hold(PIN_BTN_RIGHT); snap("logbook");
  hold(PIN_BTN_RIGHT); snap("badges");                        // the top row: Badges
  tap(PIN_BTN_LEFT);
  tap(PIN_BTN_RIGHT); hold(PIN_BTN_RIGHT); snap("expedition_open");  // a trip
  tap(PIN_BTN_LEFT); tap(PIN_BTN_LEFT);
  tap(PIN_BTN_RIGHT); hold(PIN_BTN_RIGHT); snap("rate");
  tap(PIN_BTN_RIGHT); tap(PIN_BTN_RIGHT); tap(PIN_BTN_RIGHT); snap("rate_pick_3");
  tap(PIN_BTN_RIGHT); tap(PIN_BTN_RIGHT); snap("rate_pick_5");
  // Hold R: the stamp. Recorded as an animation from the moment it fires.
  g_pin[PIN_BTN_RIGHT] = 0; run(720);
  recordIntro("anim_stamp", 200, 50); g_pin[PIN_BTN_RIGHT] = 1;
  snap("rate_stamp_slam");
  recordIntro("anim_stamp2", 2600, 50);
  snap("rate_stamped");
  tap(PIN_BTN_LEFT); tap(PIN_BTN_LEFT);
  tap(PIN_BTN_RIGHT); hold(PIN_BTN_RIGHT); run(2500); snap("rig_found");
  hold(PIN_BTN_RIGHT); run(300); snap("rig_paired_toast");
  run(11000); snap("rig_dashboard");  // after the "Rigged Up" badge toast
  tap(PIN_BTN_LEFT); snap("menu_back");
  tap(PIN_BTN_RIGHT); hold(PIN_BTN_RIGHT); snap("settings");
  hold(PIN_BTN_RIGHT); snap("settings_edit");
  tap(PIN_BTN_RIGHT); snap("settings_edit_next");
  hold(PIN_BTN_RIGHT); snap("settings_saved");
  tap(PIN_BTN_RIGHT); snap("settings_led");
  tap(PIN_BTN_RIGHT); snap("settings_pocket");
  tap(PIN_BTN_RIGHT); snap("settings_alert");
  tap(PIN_BTN_RIGHT); snap("settings_drive");
  tap(PIN_BTN_RIGHT); snap("settings_intro");
  tap(PIN_BTN_RIGHT); snap("settings_introtune");
  tap(PIN_BTN_RIGHT); snap("settings_soundcal");
  tap(PIN_BTN_RIGHT); snap("settings_wifiscan");
  tap(PIN_BTN_RIGHT); snap("settings_clock");
  hold(PIN_BTN_RIGHT); snap("clock");                          // set the time by hand
  for (int i = 0; i < 9; i++) tap(PIN_BTN_RIGHT);              // hour +9
  hold(PIN_BTN_RIGHT); tap(PIN_BTN_RIGHT); tap(PIN_BTN_RIGHT); // tens +2
  hold(PIN_BTN_RIGHT); snap("clock_editing");
  hold(PIN_BTN_RIGHT);                                         // ones, then month
  for (int i = 0; i < 9; i++) tap(PIN_BTN_RIGHT);              // October
  hold(PIN_BTN_RIGHT); for (int i = 0; i < 8; i++) tap(PIN_BTN_RIGHT);  // the 9th
  snap("clock_date");
  hold(PIN_BTN_RIGHT); run(300); snap("clock_saved");
  tap(PIN_BTN_RIGHT); snap("settings_wifilog");
  hold(PIN_BTN_RIGHT); snap("settings_erase_ask");
  tap(PIN_BTN_LEFT); tap(PIN_BTN_RIGHT); snap("settings_mine");
  tap(PIN_BTN_RIGHT); snap("settings_poweroff");
  hold(PIN_BTN_RIGHT); snap("settings_poweroff_ask");
  hold(PIN_BTN_RIGHT); run(300); snap("settings_poweroff_usb");  // on USB: it can't, and says so
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
  tap(PIN_BTN_LEFT); for (int i = 0; i < 6; i++) tap(PIN_BTN_RIGHT);  // back to Settings
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
  while (g_fake_ms < 9 * 60000) run(1000, 40);
  snap("drive_mode");                              // the rig says 40 mph: Trail Dash
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
  // The dashes again, after a busy half hour. (Parked for a minute first, so
  // drive mode hands the home screen back to Jerry.)
  run(61000, 40); snap("drive_mode_over");
  tap(PIN_BTN_RIGHT); run(1500); snap("dash_radar_busy");
  recordIntro("anim_radar", 2500, 50);  // one full sweep
  tap(PIN_BTN_RIGHT); run(1500); snap("dash_trail_later");
  tap(PIN_BTN_RIGHT); run(500); snap("dash_allclear_later");
  tap(PIN_BTN_RIGHT); run(500); snap("dash_trends_later");
  tap(PIN_BTN_RIGHT); run(500); snap("dash_night_later");
  tap(PIN_BTN_RIGHT); run(300);
  // Badges earned along the way (menu cursor is on Cameras: 3 taps to the Logbook).
  tap(PIN_BTN_LEFT); for (int i = 0; i < 3; i++) tap(PIN_BTN_RIGHT);
  hold(PIN_BTN_RIGHT); tap(PIN_BTN_RIGHT); hold(PIN_BTN_RIGHT); snap("badges_later");
  hold(PIN_BTN_LEFT); run(300);
  // Pocket mode: hold both buttons 1 s.
  g_pin[PIN_BTN_LEFT] = 0; g_pin[PIN_BTN_RIGHT] = 0; run(1200);
  g_pin[PIN_BTN_LEFT] = 1; g_pin[PIN_BTN_RIGHT] = 1; run(300); snap("pocket_locked");
  run(4000); snap("pocket_dark");
  tap(PIN_BTN_RIGHT); run(300); snap("pocket_peek");
  g_pin[PIN_BTN_LEFT] = 0; g_pin[PIN_BTN_RIGHT] = 0; run(1200);
  g_pin[PIN_BTN_LEFT] = 1; g_pin[PIN_BTN_RIGHT] = 1; run(300); snap("pocket_unlocked");
  // A longer REC with the rig's GPS: Bluetooth gets logged too (printed for checking).
  hold(PIN_BTN_RIGHT); run(12000, 40); hold(PIN_BTN_RIGHT); run(300); snap("recap_long");
  tap(PIN_BTN_LEFT);
  printf("--- ble log ---\n"); wifi::dumpBleLog(); printf("--- end ---\n");
  printf("brightness now %u, depth ok\n", lcd.brightness);
  return 0;
}
