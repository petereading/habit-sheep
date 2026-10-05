#pragma once
#include <array>

#include "HabitEventLog.h"
#include "activities/Activity.h"

class HabitHistoryActivity final : public Activity {
 public:
  HabitHistoryActivity(GfxRenderer& r, MappedInputManager& input, const HabitDefinition& value)
      : Activity("HabitHistory", r, input), habit(value) {}
  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  HabitDefinition habit;
  std::array<HabitDailyProgress, 14> days{};
  std::array<uint32_t, 14> sessions{};
  std::array<char, 512> scratch{};
  tm today{};
  uint8_t page = 0;
  bool dated = false, readable = true;
};
