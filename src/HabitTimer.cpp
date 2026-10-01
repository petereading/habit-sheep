#include "HabitTimer.h"

#include <HalClock.h>

#include <algorithm>
#include <ctime>

#include "HabitEventLog.h"
#include "HabitSheepStore.h"

int64_t HabitTimer::currentEpoch() {
  struct tm local{};
  if (!halClock.isAvailable() || !halClock.localTime(local)) return 0;
  return static_cast<int64_t>(mktime(&local));
}

uint32_t HabitTimer::elapsedMs(const Session& session) {
  return session.accumulatedMs + (session.running ? millis() - session.startedAtMs : 0);
}

HabitTimer::Session* HabitTimer::find(const std::string& id) {
  for (auto& session : sessions) {
    if (!session.habitId.empty() && session.habitId == id) return &session;
  }
  return nullptr;
}

const HabitTimer::Session* HabitTimer::find(const std::string& id) const {
  for (const auto& session : sessions) {
    if (!session.habitId.empty() && session.habitId == id) return &session;
  }
  return nullptr;
}

void HabitTimer::clear(Session& session) { session = Session{}; }

bool HabitTimer::isActive() const {
  for (const auto& session : sessions) {
    if (!session.habitId.empty()) return true;
  }
  return false;
}

bool HabitTimer::isRunning() const {
  for (const auto& session : sessions) {
    if (session.running) return true;
  }
  return false;
}

bool HabitTimer::isForHabit(const std::string& id) const { return find(id) != nullptr; }

bool HabitTimer::isRunningFor(const std::string& id) const {
  const Session* session = find(id);
  return session && session->running;
}

bool HabitTimer::hasOtherRunning(const std::string& id) const {
  for (const auto& session : sessions) {
    if (session.running && session.habitId != id) return true;
  }
  return false;
}

uint32_t HabitTimer::elapsedSecondsFor(const std::string& id) const {
  const Session* session = find(id);
  return session ? elapsedMs(*session) / 1000 : 0;
}

HabitTimer::Phase HabitTimer::phaseFor(const std::string& id) const {
  const Session* session = find(id);
  return session ? session->phase : Phase::Focus;
}

bool HabitTimer::start(const std::string& id) {
  if (id.empty() || find(id) || isRunning() || !HABIT_SHEEP.findHabit(id)) return false;
  for (auto& session : sessions) {
    if (!session.habitId.empty()) continue;
    session.habitId = id;
    session.startedAtMs = millis();
    session.running = true;
    if (saveToFile()) return true;
    clear(session);
    return false;
  }
  return false;
}

bool HabitTimer::pause(const std::string& id) {
  Session* session = find(id);
  if (!session || !session->running) return false;
  session->accumulatedMs = elapsedMs(*session);
  session->running = false;
  return saveToFile();
}

bool HabitTimer::resume(const std::string& id) {
  Session* session = find(id);
  if (!session || session->running || isRunning()) return false;
  session->startedAtMs = millis();
  session->running = true;
  if (saveToFile()) return true;
  session->running = false;
  return false;
}

uint32_t HabitTimer::stopAndLog(const std::string& id) {
  Session* session = find(id);
  if (!session) return 0;
  const uint32_t seconds = elapsedMs(*session) / 1000;
  if (seconds > 0 && session->phase == Phase::Focus &&
      !HABIT_EVENTS.appendDurationSeconds(id, seconds, HabitEventSource::Timer))
    return 0;
  clear(*session);
  saveToFile();
  return seconds;
}

bool HabitTimer::skipShortBreak(const std::string& id) {
  Session* session = find(id);
  const HabitDefinition* habit = HABIT_SHEEP.findHabit(id);
  if (!session || !habit || habit->type != HabitType::Pomodoro || session->phase != Phase::ShortBreak ||
      hasOtherRunning(id))
    return false;
  const uint32_t previousMs = elapsedMs(*session);
  const bool wasRunning = session->running;
  session->phase = Phase::Focus;
  session->accumulatedMs = 0;
  session->startedAtMs = millis();
  session->running = true;
  if (saveToFile()) return true;
  session->phase = Phase::ShortBreak;
  session->accumulatedMs = previousMs;
  session->startedAtMs = millis();
  session->running = wasRunning;
  return false;
}

