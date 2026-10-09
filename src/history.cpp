// history.cpp - see history.h.

#include "history.h"

void History::add(int bt, int wifi, int db) {
  v_[BT][head_] = (int16_t)bt;
  v_[WIFI][head_] = (int16_t)wifi;
  v_[SOUND][head_] = (int16_t)db;
  head_ = (head_ + 1) % N;
  if (n_ < N) n_++;
}

int History::at(Series s, int i) const {
  if (i < 0 || i >= n_) return -1;
  return v_[s][(head_ - n_ + i + N) % N];
}

int History::max(Series s) const {
  int m = 0;
  for (int i = 0; i < n_; i++)
    if (at(s, i) > m) m = at(s, i);
  return m;
}
