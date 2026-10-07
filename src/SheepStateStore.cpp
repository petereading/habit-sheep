#include "SheepStateStore.h"

#include <HalClock.h>
#include <Logging.h>

#include <algorithm>
#include <climits>
#include <cstdio>
#include <ctime>

#include "HabitReset.h"
#include "HabitSheepStore.h"

namespace {
uint32_t dayKey(const tm& local) {
  return static_cast<uint32_t>(local.tm_year + 1900) * 10000 + static_cast<uint32_t>(local.tm_mon + 1) * 100 +
         static_cast<uint32_t>(local.tm_mday);
}

bool currentDay(uint32_t& day) {
  tm local{};
  if (!halClock.isAvailable() || !halClock.localTime(local)) return false;
  day = dayKey(local);
  return true;
}

uint32_t parseDay(const char* value) {
  if (!value) return 0;
  unsigned year = 0, month = 0, day = 0;
  if (sscanf(value, "%4u-%2u-%2u", &year, &month, &day) != 3 || year < 2020 || month < 1 || month > 12 || day < 1 ||
      day > 31)
    return 0;
  return year * 10000 + month * 100 + day;
}

bool nextDay(uint32_t day, uint32_t& next) {
  tm local{};
  local.tm_year = static_cast<int>(day / 10000) - 1900;
  local.tm_mon = static_cast<int>(day / 100 % 100) - 1;
  local.tm_mday = static_cast<int>(day % 100) + 1;
  local.tm_hour = 12;  // Midday avoids daylight-saving transitions around midnight.
  local.tm_isdst = -1;
  if (mktime(&local) == static_cast<time_t>(-1)) return false;
  next = dayKey(local);
  return next > day;
}
}  // namespace

void SheepStateStore::toJson(JsonDocument& doc) const {
  doc["schema"] = 4;
  doc["mood"] = mood;
  doc["mealsProcessed"] = mealsProcessed;
  doc["eatenToday"] = eatenToday;
  doc["missedMeal"] = missedMeal;
  doc["paused"] = paused;
  doc["lastInteractionDay"] = lastInteractionDay;
  doc["bondPoints"] = bondPoints;
  doc["grassStock"] = grassStock;
  doc["lastFedDay"] = lastFedDay;
  JsonArray days = doc["grassDays"].to<JsonArray>();
  for (const auto& entry : grassDays) {
    if (!entry.day) continue;
    JsonObject row = days.add<JsonObject>();
    row["d"] = entry.day;
    row["g"] = entry.earned;
    row["e"] = entry.eaten;
    row["p"] = entry.paused;
  }
}

bool SheepStateStore::fromJson(JsonVariantConst doc) {
  bondPoints = doc["bondPoints"] | static_cast<uint32_t>(0);
  grassDays = {};
  mood = std::min<uint8_t>(5, doc["mood"] | static_cast<uint8_t>(5));
  mealsProcessed = std::min<uint8_t>(3, doc["mealsProcessed"] | static_cast<uint8_t>(0));
  eatenToday = std::min<uint8_t>(3, doc["eatenToday"] | static_cast<uint8_t>(0));
  missedMeal = doc["missedMeal"] | false;
  paused = doc["paused"] | !HABIT_SHEEP.isEnabled();
  lastInteractionDay = doc["lastInteractionDay"] | static_cast<uint32_t>(0);
  const uint8_t schema = doc["schema"] | static_cast<uint8_t>(1);
  if (schema < 3) {
    // Preserve legacy pasture with a three-day starting buffer.
    const uint32_t pasture = doc["pasturePoints"] | static_cast<uint32_t>(0);
    grassStock = static_cast<uint8_t>(std::min<uint32_t>(GRASS_CAP, std::max<uint32_t>(9, pasture / 20)));
    lastFedDay = 0;
    requestResave();
    return true;
  }
  grassStock = std::min<uint8_t>(doc["grassStock"] | static_cast<uint8_t>(9), GRASS_CAP);
  lastFedDay = schema >= 4 ? (doc["lastFedDay"] | static_cast<uint32_t>(0)) : 0;
  if (schema < 4) requestResave();
  for (JsonObjectConst row : doc["grassDays"].as<JsonArrayConst>()) {
    const uint32_t day = row["d"] | static_cast<uint32_t>(0);
    if (day < 20200101) continue;
    GrassDay& entry = entryForDay(day);
    entry.earned = row["g"] | static_cast<uint8_t>(0);
    entry.eaten = row["e"] | static_cast<uint8_t>(0);
    entry.paused = row["p"] | false;
  }
  return true;
}

SheepStateStore::GrassDay& SheepStateStore::entryForDay(const uint32_t day) {
  for (auto& entry : grassDays) {
    if (entry.day == day) return entry;
  }
  auto* oldest = &grassDays[0];
  for (auto& entry : grassDays) {
    if (entry.day < oldest->day) oldest = &entry;
  }
  *oldest = GrassDay{day, 0, 0};
  return *oldest;
}

SheepStateStore::GrassDay SheepStateStore::grassForDay(const uint32_t day) const {
  for (const auto& entry : grassDays) {
    if (entry.day == day) return entry;
  }
  return {};
}

bool SheepStateStore::syncPause() {
  if (habitResetPending()) return false;
  const auto previous = snapshot();
  tm local{};
  if (!halClock.isAvailable() || !halClock.localTime(local)) {
    paused = !HABIT_SHEEP.isEnabled();
    lastFedDay = 0;
    return persistOrRestore(previous);
  }
  const uint32_t today = dayKey(local);
  markPausedDays(local);
  const uint8_t due = (local.tm_hour >= 8) + (local.tm_hour >= 13) + (local.tm_hour >= 19);
  if (lastFedDay != today) {
    eatenToday = 0;
    mealsProcessed = due;
  } else {
    mealsProcessed = std::max(mealsProcessed, due);
  }
  lastFedDay = today;
  paused = !HABIT_SHEEP.isEnabled();
  if (paused) entryForDay(today).paused = true;
  missedMeal = false;
  return persistOrRestore(previous);
}

