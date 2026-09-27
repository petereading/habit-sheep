#pragma once

#include <string>
#include <vector>

#include "activities/Activity.h"
#include "components/OptionPopup.h"

class HabitDurationActivity final : public Activity {
 public:
  HabitDurationActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, std::string habitId);

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  std::string habitId;
  int selection = 0;
  int lastRenderedMinute = -1;
  OptionPopup addMinutesPopup;

  std::vector<std::string> actionLabels() const;
  void activate();
  void showAddMinutes();
  void continueReading();
};
