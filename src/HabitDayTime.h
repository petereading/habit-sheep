#pragma once
#include <algorithm>
#include <cstdint>
#include <ctime>

inline uint32_t habitMillisAfterMidnight(uint32_t elapsed, const tm& local, bool running = true) {
  return running ? std::min<uint32_t>(elapsed, (local.tm_hour * 3600U + local.tm_min * 60U + local.tm_sec) * 1000U) : 0;
}
