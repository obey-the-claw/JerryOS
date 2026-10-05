#pragma once
#include <map>
#include <string>
struct Preferences {
  static std::map<std::string, int> &store() { static std::map<std::string, int> m; return m; }
  bool begin(const char *, bool) { return true; }
  void end() {}
  uint8_t getUChar(const char *k, uint8_t d) { auto it = store().find(k); return it == store().end() ? d : it->second; }
  bool getBool(const char *k, bool d) { auto it = store().find(k); return it == store().end() ? d : it->second; }
  void putUChar(const char *k, uint8_t v) { store()[k] = v; }
  uint32_t getUInt(const char *k, uint32_t d) { auto it = store().find(k); return it == store().end() ? d : it->second; }
  void putUInt(const char *k, uint32_t v) { store()[k] = (int)v; }
  bool isKey(const char *k) { return store().count(k) > 0; }
  void putBool(const char *k, bool v) { store()[k] = v; }
};
