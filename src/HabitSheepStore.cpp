#include "HabitSheepStore.h"

#include <Logging.h>

#include <algorithm>
#include <cstring>
#include <string_view>
#include <utility>

namespace {
constexpr uint8_t HABIT_SHEEP_SCHEMA_VERSION = 1;

const char* habitTypeName(const HabitType type) {
  return type == HabitType::Duration ? "duration" : "completion";
}

HabitType parseHabitType(const char* value) {
  return value && std::string_view(value) == "duration" ? HabitType::Duration : HabitType::Completion;
}
}  // namespace

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

  JsonArray habitArray = doc["habits"].to<JsonArray>();
  for (const auto& habit : habits) {
    JsonObject obj = habitArray.add<JsonObject>();
    obj["id"] = habit.id;
    obj["name"] = habit.name;
    obj["type"] = habitTypeName(habit.type);
    obj["targetMinutes"] = habit.targetMinutes;
    obj["readingIntegration"] = habit.readingIntegration;
  }

  JsonArray activeArray = doc["activeHabitIds"].to<JsonArray>();
  for (const auto& id : activeHabitIds) activeArray.add(id);
}

bool HabitSheepStore::fromJson(JsonVariantConst doc) {
  sheepName.clear();
  habits.clear();
  activeHabitIds.fill("");

  const char* storedSheepName = doc["sheepName"] | "";
  if (storedSheepName && strlen(storedSheepName) <= MAX_NAME_BYTES) sheepName = storedSheepName;

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
    habit.targetMinutes = obj["targetMinutes"] | static_cast<uint16_t>(0);
    habit.readingIntegration = obj["readingIntegration"] | false;
    if (habit.type == HabitType::Duration && habit.targetMinutes == 0) continue;
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

  LOG_DBG("HABIT", "Habit Sheep loaded (%u habits)", static_cast<unsigned>(habits.size()));
  return true;
}

const HabitDefinition* HabitSheepStore::findHabit(const std::string& id) const {
  const auto it = std::find_if(habits.begin(), habits.end(), [&](const HabitDefinition& habit) { return habit.id == id; });
  return it == habits.end() ? nullptr : &*it;
}

bool HabitSheepStore::setSheepName(const std::string& name) {
  if (!name.empty() && name.size() > MAX_NAME_BYTES) return false;
  sheepName = name;
  return saveToFile();
}

bool HabitSheepStore::upsertHabit(const HabitDefinition& habit) {
  if (!validId(habit.id) || !validName(habit.name)) return false;
  if (habit.type == HabitType::Duration && habit.targetMinutes == 0) return false;

  HabitDefinition normalized = habit;
  if (normalized.type == HabitType::Completion) {
    normalized.targetMinutes = 0;
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
  const auto it = std::find_if(habits.begin(), habits.end(), [&](const HabitDefinition& habit) { return habit.id == id; });
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
