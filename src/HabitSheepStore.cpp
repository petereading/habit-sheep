#include "HabitSheepStore.h"

#include <Logging.h>

#include <algorithm>
#include <cstring>
#include <string_view>
#include <utility>

#include "HabitReset.h"
#include "HabitTimer.h"
#include "SheepStateStore.h"

namespace {
constexpr uint8_t HABIT_SHEEP_SCHEMA_VERSION = 8;

const char* habitTypeName(const HabitType type) {
  if (type == HabitType::Pomodoro) return "pomodoro";
  return type == HabitType::Duration ? "duration" : "completion";
}

HabitType parseHabitType(const char* value) {
  if (value && std::string_view(value) == "pomodoro") return HabitType::Pomodoro;
  return value && std::string_view(value) == "duration" ? HabitType::Duration : HabitType::Completion;
}
}  // namespace

HabitSheepStore::HabitSheepStore() { seedDefaultHabits(); }

void HabitSheepStore::seedDefaultHabits() {
  habits.reserve(2);
  habits.push_back({"reading", "Reading", HabitType::Duration, 30, true});
  habits.push_back({"pomodoro", "Pomodoro", HabitType::Pomodoro, 25, false});
  activeHabitIds[0] = "reading";
  activeHabitIds[1] = "pomodoro";
}

bool HabitSheepStore::validId(const std::string& id) { return !id.empty() && id.size() <= MAX_ID_BYTES; }

bool HabitSheepStore::validName(const std::string& name) { return !name.empty() && name.size() <= MAX_NAME_BYTES; }

bool HabitSheepStore::isActiveElsewhere(const size_t slot, const std::string& habitId) const {
  for (size_t i = 0; i < activeHabitIds.size(); ++i) {
    if (i != slot && activeHabitIds[i] == habitId) return true;
  }
  return false;
}

void HabitSheepStore::toJson(JsonDocument& doc) const {
  doc["schema"] = HABIT_SHEEP_SCHEMA_VERSION;
  doc["sheepName"] = sheepName;
  doc["enabled"] = enabled;
  doc["weekStart"] = weekStart;
  doc["orientation"] = orientation;
  doc["homeFocus"] = homeFocus;
  doc["pausedSleepScreen"] = pausedSleepScreen;

  JsonArray habitArray = doc["habits"].to<JsonArray>();
  for (const auto& habit : habits) {
    JsonObject obj = habitArray.add<JsonObject>();
    obj["id"] = habit.id;
    obj["name"] = habit.name;
    obj["type"] = habitTypeName(habit.type);
    obj["targetMinutes"] = habit.targetMinutes;
    obj["readingIntegration"] = habit.readingIntegration;
    obj["shortBreakMinutes"] = habit.shortBreakMinutes;
    obj["longBreakMinutes"] = habit.longBreakMinutes;
    obj["sessionsPerCycle"] = habit.sessionsPerCycle;
    obj["period"] = habit.period == HabitPeriod::Weekly ? "weekly" : "daily";
    obj["targetCount"] = habit.targetCount;
    obj["icon"] = habit.icon;
  }

  JsonArray activeArray = doc["activeHabitIds"].to<JsonArray>();
  for (const auto& id : activeHabitIds) activeArray.add(id);
}

