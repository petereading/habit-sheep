#include "HabitCountActivity.h"

#include <HalClock.h>
#include <HalDisplay.h>
#include <I18n.h>

#include <algorithm>
#include <cstdio>
#include <utility>

#include "HabitEventLog.h"
#include "components/HabitReward.h"
#include "components/HabitUi.h"
#include "components/UITheme.h"
#include "fontIds.h"

HabitCountActivity::HabitCountActivity(GfxRenderer& r, MappedInputManager& input, std::string id)
    : Activity("HabitCount", r, input), habitId(std::move(id)) {}
void HabitCountActivity::onEnter() {
  habitUi::applyOrientation(renderer);
  Activity::onEnter();
  popup.setHabitStyle();
}
void HabitCountActivity::loop() {
  RenderLock lock;
  if (clock.changed()) requestUpdate();
  if (popup.handleInput(mappedInput, [this] { requestUpdate(); })) return;
  if (showHabitReward(popup, &habitId)) {
    requestUpdate();
    return;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    finish();
    return;
  }
  if (!HABIT_SHEEP.isEnabled()) return;
  const Rect safe = GUI.getScreenSafeArea(renderer, true, false);
  const int h = safe.y + safe.height;
  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm) ||
      mappedInput.wasTapInRect(safe.x + 24, h - 100, safe.width - 48, 62)) {
    const auto* habit = HABIT_SHEEP.findHabit(habitId);
    if (!habit) return;
    const char* options[] = {tr(STR_CANCEL), tr(STR_HABIT_LOG_ONE)};
    popup.show(tr(STR_HABIT_CONFIRM_DONE), habit->name.c_str(), options, 2, 0, [this](int selected) {
      if (selected == 1) HABIT_EVENTS.appendCompletion(habitId);
    });
    requestUpdate();
  }
}
void HabitCountActivity::render(RenderLock&&) {
  renderer.clearScreen();
  const auto* habit = HABIT_SHEEP.findHabit(habitId);
  if (!habit) return;
  const Rect safe = GUI.getScreenSafeArea(renderer, true, false);
  const int w = safe.width, h = safe.y + safe.height, center = safe.x + w / 2;
  const bool compact = renderer.getScreenHeight() <= 600;
  GUI.drawHeader(renderer,
                 Rect{safe.x, safe.y + UITheme::getInstance().getMetrics().topPadding, w,
                      UITheme::getInstance().getMetrics().headerHeight},
                 habit->name.c_str(), nullptr, true, true);
  const int top =
      safe.y + UITheme::getInstance().getMetrics().topPadding + UITheme::getInstance().getMetrics().headerHeight + 12;
  habitUi::icon(renderer, habitUi::iconFor(*habit), center - 24, top, 48);
  const bool weekly = habit->period == HabitPeriod::Weekly;
  habitUi::centeredText(renderer, SMALL_FONT_ID, top + 56, weekly ? tr(STR_HABIT_THIS_WEEK) : tr(STR_HABIT_TODAY));
  if (weekly) {
    tm local{};
    if (halClock.localTime(local)) {
      local.tm_mday -= (local.tm_wday - HABIT_SHEEP.getWeekStart() + 7) % 7;
      local.tm_hour = 12;
      local.tm_isdst = -1;
      mktime(&local);
      char first[16], last[16], range[40];
      strftime(first, sizeof(first), "%d %b", &local);
      local.tm_mday += 6;
      local.tm_isdst = -1;
      mktime(&local);
      strftime(last, sizeof(last), "%d %b", &local);
      snprintf(range, sizeof(range), "%s - %s", first, last);
      habitUi::centeredText(renderer, SMALL_FONT_ID, top + 82, range);
    }
  }
  const uint16_t count =
      weekly ? HABIT_EVENTS.completionCountForWeek(habitId) : HABIT_EVENTS.progressForToday(habitId).completionCount;
  char unit[24];
  snprintf(unit, sizeof(unit), "/ %u %s", habit->targetCount, tr(STR_HABIT_TIMES));
  const int digits = top + (weekly ? 112 : 94), size = compact ? 70 : 112;
  habitUi::number(renderer, center, digits, count, unit, size);
  habitUi::progress(renderer, safe.x + 24, digits + size + 20, w - 48, count, habit->targetCount);
  const int sheepTop = digits + size + 42, sheepH = std::max(0, h - 122 - sheepTop);
  if (sheepH >= 60) habitUi::sheep(renderer, center - 80, sheepTop, 160, sheepH, 2);
  habitUi::frame(renderer, safe.x + 24, h - 100, w - 48, 62);
  habitUi::centeredText(renderer, NOTOSANS_14_FONT_ID, h - 85, tr(STR_HABIT_LOG_ONE));
  if (popup.processRender(renderer, mappedInput)) return;
  const auto hints = mappedInput.mapLabels(tr(STR_BACK), tr(STR_SELECT), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, hints.btn1, hints.btn2, hints.btn3, hints.btn4);
  renderer.displayBuffer(HalDisplay::FAST_REFRESH);
}
