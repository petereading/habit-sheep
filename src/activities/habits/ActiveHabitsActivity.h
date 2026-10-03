#pragma once

#include <array>
#include <string>
#include <vector>

#include "activities/UiListActivity.h"
#include "components/OptionPopup.h"

class ActiveHabitsActivity final : public UiListActivity {
 public:
  ActiveHabitsActivity(GfxRenderer& renderer, MappedInputManager& mappedInput);

  void onEnter() override;
  void loop() override;
  void render(RenderLock&& lock) override;

 private:
  std::array<std::string, 3> values;
  std::array<freeink::ui::ListItem, 3> rows{};
  OptionPopup picker;
  std::vector<std::string> pickerIds;

  int listCount() const override { return 3; }
  const char* headerTitle() const override { return "Active habits"; }
  void buildScreen(UiScreen& screen) override;
  void activateIndex(int index) override;
  bool handleCustomInput() override;
  void refreshRows();
  void showPicker(int slot);
};
