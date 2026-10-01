#pragma once

#include "activities/Activity.h"

class GrassHistoryActivity final : public Activity {
 public:
  GrassHistoryActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("GrassHistory", renderer, mappedInput) {}

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  uint8_t page = 0;
};