bool HabitSheepStore::fromJson(JsonVariantConst doc) {
  sheepName.clear();
  habits.clear();
  activeHabitIds.fill("");
  sleepSceneEnabled = doc["sleepSceneEnabled"] | true;
  enabled = doc["enabled"] | true;
  weekStart = doc["weekStart"] | static_cast<uint8_t>(1);
  if (weekStart > 6) weekStart = 1;
  orientation = doc["orientation"] | static_cast<uint8_t>(0);
  if (orientation > 3) orientation = 0;
  homeFocus = doc["homeFocus"] | static_cast<uint8_t>(0);
  if (homeFocus > 2) homeFocus = 0;
  pausedSleepScreen = doc["pausedSleepScreen"] | static_cast<uint8_t>(255);

  const char* storedSheepName = doc["sheepName"] | "";
  if (storedSheepName && strlen(storedSheepName) <= MAX_NAME_BYTES) sheepName = storedSheepName;

  const uint8_t schema = doc["schema"] | static_cast<uint8_t>(1);
  JsonArrayConst habitArray = doc["habits"].as<JsonArrayConst>();
  habits.reserve(std::min(habitArray.size(), MAX_HABITS));
  for (JsonObjectConst obj : habitArray) {
    if (habits.size() >= MAX_HABITS) break;

    HabitDefinition habit;
    const char* id = obj["id"] | "";
    const char* name = obj["name"] | "";
    habit.id = id;
    habit.name = name;
    if (!validId(habit.id) || !validName(habit.name) || findHabit(habit.id)) continue;

    habit.type = parseHabitType(obj["type"] | "completion");
    if (schema < 3 && habit.id == "pomodoro" && habit.type == HabitType::Duration) {
      habit.type = HabitType::Pomodoro;
      requestResave();
    }
    habit.targetMinutes = obj["targetMinutes"] | static_cast<uint16_t>(0);
    habit.readingIntegration = obj["readingIntegration"] | false;
    habit.shortBreakMinutes = obj["shortBreakMinutes"] | static_cast<uint16_t>(5);
    habit.longBreakMinutes = obj["longBreakMinutes"] | static_cast<uint16_t>(15);
    habit.sessionsPerCycle = obj["sessionsPerCycle"] | static_cast<uint8_t>(4);
    habit.period = std::string_view(obj["period"] | "daily") == "weekly" ? HabitPeriod::Weekly : HabitPeriod::Daily;
    habit.targetCount = obj["targetCount"] | static_cast<uint8_t>(1);
    habit.icon = obj["icon"] | static_cast<uint8_t>(255);
    if (habit.icon >= 24) habit.icon = 255;
    if (habit.targetCount == 0 || habit.targetCount > 99) habit.targetCount = 1;
    if (habit.type != HabitType::Completion && habit.targetMinutes == 0) continue;
    if (habit.type == HabitType::Pomodoro &&
        (habit.shortBreakMinutes == 0 || habit.longBreakMinutes == 0 || habit.sessionsPerCycle == 0))
      continue;
    if (habit.type == HabitType::Completion) {
      habit.targetMinutes = 0;
      habit.readingIntegration = false;
    }
    habits.push_back(std::move(habit));
  }

  JsonArrayConst activeArray = doc["activeHabitIds"].as<JsonArrayConst>();
  const size_t count = std::min(activeArray.size(), activeHabitIds.size());
  for (size_t slot = 0; slot < count; ++slot) {
    const char* id = activeArray[slot] | "";
    if (!id || *id == '\0') continue;
    const std::string candidate(id);
    if (findHabit(candidate) && !isActiveElsewhere(slot, candidate)) activeHabitIds[slot] = candidate;
  }

  // Upgrade the original empty first-run configuration once. Schema 2 keeps
  // an intentionally emptied library empty on subsequent loads.
  if (schema < 2 && habits.empty()) {
    seedDefaultHabits();
    requestResave();
  }

  LOG_DBG("HABIT", "Habit Sheep loaded (%u habits)", static_cast<unsigned>(habits.size()));
  return true;
}

const HabitDefinition* HabitSheepStore::findHabit(const std::string& id) const {
  const auto it =
      std::find_if(habits.begin(), habits.end(), [&](const HabitDefinition& habit) { return habit.id == id; });
  return it == habits.end() ? nullptr : &*it;
}

bool HabitSheepStore::setOrientation(const uint8_t value) {
  if (value > 3) return false;
  if (value == orientation) return true;
  const uint8_t previous = orientation;
  orientation = value;
  if (saveToFile()) return true;
  orientation = previous;
  return false;
}

bool HabitSheepStore::setHomeFocus(const uint8_t value) {
  if (value > 2) return false;
  if (value == homeFocus) return true;
  const uint8_t previous = homeFocus;
  homeFocus = value;
  if (saveToFile()) return true;
  homeFocus = previous;
  return false;
}

