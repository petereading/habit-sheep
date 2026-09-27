#pragma once

#include <ArduinoJson.h>
#include <PersistableStore.h>

#include <cstdint>

class SheepStateStore : public PersistableStore<SheepStateStore> {
 public:
  static const char* getFilePath() { return "/.crosspoint/habit_sheep_state.json"; }

  void toJson(JsonDocument& doc) const;
  bool fromJson(JsonVariantConst doc);

  uint32_t getBondPoints() const { return bondPoints; }
  uint32_t getPasturePoints() const { return pasturePoints; }

  void recordInteraction();
  void recordCompletion();
  void recordDuration(uint32_t seconds);

 private:
  SheepStateStore() = default;
  ~SheepStateStore() = default;
  friend class PersistableStore<SheepStateStore>;

  uint32_t bondPoints = 0;
  uint32_t pasturePoints = 0;
};

#define SHEEP_STATE SheepStateStore::getInstance()
