#pragma once

#include <array>
#include <string>

#include "activities/Activity.h"
#include "components/HabitClock.h"
#include "components/OptionPopup.h"

class HabitDurationActivity final : public Activity {
 public:
  HabitDurationActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, std::string habitId);

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  std::string habitId;
  int selection = 0;
  int lastRenderedMinute = -1;
  int lastRenderedPhase = -1;
  bool lastRenderedRunning = false;
  uint8_t lastSheepPose = 255;
  OptionPopup addMinutesPopup;
  HabitClock habitClock;

  struct ActionLabels {
    std::array<const char*, 6> items{};
    int count = 0;
  };

  ActionLabels actionLabels() const;
  void activate();
  void showAddMinutes();
  void showCustomMinutes();
  void confirmMinutes(uint16_t minutes);
  void continueReading();
};
