#pragma once

#include <cstdint>
#include <string>
#include <vector>

enum class HabitEventSource : uint8_t { Manual = 0, Timer = 1, Reader = 2 };

struct HabitDailyProgress {
  bool completed = false;
  uint32_t durationSeconds = 0;
  uint16_t pomodoroSessions = 0;
};

class HabitEventLog {
 public:
  static HabitEventLog& getInstance();

  bool refreshToday();
  HabitDailyProgress progressForToday(const std::string& habitId);
  bool appendCompletion(const std::string& habitId, HabitEventSource source = HabitEventSource::Manual);
  bool appendDurationSeconds(const std::string& habitId, uint32_t seconds,
                             HabitEventSource source = HabitEventSource::Manual);
  bool appendPomodoroFocus(const std::string& habitId, uint32_t seconds);

 private:
  struct CachedProgress {
    std::string habitId;
    HabitDailyProgress progress;
  };

  std::string cachedDay;
  std::vector<CachedProgress> cachedProgress;

  bool currentDay(std::string& day, int64_t& epoch) const;
  std::string pathForDay(const std::string& day) const;
  CachedProgress& progressEntry(const std::string& habitId);
  bool appendEvent(const std::string& habitId, const char* type, uint32_t amount, const char* unit,
                   HabitEventSource source);
};

#define HABIT_EVENTS HabitEventLog::getInstance()
