#pragma once
#include "SheepPuzzle.h"
#include "activities/Activity.h"

class SheepPuzzleActivity final : public Activity {
 public:
  SheepPuzzleActivity(GfxRenderer& r, MappedInputManager& input, SheepPuzzle::Mode value)
      : Activity("SheepPuzzle", r, input), mode(value) {}
  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  SheepPuzzle puzzle;
  SheepPuzzle::Mode mode;
  uint8_t selection = 0;
  uint8_t steps = 0;
  uint8_t direction = 1;
  int optionCount() const;
  bool selectable(int index) const;
  void moveSelection(int delta);
  void choose();
};
