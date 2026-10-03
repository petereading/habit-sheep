#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "HabitSheepStore.h"

enum class HabitEventSource : uint8_t { Manual = 0, Timer = 1, Reader = 2 };

struct HabitDailyProgress {
  bool completed = false;
  uint16_t completionCount = 0;
  uint32_t durationSeconds = 0;
  uint16_t pomodoroSessions = 0;
};

class HabitEventLog {
 public:
  struct RewardNotice {
    char habitId[HabitSheepStore::MAX_ID_BYTES + 1]{};
    uint16_t grass = 0;
    uint8_t stock = 0;
  };
  static HabitEventLog& getInstance();
  bool takeReward(RewardNotice& notice, const std::string* habitId = nullptr);

  bool refreshToday();
  HabitDailyProgress progressForToday(const std::string& habitId);
  uint16_t completionCountForWeek(const std::string& habitId);
  bool appendCompletion(const std::string& habitId, HabitEventSource source = HabitEventSource::Manual);
  bool appendDurationSeconds(const std::string& habitId, uint32_t seconds,
                             HabitEventSource source = HabitEventSource::Manual);
  bool appendDurationSecondsOnDay(const std::string& habitId, uint32_t seconds, const char* day,
                                  HabitEventSource source = HabitEventSource::Reader);
  bool appendPomodoroFocus(const std::string& habitId, uint32_t seconds);

 private:
  struct CachedProgress {
    std::string habitId;
    HabitDailyProgress progress;
  };
  struct CachedWeekCount {
    std::string habitId;
    uint16_t count = 0;
  };

  std::string cachedDay;
  uint8_t cachedWeekStart = 255;
  std::vector<CachedProgress> cachedProgress;
  std::vector<CachedWeekCount> cachedWeekCounts;
  std::array<RewardNotice, HabitSheepStore::MAX_HABITS> pendingRewards{};
  void awardGrass(const std::string& habitId, uint8_t amount, const char* day = nullptr);

  bool currentDay(std::string& day, int64_t& epoch) const;
  std::string pathForDay(const std::string& day) const;
  CachedProgress& progressEntry(const std::string& habitId);
  uint16_t completionCountForDay(const std::string& habitId, const std::string& day) const;
  uint32_t durationSecondsForDay(const std::string& habitId, const std::string& day) const;
  bool appendEvent(const std::string& habitId, const char* type, uint32_t amount, const char* unit,
                   HabitEventSource source, const char* dayOverride = nullptr);
};

#define HABIT_EVENTS HabitEventLog::getInstance()
