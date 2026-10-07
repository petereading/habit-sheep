#include "HabitSheepMode.h"

#include "CrossPointSettings.h"
#include "HabitSheepStore.h"

bool setHabitSheepEnabled(const bool enabled) {
  if (enabled == HABIT_SHEEP.isEnabled()) return true;
  const uint8_t previous = SETTINGS.sleepScreen;
  const uint8_t restore = HABIT_SHEEP.getPausedSleepScreen();
  if (!HABIT_SHEEP.setEnabled(enabled, previous)) return false;
  if (!enabled) {
    SETTINGS.sleepScreen = CrossPointSettings::COVER;
  } else if (previous == CrossPointSettings::COVER && restore < CrossPointSettings::SLEEP_SCREEN_MODE_COUNT) {
    SETTINGS.sleepScreen = restore;
  }
  if (SETTINGS.saveToFile()) return true;
  SETTINGS.sleepScreen = previous;
  HABIT_SHEEP.setEnabled(!enabled, restore);
  return false;
}
