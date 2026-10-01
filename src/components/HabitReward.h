#pragma once

#include <cstdio>

#include "HabitEventLog.h"
#include "I18n.h"
#include "SheepStateStore.h"
#include "components/OptionPopup.h"

inline bool showHabitReward(OptionPopup& popup, const std::string* habitId = nullptr) {
  HabitEventLog::RewardNotice notice;
  if (!HABIT_EVENTS.takeReward(notice, habitId)) return false;
  const HabitDefinition* habit = HABIT_SHEEP.findHabit(notice.habitId);
  char message[80];
  if (notice.grass)
    snprintf(message, sizeof(message), tr(STR_HABIT_REWARD_GAIN), static_cast<unsigned>(notice.grass),
             static_cast<unsigned>(notice.stock), static_cast<unsigned>(SheepStateStore::GRASS_CAP));
  else
    snprintf(message, sizeof(message), "%s", tr(STR_HABIT_REWARD_FULL));
  popup.showGrassReward(habit ? habit->name.c_str() : tr(STR_HABIT_REWARD_TITLE), message, notice.grass, notice.stock);
  return true;
}
