#pragma once

#include <ArduinoJson.h>
#include <PersistableStore.h>

#include <array>
#include <cstdint>
#include <string>
#include <vector>

enum class HabitType : uint8_t { Completion = 0, Duration = 1, Pomodoro = 2 };
enum class HabitPeriod : uint8_t { Daily = 0, Weekly = 1 };

struct HabitDefinition {
  std::string id;
  std::string name;
  HabitType type = HabitType::Completion;
  uint16_t targetMinutes = 0;
  bool readingIntegration = false;
  uint16_t shortBreakMinutes = 5;
  uint16_t longBreakMinutes = 15;
  uint8_t sessionsPerCycle = 4;
  HabitPeriod period = HabitPeriod::Daily;
  uint8_t targetCount = 1;
  uint8_t icon = 255;  // Unset legacy icons derive from the habit type.
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
  bool sleepSceneEnabled = true;
  bool enabled = true;
  uint32_t modeRevision = 0;
  uint8_t weekStart = 1;
  uint8_t orientation = 0;
  uint8_t homeFocus = 0;  // Habit, sheep, continue reading (physical-button Home only).
  uint8_t pausedSleepScreen = 255;

  HabitSheepStore();
  ~HabitSheepStore() = default;

  friend class PersistableStore<HabitSheepStore>;

  static bool validId(const std::string& id);
  static bool validName(const std::string& name);
  bool isActiveElsewhere(size_t slot, const std::string& habitId) const;
  void seedDefaultHabits();

 public:
  static const char* getFilePath() { return "/.crosspoint/habit_sheep.json"; }

  static bool writeDefaults(const char* path);
  void toJson(JsonDocument& doc) const;
  bool fromJson(JsonVariantConst doc);

  const std::string& getSheepName() const { return sheepName; }
  const std::vector<HabitDefinition>& getHabits() const { return habits; }
  const std::array<std::string, MAX_ACTIVE_HABITS>& getActiveHabitIds() const { return activeHabitIds; }
  uint32_t getModeRevision() const { return modeRevision; }
  void notifyReset() { ++modeRevision; }
  void blockForResetRecovery() {
    enabled = false;
    ++modeRevision;
  }
  bool clearPausedSleepScreen();
  bool isEnabled() const { return enabled; }
  uint8_t getWeekStart() const { return weekStart; }
  uint8_t getOrientation() const { return orientation; }
  bool setOrientation(uint8_t value);
  uint8_t getHomeFocus() const { return homeFocus; }
  bool setHomeFocus(uint8_t value);
  int homeSelection(bool hasBook) const;
  uint8_t getPausedSleepScreen() const { return pausedSleepScreen; }
  bool setEnabled(bool value, uint8_t sleepScreen = 255);
  bool setWeekStart(uint8_t value);
  bool legacySleepSceneEnabled() const { return sleepSceneEnabled; }

  const HabitDefinition* findHabit(const std::string& id) const;
  bool setSheepName(const std::string& name);
  bool upsertHabit(const HabitDefinition& habit);
  bool removeHabit(const std::string& id);
  bool setActiveHabit(size_t slot, const std::string& habitId);
};

#define HABIT_SHEEP HabitSheepStore::getInstance()
