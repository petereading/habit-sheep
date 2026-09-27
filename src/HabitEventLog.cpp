#include "HabitEventLog.h"

#include <ArduinoJson.h>
#include <HalClock.h>
#include <HalStorage.h>
#include <Logging.h>
#include <esp_mac.h>
#include <esp_random.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <ctime>

#include "SheepStateStore.h"

namespace {
constexpr const char* EVENT_DIR = "/.crosspoint/habit_events";
constexpr size_t MAX_EVENT_LINE = 512;

const char* sourceName(const HabitEventSource source) {
  switch (source) {
    case HabitEventSource::Timer:
      return "timer";
    case HabitEventSource::Reader:
      return "reader";
    case HabitEventSource::Manual:
    default:
      return "manual";
  }
}
}  // namespace

HabitEventLog& HabitEventLog::getInstance() {
  static HabitEventLog instance;
  return instance;
}

bool HabitEventLog::currentDay(std::string& day, int64_t& epoch) const {
  struct tm local{};
  if (!halClock.isAvailable() || !halClock.localTime(local)) {
    day = "undated";
    epoch = 0;
    return false;
  }

  char dayBuffer[16];
  snprintf(dayBuffer, sizeof(dayBuffer), "%04d-%02d-%02d", local.tm_year + 1900, local.tm_mon + 1, local.tm_mday);
  day = dayBuffer;
  epoch = static_cast<int64_t>(mktime(&local));
  return true;
}

std::string HabitEventLog::pathForDay(const std::string& day) const {
  return std::string(EVENT_DIR) + "/" + day + ".jsonl";
}

HabitEventLog::CachedProgress& HabitEventLog::progressEntry(const std::string& habitId) {
  const auto it = std::find_if(cachedProgress.begin(), cachedProgress.end(),
                               [&](const CachedProgress& item) { return item.habitId == habitId; });
  if (it != cachedProgress.end()) return *it;
  cachedProgress.push_back(CachedProgress{habitId, {}});
  return cachedProgress.back();
}

bool HabitEventLog::refreshToday() {
  std::string day;
  int64_t ignoredEpoch = 0;
  currentDay(day, ignoredEpoch);
  cachedDay = day;
  cachedProgress.clear();

  const std::string path = pathForDay(day);
  if (!Storage.exists(path.c_str())) return true;

  HalFile file;
  if (!Storage.openFileForRead("HABIT", path, file)) return false;

  char line[MAX_EVENT_LINE];
  size_t used = 0;
  while (file.available()) {
    const int raw = file.read();
    if (raw < 0) break;
    const char ch = static_cast<char>(raw);
    if (ch != '\n' && used + 1 < sizeof(line)) {
      line[used++] = ch;
      continue;
    }

    if (used > 0) {
      line[used] = '\0';
      JsonDocument doc;
      if (!deserializeJson(doc, line)) {
        const char* habitId = doc["habit_id"] | "";
        const char* type = doc["type"] | "";
        const uint32_t amount = doc["amount"] | static_cast<uint32_t>(0);
        if (*habitId) {
          auto& progress = progressEntry(habitId).progress;
          if (strcmp(type, "completion") == 0) {
            progress.completed = true;
          } else if (strcmp(type, "duration") == 0) {
            progress.durationSeconds += amount;
          } else if (strcmp(type, "pomodoro") == 0) {
            progress.durationSeconds += amount;
            ++progress.pomodoroSessions;
          }
        }
      }
    }
    used = 0;

    // An overlong/corrupt line is discarded up to its newline rather than
    // growing a heap buffer on low-memory ESP32-C3 devices.
    if (ch != '\n') {
      while (file.available()) {
        const int skip = file.read();
        if (skip < 0 || static_cast<char>(skip) == '\n') break;
      }
    }
  }
  file.close();
  return true;
}

HabitDailyProgress HabitEventLog::progressForToday(const std::string& habitId) {
  std::string day;
  int64_t ignoredEpoch = 0;
  currentDay(day, ignoredEpoch);
  if (day != cachedDay) refreshToday();

  const auto it = std::find_if(cachedProgress.begin(), cachedProgress.end(),
                               [&](const CachedProgress& item) { return item.habitId == habitId; });
  return it == cachedProgress.end() ? HabitDailyProgress{} : it->progress;
}

bool HabitEventLog::appendEvent(const std::string& habitId, const char* type, const uint32_t amount, const char* unit,
                                const HabitEventSource source) {
  if (habitId.empty() || !type || !unit) return false;

  std::string day;
  int64_t epoch = 0;
  currentDay(day, epoch);
  if (day != cachedDay) refreshToday();

  if (!Storage.ensureDirectoryExists(EVENT_DIR)) {
    LOG_ERR("HABIT", "Cannot create habit event directory");
    return false;
  }

  uint8_t mac[6] = {0};
  esp_read_mac(mac, ESP_MAC_WIFI_STA);
  char eventId[48];
  snprintf(eventId, sizeof(eventId), "%02X%02X%02X%02X%02X%02X-%08lX-%08lX", mac[0], mac[1], mac[2], mac[3], mac[4],
           mac[5], static_cast<unsigned long>(esp_random()), static_cast<unsigned long>(esp_random()));

  JsonDocument doc;
  doc["event_id"] = eventId;
  doc["habit_id"] = habitId;
  doc["day"] = day;
  doc["timestamp"] = epoch;
  doc["type"] = type;
  doc["amount"] = amount;
  doc["unit"] = unit;
  doc["source"] = sourceName(source);

  const std::string path = pathForDay(day);
  HalFile file = Storage.open(path.c_str(), O_WRONLY | O_CREAT | O_APPEND);
  if (!file) {
    LOG_ERR("HABIT", "Cannot append habit event");
    return false;
  }
  const size_t written = serializeJson(doc, file);
  file.write(static_cast<uint8_t>('\n'));
  file.flush();
  file.close();
  if (written == 0) return false;

  auto& progress = progressEntry(habitId).progress;
  if (strcmp(type, "completion") == 0) {
    progress.completed = true;
    SHEEP_STATE.recordCompletion();
  } else if (strcmp(type, "duration") == 0) {
    progress.durationSeconds += amount;
    SHEEP_STATE.recordDuration(amount);
  } else if (strcmp(type, "pomodoro") == 0) {
    progress.durationSeconds += amount;
    ++progress.pomodoroSessions;
    SHEEP_STATE.recordDuration(amount);
  }
  return true;
}

bool HabitEventLog::appendCompletion(const std::string& habitId, const HabitEventSource source) {
  const auto current = progressForToday(habitId);
  if (current.completed) return true;
  return appendEvent(habitId, "completion", 1, "completion", source);
}

bool HabitEventLog::appendDurationSeconds(const std::string& habitId, const uint32_t seconds,
                                          const HabitEventSource source) {
  if (seconds == 0) return false;
  return appendEvent(habitId, "duration", seconds, "seconds", source);
}

bool HabitEventLog::appendPomodoroFocus(const std::string& habitId, const uint32_t seconds) {
  if (seconds == 0) return false;
  return appendEvent(habitId, "pomodoro", seconds, "seconds", HabitEventSource::Timer);
}
