#pragma once

#include <array>
#include <string>

#include "activities/UiListActivity.h"

class HabitSheepSettingsActivity final : public UiListActivity {
 public:
  HabitSheepSettingsActivity(GfxRenderer& renderer, MappedInputManager& mappedInput);

  void onEnter() override;

 private:
  static constexpr int ROW_COUNT = 5;
  std::array<std::string, ROW_COUNT> values;
  std::array<freeink::ui::ListItem, ROW_COUNT> rows{};

  int listCount() const override { return ROW_COUNT; }
  const char* headerTitle() const override { return "Habit Sheep"; }
  void buildScreen(UiScreen& screen) override;
  void activateIndex(int index) override;
  void refreshRows();
};
