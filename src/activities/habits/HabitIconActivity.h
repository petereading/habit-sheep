#pragma once

#include "activities/Activity.h"

class HabitIconActivity final : public Activity {
 public:
  HabitIconActivity(GfxRenderer& renderer, MappedInputManager& input, uint8_t current)
      : Activity("HabitIcon", renderer, input), selected(current < 24 ? current : 15) {}
  void loop() override;
  void render(RenderLock&&) override;

 private:
  uint8_t selected;
};