void HabitTimer::tick() {
  for (auto& session : sessions) {
    if (session.habitId.empty()) continue;
    const HabitDefinition* habit = HABIT_SHEEP.findHabit(session.habitId);
    if (!habit) {
      clear(session);
      saveToFile();
      return;
    }
    if (!session.running) continue;
    if (habit->type != HabitType::Pomodoro) {
      const uint32_t seconds = elapsedMs(session) / 1000;
      if (millis() - session.lastTargetCheckMs < 1000) continue;
      session.lastTargetCheckMs = millis();
      const uint32_t target = static_cast<uint32_t>(habit->targetMinutes) * 60;
      const uint32_t logged = HABIT_EVENTS.progressForToday(session.habitId).durationSeconds;
      if (habit->type == HabitType::Duration && logged < target && seconds >= target - logged) {
        if (!HABIT_EVENTS.appendDurationSeconds(session.habitId, seconds, HabitEventSource::Timer)) return;
        session.accumulatedMs = elapsedMs(session) - seconds * 1000;
        session.startedAtMs = millis();
        saveToFile();
      }
      continue;
    }
    const uint32_t targetSeconds =
        static_cast<uint32_t>(session.phase == Phase::Focus        ? habit->targetMinutes
                              : session.phase == Phase::ShortBreak ? habit->shortBreakMinutes
                                                                   : habit->longBreakMinutes) *
        60;
    if (elapsedMs(session) / 1000 < targetSeconds) continue;

    if (session.phase == Phase::Focus) {
      if (!HABIT_EVENTS.appendPomodoroFocus(session.habitId, targetSeconds)) return;
      const auto progress = HABIT_EVENTS.progressForToday(session.habitId);
      session.phase = progress.pomodoroSessions % habit->sessionsPerCycle == 0 ? Phase::LongBreak : Phase::ShortBreak;
      session.accumulatedMs = 0;
      session.running = false;
      saveToFile();
    } else {
      // A completed break waits for an explicit start of the next focus session.
      session.phase = Phase::Focus;
      session.accumulatedMs = 0;
      session.running = false;
      saveToFile();
    }
    return;
  }
}

void HabitTimer::toJson(JsonDocument& doc) const {
  doc["schema"] = 1;
  JsonArray values = doc["sessions"].to<JsonArray>();
  const int64_t now = currentEpoch();
  for (const auto& session : sessions) {
    if (session.habitId.empty()) continue;
    JsonObject item = values.add<JsonObject>();
    item["habitId"] = session.habitId;
    item["accumulatedMs"] = elapsedMs(session);
    item["running"] = session.running;
    item["savedEpoch"] = now;
    item["phase"] = static_cast<uint8_t>(session.phase);
  }
}

bool HabitTimer::fromJson(JsonVariantConst doc) {
  for (auto& session : sessions) clear(session);
  const int64_t now = currentEpoch();
  JsonArrayConst values = doc["sessions"].as<JsonArrayConst>();
  size_t index = 0;
  for (JsonObjectConst item : values) {
    if (index >= sessions.size()) break;
    const char* id = item["habitId"] | "";
    if (!*id || !HABIT_SHEEP.findHabit(id) || find(id)) continue;
    Session& session = sessions[index++];
    session.habitId = id;
    session.accumulatedMs = item["accumulatedMs"] | static_cast<uint32_t>(0);
    const int phase = item["phase"] | 0;
    session.phase = phase >= 0 && phase <= 2 ? static_cast<Phase>(phase) : Phase::Focus;
    const int64_t saved = item["savedEpoch"] | static_cast<int64_t>(0);
    if (saved > 0 && now > 0) {
      time_t savedTime = static_cast<time_t>(saved);
      time_t currentTime = static_cast<time_t>(now);
      struct tm savedLocal{};
      struct tm currentLocal{};
      localtime_r(&savedTime, &savedLocal);
      localtime_r(&currentTime, &currentLocal);
      if (savedLocal.tm_year != currentLocal.tm_year || savedLocal.tm_yday != currentLocal.tm_yday) {
        clear(session);
        --index;
        continue;
      }
    }
    // An interrupted activity or deep sleep never advances a habit timer.
    // The persisted elapsed value can be resumed explicitly by the user.
  }
  return true;
}
