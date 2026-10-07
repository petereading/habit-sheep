#pragma once
#include "SheepSudoku.h"
#include "activities/Activity.h"

class SheepSudokuActivity final : public Activity {
 public:
  SheepSudokuActivity(GfxRenderer& r, MappedInputManager& input) : Activity("SheepDoku", r, input) {}
  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  SheepSudoku game;
  uint8_t selection = 0, choice = 0;
  bool editing = false, suggested = false;
  bool selectable(int index) const;
  void move(int delta);
  void choose();
  void applyChoice();
};
