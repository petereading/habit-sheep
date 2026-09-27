#pragma once

#include <Arduino.h>

#include <cstdint>
#include <string>

class HabitTimer {
 public:
  static HabitTimer& getInstance();

  bool start(const std::string& habitId);
  bool pause();
  bool resume();
  uint32_t stopAndLog();
  void cancel();

  bool isActive() const { return !habitId.empty(); }
  bool isRunning() const { return running; }
  bool isForHabit(const std::string& id) const { return isActive() && habitId == id; }
  const std::string& activeHabitId() const { return habitId; }
  uint32_t elapsedSeconds() const;

 private:
  std::string habitId;
  uint32_t accumulatedMs = 0;
  unsigned long startedAtMs = 0;
  bool running = false;
};

#define HABIT_TIMER HabitTimer::getInstance()
