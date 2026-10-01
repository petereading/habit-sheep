#include "SheepStateStore.h"

#include <HalClock.h>

#include <algorithm>
#include <climits>
#include <cstdio>
#include <ctime>

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
  doc["schema"] = 3;
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
  }
}

bool SheepStateStore::fromJson(JsonVariantConst doc) {
  bondPoints = doc["bondPoints"] | static_cast<uint32_t>(0);
  grassDays.fill(GrassDay{});
  const uint8_t schema = doc["schema"] | static_cast<uint8_t>(1);
  if (schema < 3) {
    // Preserve at least the legacy visible pasture, and give existing beta
    // users the same three-day starting buffer as a fresh installation.
    const uint32_t pasture = doc["pasturePoints"] | static_cast<uint32_t>(0);
    grassStock = static_cast<uint8_t>(std::min<uint32_t>(GRASS_CAP, std::max<uint32_t>(3, pasture / 20)));
    lastFedDay = 0;
    requestResave();
    return true;
  }
  grassStock = std::min<uint8_t>(doc["grassStock"] | static_cast<uint8_t>(3), GRASS_CAP);
  lastFedDay = doc["lastFedDay"] | static_cast<uint32_t>(0);
  for (JsonObjectConst row : doc["grassDays"].as<JsonArrayConst>()) {
    const uint32_t day = row["d"] | static_cast<uint32_t>(0);
    if (day < 20200101) continue;
    GrassDay& entry = entryForDay(day);
    entry.earned = row["g"] | static_cast<uint8_t>(0);
    entry.eaten = row["e"] | static_cast<uint8_t>(0);
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

bool SheepStateStore::settleDay() {
  uint32_t today = 0;
  if (!currentDay(today)) return false;
  if (lastFedDay == today) return false;
  if (lastFedDay == 0 || lastFedDay < 20200101 || lastFedDay > today) {
    // A clock or timezone correction must not make the sheep wait for a
    // previously recorded future date before it can eat again.
    lastFedDay = today;
    saveToFile();
    return true;
  }

  uint32_t day = lastFedDay;
  while (day < today && grassStock > 0) {
    uint32_t next = 0;
    if (!nextDay(day, next) || next > today) break;
    day = next;
    --grassStock;
    GrassDay& entry = entryForDay(day);
    if (entry.eaten < UINT8_MAX) ++entry.eaten;
  }
  // With no grass left, the sheep forages on its own. Never accrue a debt.
  lastFedDay = today;
  saveToFile();
  return true;
}

void SheepStateStore::recordInteraction() {
  if (bondPoints < UINT32_MAX) ++bondPoints;
  saveToFile();
}

uint8_t SheepStateStore::addGrass(const uint8_t amount, const char* eventDay) {
  settleDay();
  const uint8_t earned = std::min<uint8_t>(amount, GRASS_CAP - grassStock);
  if (!earned) return 0;
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
  saveToFile();
  return earned;
}
