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

#include "HabitReset.h"
#include "HabitSheepStore.h"
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
  cachedWeekCounts.clear();
  cachedProgress.reserve(HabitSheepStore::MAX_HABITS);
  cachedWeekCounts.reserve(HabitSheepStore::MAX_HABITS);

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
            ++progress.completionCount;
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

bool HabitEventLog::progressOnDay(const std::string& habitId, const char* day, HabitDailyProgress& result, char* line,
                                  const size_t capacity) const {
  result = {};
  if (!day || !line || capacity < 2) return false;
  const std::string path = pathForDay(day);
  if (!Storage.exists(path.c_str())) return true;
  HalFile file;
  if (!Storage.openFileForRead("HABIT", path, file)) return false;
  size_t used = 0;
  bool oversized = false;
  const auto parse = [&] {
    if (!used || oversized) return;
    line[used] = '\0';
    JsonDocument doc;
    if (deserializeJson(doc, line) || habitId != (doc["habit_id"] | "")) return;
    const char* type = doc["type"] | "";
    const uint32_t amount = doc["amount"] | static_cast<uint32_t>(0);
    if (strcmp(type, "completion") == 0) {
      result.completed = true;
      if (result.completionCount < UINT16_MAX) ++result.completionCount;
    } else if (strcmp(type, "duration") == 0 || strcmp(type, "pomodoro") == 0) {
      result.durationSeconds =
          amount > UINT32_MAX - result.durationSeconds ? UINT32_MAX : result.durationSeconds + amount;
      if (strcmp(type, "pomodoro") == 0 && result.pomodoroSessions < UINT16_MAX) ++result.pomodoroSessions;
    }
  };
  while (file.available()) {
    const int raw = file.read();
    if (raw < 0) return false;
    if (raw == '\n') {
      parse();
      used = 0;
      oversized = false;
    } else if (used + 1 < capacity && !oversized)
      line[used++] = static_cast<char>(raw);
    else
      oversized = true;
  }
  parse();
  return true;
}

uint16_t HabitEventLog::completionCountForDay(const std::string& habitId, const std::string& day) const {
  const std::string path = pathForDay(day);
  if (!Storage.exists(path.c_str())) return 0;
  HalFile file;
  if (!Storage.openFileForRead("HABIT", path, file)) return 0;
  uint16_t count = 0;
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
    if (ch == '\n' && used > 0) {
      line[used] = '\0';
      JsonDocument doc;
      if (!deserializeJson(doc, line) && habitId == (doc["habit_id"] | "") &&
          strcmp(doc["type"] | "", "completion") == 0)
        ++count;
    }
    used = 0;
    if (ch != '\n') {
      while (file.available()) {
        const int skip = file.read();
        if (skip < 0 || static_cast<char>(skip) == '\n') break;
      }
    }
  }
  return count;
}

uint32_t HabitEventLog::durationSecondsForDay(const std::string& habitId, const std::string& day) const {
  const std::string path = pathForDay(day);
  HalFile file;
  if (!Storage.exists(path.c_str()) || !Storage.openFileForRead("HABIT", path, file)) return 0;
  uint32_t seconds = 0;
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
    if (ch == '\n' && used > 0) {
      line[used] = '\0';
      JsonDocument doc;
      if (!deserializeJson(doc, line) && habitId == (doc["habit_id"] | "") &&
          strcmp(doc["type"] | "", "duration") == 0) {
        const uint32_t amount = doc["amount"] | static_cast<uint32_t>(0);
        seconds = amount > UINT32_MAX - seconds ? UINT32_MAX : seconds + amount;
      }
    }
    used = 0;
    if (ch != '\n') {
      while (file.available()) {
        const int skip = file.read();
        if (skip < 0 || static_cast<char>(skip) == '\n') break;
      }
    }
  }
  file.close();
  return seconds;
}

