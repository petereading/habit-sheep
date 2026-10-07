#pragma once
#include <ctime>
struct TestClock {
  bool available = true;
  tm now{};
  bool isAvailable() const { return available; }
  bool localTime(tm& out) const {
    out = now;
    return available;
  }
};
extern TestClock halClock;
