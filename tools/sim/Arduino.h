#pragma once
// Host stand-in for the Arduino API so JerryOS screens can run on a PC.
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdarg.h>
#include <string>
#include <algorithm>
extern uint32_t g_fake_ms;
extern int g_pin[64];
inline uint32_t millis() { return g_fake_ms; }
inline void delay(uint32_t ms) { g_fake_ms += ms; }
inline long random(long hi) { return hi > 0 ? rand() % hi : 0; }
inline long random(long lo, long hi) { return hi > lo ? lo + rand() % (hi - lo) : lo; }
#ifndef PI
#define PI 3.14159265358979f
#endif
using std::max; using std::min;
#define INPUT 0
#define OUTPUT 1
#define INPUT_PULLUP 2
#define HIGH 1
#define LOW 0
enum { ADC_11db = 3 };
inline void pinMode(int, int) {}
inline void digitalWrite(int p, int v) { g_pin[p] = v; }
inline int digitalRead(int p) { return g_pin[p]; }
inline uint32_t analogReadMilliVolts(int) { return 1660; }
inline void analogSetPinAttenuation(int, int) {}
#define F(x) x
struct String : std::string { String(int v) : std::string(std::to_string(v)) {} const char *c_str() const { return std::string::c_str(); } String operator+(const char *s) const { String r(*this); r.append(s); return r; } private: String() {} };
struct SerialStub {
  void begin(int) {}
  int available() { return 0; }
  int read() { return -1; }
  void println(const char *s = "") { if (getenv("SIMLOG")) printf("%s\n", s); }
  void print(const char *s) { if (getenv("SIMLOG")) printf("%s", s); }
  int printf(const char *fmt, ...) { if (!getenv("SIMLOG")) return 0; va_list a; va_start(a, fmt); int n = vprintf(fmt, a); va_end(a); return n; }
};
extern SerialStub Serial;
struct EspStub { void restart() { printf("[sim] restart\n"); } uint32_t getFreeHeap() { return 280000; } uint32_t getPsramSize() { return 8386035; } const char *getChipModel() { return "ESP32-S3"; } int getChipRevision() { return 2; } };
extern EspStub ESP;