uint16_t HabitEventLog::completionCountForWeek(const std::string& habitId) {
  std::string day;
  int64_t ignoredEpoch = 0;
  if (!currentDay(day, ignoredEpoch)) return progressForToday(habitId).completionCount;
  if (day != cachedDay) refreshToday();
  if (cachedWeekStart != HABIT_SHEEP.getWeekStart()) {
    cachedWeekCounts.clear();
    cachedWeekStart = HABIT_SHEEP.getWeekStart();
  }
  const auto cached = std::find_if(cachedWeekCounts.begin(), cachedWeekCounts.end(),
                                   [&](const CachedWeekCount& item) { return item.habitId == habitId; });
  if (cached != cachedWeekCounts.end() && cached->countValid) return cached->count;

  struct tm local{};
  if (!halClock.localTime(local)) return progressForToday(habitId).completionCount;
  const int daysSinceMonday = (local.tm_wday + 7 - HABIT_SHEEP.getWeekStart()) % 7;
  uint16_t count = progressForToday(habitId).completionCount;
  for (int offset = 1; offset <= daysSinceMonday; ++offset) {
    struct tm previous = local;
    previous.tm_mday -= offset;
    mktime(&previous);
    char previousDay[16];
    strftime(previousDay, sizeof(previousDay), "%Y-%m-%d", &previous);
    count += completionCountForDay(habitId, previousDay);
  }
  if (cached != cachedWeekCounts.end()) {
    cached->count = count;
    cached->countValid = true;
  } else {
    cachedWeekCounts.push_back({habitId, count});
  }
  return count;
}

uint32_t HabitEventLog::durationSecondsForPeriod(const HabitDefinition& habit, const char* day) {
  std::string today;
  int64_t epoch = 0;
  currentDay(today, epoch);
  if (today != cachedDay) refreshToday();
  const bool current = !day || today == day;
  if (habit.period != HabitPeriod::Weekly || today == "undated")
    return current ? progressForToday(habit.id).durationSeconds : durationSecondsForDay(habit.id, day);
  if (cachedWeekStart != HABIT_SHEEP.getWeekStart()) {
    cachedWeekCounts.clear();
    cachedWeekStart = HABIT_SHEEP.getWeekStart();
  }
  auto cached = std::find_if(cachedWeekCounts.begin(), cachedWeekCounts.end(),
                             [&](const CachedWeekCount& item) { return item.habitId == habit.id; });
  if (current && cached != cachedWeekCounts.end() && cached->durationValid) return cached->seconds;
  struct tm local{};
  if (day) {
    int year = 0, month = 0, date = 0;
    if (sscanf(day, "%d-%d-%d", &year, &month, &date) != 3) return durationSecondsForDay(habit.id, day);
    local.tm_year = year - 1900;
    local.tm_mon = month - 1;
    local.tm_mday = date;
    local.tm_isdst = -1;
    mktime(&local);
  } else if (!halClock.localTime(local)) {
    return progressForToday(habit.id).durationSeconds;
  }
  local.tm_mday -= (local.tm_wday + 7 - HABIT_SHEEP.getWeekStart()) % 7;
  mktime(&local);
  uint32_t seconds = 0;
  for (int offset = 0; offset < 7; ++offset) {
    struct tm date = local;
    date.tm_mday += offset;
    mktime(&date);
    char dateText[16];
    strftime(dateText, sizeof(dateText), "%Y-%m-%d", &date);
    const uint32_t amount =
        today == dateText ? progressForToday(habit.id).durationSeconds : durationSecondsForDay(habit.id, dateText);
    seconds = amount > UINT32_MAX - seconds ? UINT32_MAX : seconds + amount;
  }
  if (current) {
    if (cached == cachedWeekCounts.end()) {
      cachedWeekCounts.push_back({habit.id, 0, seconds, true, false});
    } else {
      cached->seconds = seconds;
      cached->durationValid = true;
    }
  }
  return seconds;
}

bool HabitEventLog::appendEvent(const std::string& habitId, const char* type, const uint32_t amount, const char* unit,
                                const HabitEventSource source, const char* dayOverride) {
  if (habitResetPending()) return false;
  if (!HABIT_SHEEP.isEnabled() || habitId.empty() || !type || !unit) return false;

  std::string today;
  int64_t epoch = 0;
  currentDay(today, epoch);
  if (today != cachedDay) refreshToday();
  const std::string day = dayOverride && *dayOverride ? dayOverride : today;

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

  HabitDailyProgress* progress = day == today ? &progressEntry(habitId).progress : nullptr;
  if (strcmp(type, "completion") == 0) {
    if (progress) {
      progress->completed = true;
      ++progress->completionCount;
      for (auto& cached : cachedWeekCounts) {
        if (cached.habitId == habitId) ++cached.count;
      }
    }
  } else if (strcmp(type, "duration") == 0) {
    if (progress)
      progress->durationSeconds =
          amount > UINT32_MAX - progress->durationSeconds ? UINT32_MAX : progress->durationSeconds + amount;
    for (auto& cached : cachedWeekCounts) cached.durationValid = false;
  } else if (strcmp(type, "pomodoro") == 0) {
    if (progress) {
      progress->durationSeconds += amount;
      ++progress->pomodoroSessions;
    }
  }
  return true;
}