int HabitSheepStore::homeSelection(bool hasBook) const {
  if (!enabled || homeFocus == 2) return hasBook ? 4 : 5;
  if (homeFocus == 1) return 0;
  for (size_t slot = 0; slot < activeHabitIds.size(); ++slot)
    if (!activeHabitIds[slot].empty()) return static_cast<int>(slot) + 1;
  return 1;
}

bool HabitSheepStore::setSheepName(const std::string& name) {
  if (!name.empty() && name.size() > MAX_NAME_BYTES) return false;
  sheepName = name;
  return saveToFile();
}

bool HabitSheepStore::upsertHabit(const HabitDefinition& habit) {
  if (!validId(habit.id) || !validName(habit.name)) return false;
  if (habit.targetCount == 0 || habit.targetCount > 99) return false;
  if (habit.type != HabitType::Completion && habit.targetMinutes == 0) return false;
  if (habit.type == HabitType::Pomodoro &&
      (habit.shortBreakMinutes == 0 || habit.longBreakMinutes == 0 || habit.sessionsPerCycle == 0))
    return false;

  HabitDefinition normalized = habit;
  if (normalized.icon >= 24) normalized.icon = 255;
  if (normalized.type == HabitType::Completion) {
    normalized.targetMinutes = 0;
    normalized.readingIntegration = false;
  } else if (normalized.type == HabitType::Pomodoro) {
    normalized.readingIntegration = false;
  }

  auto it =
      std::find_if(habits.begin(), habits.end(), [&](const HabitDefinition& item) { return item.id == normalized.id; });
  if (it == habits.end()) {
    if (habits.size() >= MAX_HABITS) return false;
    habits.push_back(std::move(normalized));
  } else {
    *it = std::move(normalized);
  }
  return saveToFile();
}

bool HabitSheepStore::removeHabit(const std::string& id) {
  const auto it =
      std::find_if(habits.begin(), habits.end(), [&](const HabitDefinition& habit) { return habit.id == id; });
  if (it == habits.end()) return false;

  habits.erase(it);
  for (auto& activeId : activeHabitIds) {
    if (activeId == id) activeId.clear();
  }
  return saveToFile();
}

bool HabitSheepStore::setActiveHabit(const size_t slot, const std::string& habitId) {
  if (slot >= activeHabitIds.size()) return false;
  if (!habitId.empty() && (!findHabit(habitId) || isActiveElsewhere(slot, habitId))) return false;
  activeHabitIds[slot] = habitId;
  return saveToFile();
}

bool HabitSheepStore::setEnabled(const bool value, const uint8_t sleepScreen) {
  if (value == enabled) return true;
  if (!value) {
    SHEEP_STATE.settleDay();
    if (!HABIT_TIMER.pauseAll()) return false;
  }
  const uint8_t previousSleep = pausedSleepScreen;
  if (!value) pausedSleepScreen = sleepScreen;
  enabled = value;
  if (!saveToFile()) {
    enabled = !value;
    pausedSleepScreen = previousSleep;
    return false;
  }
  ++modeRevision;
  if (!SHEEP_STATE.syncPause()) LOG_ERR("HABIT", "Sheep pause state will retry at next settlement");
  return true;
}

bool HabitSheepStore::setWeekStart(const uint8_t value) {
  if (value > 6) return false;
  if (value == weekStart) return true;
  const uint8_t previous = weekStart;
  weekStart = value;
  if (saveToFile()) return true;
  weekStart = previous;
  return false;
}

bool HabitSheepStore::clearPausedSleepScreen() {
  const uint8_t previous = pausedSleepScreen;
  pausedSleepScreen = 255;
  if (saveToFile()) return true;
  pausedSleepScreen = previous;
  return false;
}

bool HabitSheepStore::writeDefaults(const char* path) {
  static_assert(sizeof(HabitSheepStore) < 256);
  HabitSheepStore fresh;
  JsonDocument doc;
  fresh.toJson(doc);
  return writeDocToFile(path, doc);
}
