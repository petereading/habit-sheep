#pragma once

#include <ArduinoJson.h>
#include <PersistableStore.h>

#include <array>
#include <cstdint>
#include <ctime>

class SheepStateStore : public PersistableStore<SheepStateStore> {
 public:
  static constexpr uint8_t GRASS_CAP = 21;
  struct GrassDay {
    uint32_t day = 0;  // YYYYMMDD in the device's local timezone.
    uint8_t earned = 0;
    uint8_t eaten = 0;
    bool paused = false;
  };

  static const char* getFilePath() { return "/.crosspoint/habit_sheep_state.json"; }

  static bool writeDefaults(const char* path);
  void toJson(JsonDocument& doc) const;
  bool fromJson(JsonVariantConst doc);

  uint32_t getBondPoints() const { return bondPoints; }
  uint8_t getGrassStock() const { return grassStock; }
  GrassDay grassForDay(uint32_t day) const;
  bool settleDay();
  uint8_t getMood() const { return mood; }
  bool isForaging() const { return mood == 0; }
  bool isResting() const { return missedMeal && !isForaging(); }
  bool ateCurrentMeal(const tm& local) const {
    const uint32_t day =
        static_cast<uint32_t>(local.tm_year + 1900) * 10000U + (local.tm_mon + 1) * 100U + local.tm_mday;
    const uint8_t slot = local.tm_hour == 8 ? 1 : local.tm_hour == 13 ? 2 : local.tm_hour == 19 ? 3 : 0;
    return slot && local.tm_min < 5 && day == lastFedDay && mealsProcessed >= slot && eatenToday && !missedMeal;
  }
  bool syncPause();

  void recordInteraction();
  uint8_t addGrass(uint8_t amount, const char* eventDay = nullptr);

 private:
  SheepStateStore() = default;
  ~SheepStateStore() = default;
  friend class PersistableStore<SheepStateStore>;

  uint32_t bondPoints = 0;
  uint8_t grassStock = 9;
  uint8_t mood = 5;
  uint8_t mealsProcessed = 0;
  uint8_t eatenToday = 0;
  bool missedMeal = false;
  bool paused = false;
  uint32_t lastInteractionDay = 0;
  uint32_t lastFedDay = 0;
  std::array<GrassDay, 14> grassDays{};

  struct Snapshot {
    std::array<GrassDay, 14> days;
    uint32_t bond;
    uint32_t fedDay;
    uint32_t interactionDay;
    uint8_t stock, mood, meals, eaten;
    bool missed, paused;
  };
  void markPausedDays(const tm& local);
  Snapshot snapshot() const;
  bool persistOrRestore(const Snapshot& previous);
  GrassDay& entryForDay(uint32_t day);
};

#define SHEEP_STATE SheepStateStore::getInstance()