bool SheepStateStore::settleDay() {
  if (habitResetPending()) return false;
  tm local{};
  if (!halClock.isAvailable() || !halClock.localTime(local)) return false;
  const uint32_t today = dayKey(local);
  const uint8_t due = (local.tm_hour >= 8) + (local.tm_hour >= 13) + (local.tm_hour >= 19);
  if (paused != !HABIT_SHEEP.isEnabled() || !lastFedDay) {
    return syncPause();
  }
  // Ignore backward clock corrections rather than consuming the same meal twice.
  if (today < lastFedDay) {
    uint32_t tomorrow = 0;
    // Small timezone/clock adjustments keep the meal cursor; a badly set RTC rebases without charges.
    if (nextDay(today, tomorrow) && tomorrow < lastFedDay) return syncPause();
    return false;
  }
  if (paused) {
    if (today == lastFedDay) return false;
    return syncPause();
  }
  if (today == lastFedDay && due <= mealsProcessed) return false;
  const auto previous = snapshot();
  while (lastFedDay <= today) {
    const uint8_t limit = lastFedDay == today ? due : 3;
    while (mealsProcessed < limit) {
      ++mealsProcessed;
      if (grassStock) {
        --grassStock;
        ++eatenToday;
        ++entryForDay(lastFedDay).eaten;
        mood = std::min<uint8_t>(5, mood + 1);
        missedMeal = false;
      } else {
        missedMeal = true;
      }
    }
    if (lastFedDay == today) break;
    if (!eatenToday && mood && !grassForDay(lastFedDay).paused) {
      --mood;
      if (bondPoints) --bondPoints;
    }
    uint32_t next = 0;
    if (!nextDay(lastFedDay, next)) break;
    lastFedDay = next;
    mealsProcessed = 0;
    eatenToday = 0;
    // With no food and no hearts there is no further debt to settle.
    // At most seven fed days and five empty days need individual settlement.
    if (!mood && lastFedDay < today) lastFedDay = today;
  }
  return persistOrRestore(previous);
}

void SheepStateStore::recordInteraction() {
  if (habitResetPending()) return;
  if (!HABIT_SHEEP.isEnabled() || isForaging()) return;
  uint32_t today = 0;
  if (!currentDay(today) || today == lastInteractionDay) return;
  const auto previous = snapshot();
  lastInteractionDay = today;
  if (bondPoints < UINT32_MAX) ++bondPoints;
  persistOrRestore(previous);
}

uint8_t SheepStateStore::addGrass(const uint8_t amount, const char* eventDay) {
  if (habitResetPending()) return 0;
  if (!HABIT_SHEEP.isEnabled()) return 0;
  settleDay();
  const uint8_t earned = std::min<uint8_t>(amount, GRASS_CAP - grassStock);
  if (!earned) return 0;
  const auto previous = snapshot();
  grassStock += earned;
  uint32_t day = parseDay(eventDay);
  if (!day) currentDay(day);
  if (day) {
    // A delayed reading event can belong to an older date. Once the ledger is
    // full, do not evict a more recent day to display an obsolete event.
    uint32_t oldest = UINT32_MAX;
    for (const auto& entry : grassDays) oldest = std::min(oldest, entry.day);
    if (day >= oldest) {
      GrassDay& entry = entryForDay(day);
      entry.earned = static_cast<uint8_t>(std::min<int>(UINT8_MAX, entry.earned + earned));
    }
  }
  if (!mood) {
    // The returning sheep shares one of today's meal slots, never a fourth meal.
    mood = 1;
    if (mealsProcessed < 3) {
      ++mealsProcessed;
      --grassStock;
      ++eatenToday;
      if (currentDay(day)) ++entryForDay(day).eaten;
      mood = 2;
    }
    missedMeal = false;
  }
  return persistOrRestore(previous) ? earned : 0;
}

SheepStateStore::Snapshot SheepStateStore::snapshot() const {
  return {grassDays, bondPoints,     lastFedDay, lastInteractionDay, grassStock,
          mood,      mealsProcessed, eatenToday, missedMeal,         paused};
}

bool SheepStateStore::persistOrRestore(const Snapshot& previous) {
  if (saveToFile()) return true;
  grassDays = previous.days;
  bondPoints = previous.bond;
  lastFedDay = previous.fedDay;
  lastInteractionDay = previous.interactionDay;
  grassStock = previous.stock;
  mood = previous.mood;
  mealsProcessed = previous.meals;
  eatenToday = previous.eaten;
  missedMeal = previous.missed;
  paused = previous.paused;
  LOG_ERR("HABIT", "Cannot save sheep state");
  return false;
}

void SheepStateStore::markPausedDays(const tm& local) {
  const uint32_t today = dayKey(local);
  // Mark only the latest fourteen dates; a long holiday never causes an unbounded loop.
  if (paused && lastFedDay && lastFedDay <= today) {
    for (int offset = 0; offset < 14; ++offset) {
      tm day = local;
      day.tm_mday -= offset;
      day.tm_hour = 12;
      day.tm_isdst = -1;
      mktime(&day);
      const uint32_t key = dayKey(day);
      if (key < lastFedDay) break;
      entryForDay(key).paused = true;
    }
  }
}

bool SheepStateStore::writeDefaults(const char* path) {
  static_assert(sizeof(SheepStateStore) < 256);
  SheepStateStore fresh;
  JsonDocument doc;
  fresh.toJson(doc);
  return writeDocToFile(path, doc);
}
