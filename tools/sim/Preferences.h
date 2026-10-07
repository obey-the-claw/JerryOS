#pragma once
#include <map>
#include <string>
#include <vector>
#include <string.h>
struct Preferences {
  static std::map<std::string, int> &store() { static std::map<std::string, int> m; return m; }
  bool begin(const char *, bool) { return true; }
  void end() {}
  uint8_t getUChar(const char *k, uint8_t d) { auto it = store().find(k); return it == store().end() ? d : it->second; }
  bool getBool(const char *k, bool d) { auto it = store().find(k); return it == store().end() ? d : it->second; }
  void putUChar(const char *k, uint8_t v) { store()[k] = v; }
  uint32_t getUInt(const char *k, uint32_t d) { auto it = store().find(k); return it == store().end() ? d : it->second; }
  void putUInt(const char *k, uint32_t v) { store()[k] = (int)v; }
  bool isKey(const char *k) { return store().count(k) > 0 || blobs().count(k) > 0; }
  static std::map<std::string, std::vector<uint8_t>> &blobs() { static std::map<std::string, std::vector<uint8_t>> m; return m; }
  size_t getBytes(const char *k, void *buf, size_t n) {
    auto it = blobs().find(k); if (it == blobs().end()) return 0;
    size_t c = it->second.size() < n ? it->second.size() : n; memcpy(buf, it->second.data(), c); return c;
  }
  size_t putBytes(const char *k, const void *buf, size_t n) {
    blobs()[k].assign((const uint8_t *)buf, (const uint8_t *)buf + n); return n;
  }
  void putBool(const char *k, bool v) { store()[k] = v; }
};
