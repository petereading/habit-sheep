#pragma once

#include <string>
#include <vector>

#include "HabitSheepStore.h"
#include "activities/UiListActivity.h"
#include "components/OptionPopup.h"

class HabitLibraryActivity final : public UiListActivity {
 public:
  HabitLibraryActivity(GfxRenderer& renderer, MappedInputManager& mappedInput);

  void onEnter() override;
  void render(RenderLock&& lock) override;
  void loop() override;

 private:
  std::vector<std::string> labels;
  std::vector<std::string> subtitles;
  std::vector<freeink::ui::ListItem> rows;
  OptionPopup popup;

  std::string pendingName;
  HabitType pendingType = HabitType::Completion;
  uint16_t pendingTargetMinutes = 0;
  HabitPeriod pendingPeriod = HabitPeriod::Daily;
  uint8_t pendingTargetCount = 1;

  int listCount() const override { return static_cast<int>(rows.size()); }
  const char* headerTitle() const override { return "Habit library"; }
  void buildScreen(UiScreen& screen) override;
  void activateIndex(int index) override;
  bool handleCustomInput() override;

  void rebuildRows();
  void startAddHabit();
  void chooseNewHabitType();
  void chooseDurationTarget();
  void chooseCompletionPeriod();
  void chooseCompletionTarget();
  void chooseReadingIntegration();
  void savePendingHabit(bool readingIntegration);
  void showEditMenu(const std::string& habitId);
  void renameHabit(const std::string& habitId);
  void changeTarget(const std::string& habitId);
  void changeCompletionPeriod(const std::string& habitId);
  void changeCompletionTarget(const std::string& habitId);
  void changePomodoroBreak(const std::string& habitId, bool longBreak);
  void changePomodoroSessions(const std::string& habitId);
  void toggleReadingIntegration(const std::string& habitId);
  void confirmDelete(const std::string& habitId);
  void changeIcon(const std::string& habitId);
};
