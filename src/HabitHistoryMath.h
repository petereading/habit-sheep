#pragma once
#include <cstdint>

inline uint32_t historySessions(uint32_t seconds, uint32_t interval, bool weekly, bool weekStart, uint32_t& carried) {
  if (!weekly || weekStart) carried = 0;
  const uint32_t before = carried;
  carried = seconds > UINT32_MAX - carried ? UINT32_MAX : carried + seconds;
  return interval ? carried / interval - before / interval : 0;
}
