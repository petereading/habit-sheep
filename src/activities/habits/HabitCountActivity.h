#pragma once

#include <string>

#include "activities/Activity.h"
#include "components/HabitClock.h"
#include "components/OptionPopup.h"

class HabitCountActivity final : public Activity {
 public:
  HabitCountActivity(GfxRenderer& renderer, MappedInputManager& input, std::string id);
  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  std::string habitId;
  OptionPopup popup;
  HabitClock clock;
};
