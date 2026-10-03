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
  const int h = renderer.getScreenHeight();
  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm) ||
      mappedInput.wasTapInRect(24, h - 140, renderer.getScreenWidth() - 48, 62)) {
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
  const int w = renderer.getScreenWidth(), h = renderer.getScreenHeight();
  const bool compact = h <= 600;
  GUI.drawHeader(
      renderer,
      Rect{0, UITheme::getInstance().getMetrics().topPadding, w, UITheme::getInstance().getMetrics().headerHeight},
      habit->name.c_str(), nullptr, true, true);
  const int top =
      UITheme::getInstance().getMetrics().topPadding + UITheme::getInstance().getMetrics().headerHeight + 12;
  habitUi::icon(renderer, habitUi::iconFor(*habit), w / 2 - 24, top, 48);
  const bool weekly = habit->period == HabitPeriod::Weekly;
  renderer.drawCenteredText(SMALL_FONT_ID, top + 56, weekly ? tr(STR_HABIT_THIS_WEEK) : tr(STR_HABIT_TODAY));
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
      renderer.drawCenteredText(SMALL_FONT_ID, top + 82, range);
    }
  }
  const uint16_t count =
      weekly ? HABIT_EVENTS.completionCountForWeek(habitId) : HABIT_EVENTS.progressForToday(habitId).completionCount;
  char unit[24];
  snprintf(unit, sizeof(unit), "/ %u %s", habit->targetCount, tr(STR_HABIT_TIMES));
  const int digits = top + (weekly ? 112 : 94), size = compact ? 70 : 112;
  habitUi::number(renderer, w / 2, digits, count, unit, size);
  habitUi::progress(renderer, 24, digits + size + 20, w - 48, count, habit->targetCount);
  const int sheepTop = digits + size + 42, sheepH = std::max(0, h - 162 - sheepTop);
  if (sheepH >= 60) habitUi::sheep(renderer, w / 2 - 80, sheepTop, 160, sheepH, 2);
  habitUi::frame(renderer, 24, h - 140, w - 48, 62);
  renderer.drawCenteredText(NOTOSANS_14_FONT_ID, h - 125, tr(STR_HABIT_LOG_ONE));
  if (popup.processRender(renderer, mappedInput)) return;
  const auto hints = mappedInput.mapLabels(tr(STR_BACK), tr(STR_SELECT), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, hints.btn1, hints.btn2, hints.btn3, hints.btn4);
  renderer.displayBuffer(HalDisplay::FAST_REFRESH);
}
