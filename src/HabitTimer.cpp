#include "HabitTimer.h"

#include "HabitEventLog.h"

HabitTimer& HabitTimer::getInstance() {
  static HabitTimer instance;
  return instance;
}

bool HabitTimer::start(const std::string& id) {
  if (id.empty() || isActive()) return false;
  habitId = id;
  accumulatedMs = 0;
  startedAtMs = millis();
  running = true;
  return true;
}

bool HabitTimer::pause() {
  if (!isActive() || !running) return false;
  accumulatedMs += millis() - startedAtMs;
  running = false;
  return true;
}

bool HabitTimer::resume() {
  if (!isActive() || running) return false;
  startedAtMs = millis();
  running = true;
  return true;
}

uint32_t HabitTimer::elapsedSeconds() const {
  if (!isActive()) return 0;
  uint32_t total = accumulatedMs;
  if (running) total += millis() - startedAtMs;
  return total / 1000;
}

uint32_t HabitTimer::stopAndLog() {
  if (!isActive()) return 0;
  if (running) {
    accumulatedMs += millis() - startedAtMs;
    running = false;
  }

  const uint32_t seconds = accumulatedMs / 1000;
  if (seconds > 0) HABIT_EVENTS.appendDurationSeconds(habitId, seconds, HabitEventSource::Timer);
  cancel();
  return seconds;
}

void HabitTimer::cancel() {
  habitId.clear();
  accumulatedMs = 0;
  startedAtMs = 0;
  running = false;
}
