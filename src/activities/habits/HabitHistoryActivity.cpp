#include "HabitHistoryActivity.h"

#include <HalClock.h>
#include <HalDisplay.h>

#include <cstdio>

#include "HabitHistoryMath.h"
#include "I18n.h"
#include "components/HabitUi.h"
#include "components/UITheme.h"
#include "fontIds.h"

void HabitHistoryActivity::onEnter() {
  habitUi::applyOrientation(renderer);
  dated = halClock.isAvailable() && halClock.localTime(today);
  if (!dated) {
    Activity::onEnter();
    return;
  }
  uint32_t carried = 0;
  for (int i = 19; i >= 0; --i) {
    tm day = today;
    day.tm_mday -= i;
    day.tm_hour = 12;
    day.tm_isdst = -1;
    mktime(&day);
    char key[16];
    strftime(key, sizeof(key), "%Y-%m-%d", &day);
    HabitDailyProgress progress;
    if (!HABIT_EVENTS.progressOnDay(habit.id, key, progress, scratch.data(), scratch.size())) readable = false;
    const uint32_t count =
        historySessions(progress.durationSeconds, std::max<uint16_t>(1, habit.targetMinutes) * 60U,
                        habit.period == HabitPeriod::Weekly, day.tm_wday == HABIT_SHEEP.getWeekStart(), carried);
    if (i < 14) {
      days[i] = progress;
      sessions[i] = count;
    }
  }
  Activity::onEnter();
}
void HabitHistoryActivity::loop() {
  RenderLock lock;
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    finish();
    return;
  }
  const Rect safe = UITheme::getInstance().getScreenSafeArea(renderer, true, false);
  if (mappedInput.wasReleased(MappedInputManager::Button::NavPrevious) ||
      mappedInput.wasTapInRect(safe.x + 24, safe.y + safe.height - 48, 140, 40)) {
    page = 0;
    requestUpdate();
  } else if (mappedInput.wasReleased(MappedInputManager::Button::NavNext) ||
             mappedInput.wasTapInRect(safe.x + safe.width - 164, safe.y + safe.height - 48, 140, 40)) {
    page = 1;
    requestUpdate();
  }
}
void HabitHistoryActivity::render(RenderLock&&) {
  renderer.clearScreen();
  const Rect safe = UITheme::getInstance().getScreenSafeArea(renderer, true, false);
  const auto title = renderer.truncatedText(UI_12_FONT_ID, habit.name.c_str(), safe.width - 48);
  habitUi::centeredText(renderer, UI_12_FONT_ID, safe.y + 12, title.c_str());
  habitUi::centeredText(
      renderer, SMALL_FONT_ID, safe.y + 48,
      habit.type == HabitType::Duration ? tr(STR_HABIT_HISTORY_INTERVAL) : tr(STR_HABIT_HISTORY_HINT));
  uint32_t total = 0;
  for (size_t i = 0; i < days.size(); ++i) {
    const uint32_t count = habit.type == HabitType::Completion ? days[i].completionCount
                           : habit.type == HabitType::Pomodoro ? days[i].pomodoroSessions
                                                               : sessions[i];
    total = count > UINT32_MAX - total ? UINT32_MAX : total + count;
  }
  char text[72];
  snprintf(text, sizeof(text),
           habit.type == HabitType::Completion ? tr(STR_HABIT_HISTORY_TOTAL) : tr(STR_HABIT_HISTORY_SESSIONS),
           static_cast<unsigned long>(total));
  habitUi::centeredText(renderer, SMALL_FONT_ID, safe.y + 76, text);
  if (!dated)
    habitUi::centeredText(renderer, SMALL_FONT_ID, safe.y + 108, tr(STR_GRASS_CLOCK_UNAVAILABLE));
  else {
    const int rowH = std::min(64, (safe.height - 158) / 7);
    for (int i = 0; i < 7; ++i) {
      const int index = page * 7 + i;
      tm day = today;
      day.tm_mday -= index;
      day.tm_hour = 12;
      day.tm_isdst = -1;
      mktime(&day);
      char date[20];
      strftime(date, sizeof(date), "%a %d %b", &day);
      const int y = safe.y + 108 + i * rowH;
      renderer.drawText(SMALL_FONT_ID, safe.x + 24, y, date);
      const auto& p = days[index];
      if (habit.type == HabitType::Completion)
        snprintf(text, sizeof(text), tr(STR_HABIT_HISTORY_COUNT), static_cast<unsigned>(p.completionCount));
      else {
        const uint32_t count = habit.type == HabitType::Pomodoro ? p.pomodoroSessions : sessions[index];
        snprintf(text, sizeof(text), tr(STR_HABIT_HISTORY_TIME), static_cast<unsigned long>(p.durationSeconds / 60),
                 static_cast<unsigned long>(count));
      }
      renderer.drawText(SMALL_FONT_ID, safe.x + safe.width - 24 - renderer.getTextWidth(SMALL_FONT_ID, text), y, text);
    }
  }
  if (!readable)
    habitUi::centeredText(renderer, SMALL_FONT_ID, safe.y + safe.height - 76, tr(STR_HABIT_HISTORY_FAILED));
  renderer.drawText(SMALL_FONT_ID, safe.x + 24, safe.y + safe.height - 42, tr(STR_GRASS_NEWER));
  renderer.drawText(SMALL_FONT_ID, safe.x + safe.width - 164, safe.y + safe.height - 42, tr(STR_GRASS_OLDER));
  const auto hints = mappedInput.mapLabels(tr(STR_BACK), "", tr(STR_GRASS_NEWER), tr(STR_GRASS_OLDER));
  GUI.drawButtonHints(renderer, hints.btn1, hints.btn2, hints.btn3, hints.btn4);
  renderer.displayBuffer(HalDisplay::FAST_REFRESH);
}