bool HabitEventLog::appendCompletion(const std::string& habitId, const HabitEventSource source) {
  const HabitDefinition* habit = HABIT_SHEEP.findHabit(habitId);
  if (!habit || habit->type != HabitType::Completion) return false;
  if (!appendEvent(habitId, "completion", 1, "completion", source)) return false;
  awardGrass(habitId, 1);
  return true;
}

bool HabitEventLog::appendDurationSeconds(const std::string& habitId, const uint32_t seconds,
                                          const HabitEventSource source) {
  if (seconds == 0) return false;
  const HabitDefinition* habit = HABIT_SHEEP.findHabit(habitId);
  const uint32_t before = habit ? durationSecondsForPeriod(*habit) : progressForToday(habitId).durationSeconds;
  if (!appendEvent(habitId, "duration", seconds, "seconds", source)) return false;
  if (habit && habit->type == HabitType::Duration && habit->targetMinutes) {
    const uint32_t session = static_cast<uint32_t>(habit->targetMinutes) * 60;
    const uint64_t earned = (static_cast<uint64_t>(before) + seconds) / session - before / session;
    if (earned) awardGrass(habitId, static_cast<uint8_t>(std::min<uint64_t>(21, earned)), nullptr, false);
  }
  return true;
}

bool HabitEventLog::appendDurationSecondsOnDay(const std::string& habitId, const uint32_t seconds, const char* day,
                                               const HabitEventSource source) {
  if (seconds == 0 || !day || !*day) return false;
  const HabitDefinition* habit = HABIT_SHEEP.findHabit(habitId);
  std::string today;
  int64_t ignoredEpoch = 0;
  currentDay(today, ignoredEpoch);
  const uint32_t before = habit          ? durationSecondsForPeriod(*habit, day)
                          : day == today ? progressForToday(habitId).durationSeconds
                                         : durationSecondsForDay(habitId, day);
  if (!appendEvent(habitId, "duration", seconds, "seconds", source, day)) return false;
  if (habit && habit->type == HabitType::Duration && habit->targetMinutes) {
    const uint32_t session = static_cast<uint32_t>(habit->targetMinutes) * 60;
    const uint64_t earned = (static_cast<uint64_t>(before) + seconds) / session - before / session;
    if (earned) awardGrass(habitId, static_cast<uint8_t>(std::min<uint64_t>(21, earned)), day, false);
  }
  return true;
}

bool HabitEventLog::appendPomodoroFocus(const std::string& habitId, const uint32_t seconds) {
  if (seconds == 0) return false;
  const auto* habit = HABIT_SHEEP.findHabit(habitId);
  if (!habit || habit->type != HabitType::Pomodoro) return false;
  if (!appendEvent(habitId, "pomodoro", seconds, "seconds", HabitEventSource::Timer)) return false;
  awardGrass(habitId, 1);
  return true;
}

void HabitEventLog::awardGrass(const std::string& habitId, const uint8_t amount, const char* day, const bool notify) {
  const uint8_t earned = SHEEP_STATE.addGrass(amount, day);
  if (!notify) return;
  RewardNotice* slot = nullptr;
  for (auto& reward : pendingRewards) {
    if (habitId == reward.habitId) {
      slot = &reward;
      break;
    }
    if (!*reward.habitId && !slot) slot = &reward;
  }
  if (!slot) slot = &pendingRewards[0];
  if (habitId != slot->habitId) *slot = RewardNotice{};
  snprintf(slot->habitId, sizeof(slot->habitId), "%s", habitId.c_str());
  slot->grass = static_cast<uint16_t>(std::min<unsigned>(UINT16_MAX, slot->grass + earned));
  slot->stock = SHEEP_STATE.getGrassStock();
}

bool HabitEventLog::takeReward(RewardNotice& notice, const std::string* habitId) {
  for (auto& reward : pendingRewards) {
    if (!*reward.habitId || (habitId && *habitId != reward.habitId)) continue;
    notice = reward;
    reward = RewardNotice{};
    return true;
  }
  return false;
}

void HabitEventLog::clearCacheAfterReset() {
  cachedDay.clear();
  cachedWeekStart = 255;
  cachedProgress.clear();
  cachedWeekCounts.clear();
  pendingRewards = {};
}
