#pragma once

#include <ArduinoJson.h>
#include <PersistableStore.h>

#include <array>
#include <cstdint>

class SheepStateStore : public PersistableStore<SheepStateStore> {
 public:
  static constexpr uint8_t GRASS_CAP = 14;
  struct GrassDay {
    uint32_t day = 0;  // YYYYMMDD in the device's local timezone.
    uint8_t earned = 0;
    uint8_t eaten = 0;
  };

  static const char* getFilePath() { return "/.crosspoint/habit_sheep_state.json"; }

  void toJson(JsonDocument& doc) const;
  bool fromJson(JsonVariantConst doc);

  uint32_t getBondPoints() const { return bondPoints; }
  uint8_t getGrassStock() const { return grassStock; }
  GrassDay grassForDay(uint32_t day) const;
  bool settleDay();

  void recordInteraction();
  uint8_t addGrass(uint8_t amount, const char* eventDay = nullptr);

 private:
  SheepStateStore() = default;
  ~SheepStateStore() = default;
  friend class PersistableStore<SheepStateStore>;

  uint32_t bondPoints = 0;
  uint8_t grassStock = 3;
  uint32_t lastFedDay = 0;
  std::array<GrassDay, 14> grassDays{};

  GrassDay& entryForDay(uint32_t day);
};

#define SHEEP_STATE SheepStateStore::getInstance()
