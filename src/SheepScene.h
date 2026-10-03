#pragma once

#include <cstdint>
#include <ctime>

namespace sheepScene {
inline bool mealVisual(const tm& local) {
  return (local.tm_hour == 8 || local.tm_hour == 13 || local.tm_hour == 19) && local.tm_min < 5;
}

inline uint8_t pose(const tm& local, bool sleepScreen, bool resting, bool eating) {
  if (eating && mealVisual(local)) return 16 + (local.tm_min % 2);
  if (sleepScreen || resting) return 12 + ((local.tm_hour * 2 + local.tm_min / 30) % 4);
  return (local.tm_hour * 6 + local.tm_min / 10) % 12;
}

inline uint32_t sleepSeconds(const tm& local) {
  const uint32_t elapsed = local.tm_min * 60U + local.tm_sec;
  uint32_t seconds = 1800 - elapsed % 1800;
  if (mealVisual(local)) {
    const uint32_t untilRest = 300 - elapsed;
    if (untilRest < seconds) seconds = untilRest;
  }
  return seconds;
}
}  // namespace sheepScene
