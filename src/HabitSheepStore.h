#pragma once

#include <ArduinoJson.h>
#include <PersistableStore.h>

#include <array>
#include <cstdint>
#include <string>
#include <vector>

enum class HabitType : uint8_t { Completion = 0, Duration = 1 };

struct HabitDefinition {
  std::string id;
  std::string name;
  HabitType type = HabitType::Completion;
  uint16_t targetMinutes = 0;
  bool readingIntegration = false;
};

class HabitSheepStore : public PersistableStore<HabitSheepStore> {
 public:
  static constexpr size_t MAX_HABITS = 9;
  static constexpr size_t MAX_ACTIVE_HABITS = 3;
  static constexpr size_t MAX_ID_BYTES = 64;
  static constexpr size_t MAX_NAME_BYTES = 96;

 private:
  std::string sheepName;
  std::vector<HabitDefinition> habits;
  std::array<std::string, MAX_ACTIVE_HABITS> activeHabitIds{};

  HabitSheepStore() = default;
  ~HabitSheepStore() = default;

  friend class PersistableStore<HabitSheepStore>;

  static bool validId(const std::string& id);
  static bool validName(const std::string& name);
  bool isActiveElsewhere(size_t slot, const std::string& habitId) const;

 public:
  static const char* getFilePath() { return "/.crosspoint/habit_sheep.json"; }

  void toJson(JsonDocument& doc) const;
  bool fromJson(JsonVariantConst doc);

  const std::string& getSheepName() const { return sheepName; }
  const std::vector<HabitDefinition>& getHabits() const { return habits; }
  const std::array<std::string, MAX_ACTIVE_HABITS>& getActiveHabitIds() const { return activeHabitIds; }

  const HabitDefinition* findHabit(const std::string& id) const;
  bool setSheepName(const std::string& name);
  bool upsertHabit(const HabitDefinition& habit);
  bool removeHabit(const std::string& id);
  bool setActiveHabit(size_t slot, const std::string& habitId);
};

#define HABIT_SHEEP HabitSheepStore::getInstance()
