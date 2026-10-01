#pragma once

#include <Arduino.h>
#include <PersistableStore.h>

#include <array>
#include <cstdint>
#include <string>

class HabitTimer : public PersistableStore<HabitTimer> {
 public:
  enum class Phase : uint8_t { Focus = 0, ShortBreak = 1, LongBreak = 2 };

  static const char* getFilePath() { return "/.crosspoint/habit_timers.json"; }
  void toJson(JsonDocument& doc) const;
  bool fromJson(JsonVariantConst doc);

  bool start(const std::string& habitId);
  bool pause(const std::string& habitId);
  bool resume(const std::string& habitId);
  bool skipShortBreak(const std::string& habitId);
  uint32_t stopAndLog(const std::string& habitId);
  void tick();

  bool isActive() const;
  bool isRunning() const;
  bool isForHabit(const std::string& id) const;
  bool isRunningFor(const std::string& id) const;
  bool hasOtherRunning(const std::string& id) const;
  uint32_t elapsedSecondsFor(const std::string& id) const;
  Phase phaseFor(const std::string& id) const;

 private:
  struct Session {
    std::string habitId;
    uint32_t accumulatedMs = 0;
    unsigned long startedAtMs = 0;
    unsigned long lastTargetCheckMs = 0;
    Phase phase = Phase::Focus;
    bool running = false;
  };

  // A paused timer follows its library habit even if the user swaps active Home slots.
  std::array<Session, 9> sessions{};

  Session* find(const std::string& id);
  const Session* find(const std::string& id) const;
  static int64_t currentEpoch();
  static uint32_t elapsedMs(const Session& session);
  void clear(Session& session);
};

#define HABIT_TIMER HabitTimer::getInstance()
