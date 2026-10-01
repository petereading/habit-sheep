#pragma once

#include <HalClock.h>

#include <cstdint>

// Tracks calendar minutes, including date and timezone changes, without
// requesting a display refresh on every main-loop iteration.
class HabitClock {
 public:
  bool changed() {
    tm local{};
    const bool available = halClock.isAvailable() && halClock.localTime(local);
    const uint32_t minute = available
                                ? static_cast<uint32_t>(local.tm_year) * 527040U +
                                      static_cast<uint32_t>(local.tm_yday) * 1440U + local.tm_hour * 60U + local.tm_min
                                : UINT32_MAX;
    if (minute == lastMinute) return false;
    lastMinute = minute;
    return true;
  }

 private:
  uint32_t lastMinute = UINT32_MAX;
};
