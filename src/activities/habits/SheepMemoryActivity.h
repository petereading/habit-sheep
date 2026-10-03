#pragma once

#include "SheepMemoryGame.h"
#include "activities/Activity.h"

class SheepMemoryActivity final : public Activity {
 public:
  SheepMemoryActivity(GfxRenderer& renderer, MappedInputManager& input) : Activity("SheepMemory", renderer, input) {}
  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  SheepMemoryGame game;
  uint8_t selection = 0;
  void select();
};
