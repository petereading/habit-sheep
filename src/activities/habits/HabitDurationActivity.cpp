#include "HabitDurationActivity.h"

#include <GfxRenderer.h>
#include <HalDisplay.h>
#include <Memory.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <utility>

#include "HabitEventLog.h"
#include "HabitSheepStore.h"
#include "HabitTimer.h"
#include "I18n.h"
#include "RecentBooksStore.h"
#include "activities/ActivityManager.h"
#include "activities/util/IntervalSelectionActivity.h"
#include "activities/util/KeyboardEntryActivity.h"
#include "components/HabitReward.h"
#include "components/HabitUi.h"
#include "components/UITheme.h"
#include "fontIds.h"

namespace {
constexpr int SIDE_PAD = 24;

struct TimerLayout {
  int phaseY;
  int digitsY;
  int progressY;
  int barY;
  int actionsY;
  int rowHeight;
};

TimerLayout pomodoroLayout(const GfxRenderer& renderer, int count = 6) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const Rect safe = UITheme::getInstance().getScreenSafeArea(renderer, true, false);
  const int headerBottom = safe.y + metrics.topPadding + metrics.headerHeight;
  const bool compact = renderer.getScreenHeight() <= 600;
  const int phaseY = headerBottom + (compact ? 18 : 72);
  const int digitsY = phaseY + (compact ? 30 : 42);
  const int progressY = digitsY + (compact ? 65 : 110);
  const int barY = progressY + (compact ? 38 : 40);
  const int rowH = compact ? 34 : 48;
  return {phaseY, digitsY, progressY, barY, safe.y + safe.height - 18 - count * rowH, rowH};
}

TimerLayout durationLayout(const GfxRenderer& renderer, int count = 6) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const Rect safe = UITheme::getInstance().getScreenSafeArea(renderer, true, false);
  const int digitsY =
      safe.y + metrics.topPadding + metrics.headerHeight + (renderer.getScreenHeight() <= 600 ? 44 : 100);
  const int barY = digitsY + (renderer.getScreenHeight() <= 600 ? 106 : 154);
  const bool compact = renderer.getScreenHeight() <= 600;
  const int rowH = compact ? 34 : 48;
  return {0, digitsY, 0, barY, safe.y + safe.height - 18 - count * rowH, rowH};
}

}  // namespace

HabitDurationActivity::HabitDurationActivity(GfxRenderer& renderer, MappedInputManager& mappedInput,
                                             std::string habitIdValue)
    : Activity("HabitDuration", renderer, mappedInput), habitId(std::move(habitIdValue)) {}

void HabitDurationActivity::onEnter() {
  habitUi::applyOrientation(renderer);
  addMinutesPopup.setHabitStyle();
  Activity::onEnter();
  selection = 0;
  lastRenderedMinute = -1;
  requestUpdate();
}

void HabitDurationActivity::onExit() {
  const HabitDefinition* habit = HABIT_SHEEP.findHabit(habitId);
  if (HABIT_TIMER.isForHabit(habitId)) {
    if (habit && habit->type == HabitType::Pomodoro) {
      if (HABIT_TIMER.isRunningFor(habitId)) HABIT_TIMER.pause(habitId);
    } else {
      HABIT_TIMER.stopAndLog(habitId);
      if (HABIT_TIMER.isRunningFor(habitId)) HABIT_TIMER.pause(habitId);
    }
  }
  Activity::onExit();
}

HabitDurationActivity::ActionLabels HabitDurationActivity::actionLabels() const {
  ActionLabels labels;
  if (!HABIT_SHEEP.isEnabled()) {
    labels.items[labels.count++] = tr(STR_BACK);
    return labels;
  }
  const HabitDefinition* habit = HABIT_SHEEP.findHabit(habitId);
  const bool pomodoro = habit && habit->type == HabitType::Pomodoro;
  const auto phase = HABIT_TIMER.phaseFor(habitId);
  if (HABIT_TIMER.isForHabit(habitId)) {
    if (HABIT_TIMER.isRunningFor(habitId))
      labels.items[labels.count++] = pomodoro ? tr(STR_HABIT_PAUSE_SESSION) : tr(STR_HABIT_PAUSE_TIMER);
    else if (HABIT_TIMER.hasOtherRunning(habitId))
      labels.items[labels.count++] = tr(STR_HABIT_PAUSE_OTHER);
    else if (pomodoro && phase != HabitTimer::Phase::Focus && HABIT_TIMER.elapsedSecondsFor(habitId) == 0)
      labels.items[labels.count++] = tr(STR_HABIT_START_BREAK);
    else if (pomodoro && phase == HabitTimer::Phase::Focus && HABIT_TIMER.elapsedSecondsFor(habitId) == 0)
      labels.items[labels.count++] = tr(STR_HABIT_START_FOCUS);
    else
      labels.items[labels.count++] = tr(STR_HABIT_RESUME_TIMER);
    if (pomodoro && phase == HabitTimer::Phase::ShortBreak && !HABIT_TIMER.hasOtherRunning(habitId))
      labels.items[labels.count++] = tr(STR_HABIT_SKIP_SHORT_BREAK);
    labels.items[labels.count++] = pomodoro ? tr(STR_HABIT_END_SESSION) : tr(STR_HABIT_STOP_LOG);
  } else if (HABIT_TIMER.isRunning()) {
    labels.items[labels.count++] = tr(STR_HABIT_PAUSE_OTHER);
  } else {
    labels.items[labels.count++] = pomodoro ? tr(STR_HABIT_START_FOCUS) : tr(STR_HABIT_START_TIMER);
  }
  labels.items[labels.count++] = tr(STR_HABIT_ADD_MINUTES);

  if (habit && habit->readingIntegration) {
    labels.items[labels.count++] = tr(STR_HABIT_CONTINUE_READING);
    labels.items[labels.count++] = tr(STR_HABIT_BROWSE_FILES);
  }
  return labels;
}

void HabitDurationActivity::showAddMinutes() {
  addMinutesPopup.showMinuteChoices([this](const int selected) {
    static constexpr uint16_t MINUTES[] = {5, 10, 15, 20, 30, 45, 60};
    if (selected == 7) {
      showCustomMinutes();
      return;
    }
    if (selected < 0 || selected >= 7) return;
    confirmMinutes(MINUTES[selected]);
    requestUpdate();
  });
  requestUpdate();
}

void HabitDurationActivity::showCustomMinutes() {
  auto editor =
      makeUniqueNoThrow<IntervalSelectionActivity>(renderer, mappedInput, "HabitMinutes", StrId::STR_HABIT_ADD_MINUTES,
                                                   15, 1, 1440, 1, 5, StrId::STR_HABIT_MINUTE_VALUE);
  if (!editor) return;
  startActivityForResult(std::move(editor), [this](const ActivityResult& result) {
    RenderLock lock;
    if (result.isCancelled) return;
    confirmMinutes(static_cast<uint16_t>(std::get<IntervalResult>(result.data).value));
    requestUpdate();
  });
}

void HabitDurationActivity::confirmMinutes(uint16_t minutes) {
  auto progress = HABIT_EVENTS.progressForToday(habitId);
  if (HABIT_TIMER.isForHabit(habitId)) progress.durationSeconds += HABIT_TIMER.elapsedSecondsFor(habitId);
  char preview[80], add[32];
  snprintf(preview, sizeof(preview), tr(STR_HABIT_MINUTES_PREVIEW),
           static_cast<unsigned long>(progress.durationSeconds / 60),
           static_cast<unsigned long>(progress.durationSeconds / 60 + minutes));
  snprintf(add, sizeof(add), tr(STR_HABIT_ADD_SELECTED), minutes);
  const char* options[] = {tr(STR_CANCEL), add};
  addMinutesPopup.show(tr(STR_HABIT_ADD_MINUTES), preview, options, 2, 1, [this, minutes](int choice) {
    if (choice == 1)
      HABIT_EVENTS.appendDurationSeconds(habitId, static_cast<uint32_t>(minutes) * 60, HabitEventSource::Manual);
    requestUpdate();
  });
  requestUpdate();
}

void HabitDurationActivity::continueReading() {
  const auto& books = RECENT_BOOKS.getBooks();
  const auto book = std::find_if(books.begin(), books.end(),
                                 [](const RecentBook& item) { return !RecentBooksStore::isMissing(item); });
  if (book != books.end()) {
    activityManager.goToReader(book->path);
    return;
  }
  activityManager.goToFileBrowser("/");
}

void HabitDurationActivity::activate() {
  if (!HABIT_SHEEP.isEnabled()) {
    finish();
    return;
  }
  const auto labels = actionLabels();
  if (selection < 0 || selection >= labels.count) return;

  int index = 0;
  if (!HABIT_TIMER.isForHabit(habitId)) {
    if (selection == index++) {
      if (!HABIT_TIMER.hasOtherRunning(habitId)) HABIT_TIMER.start(habitId);
      requestUpdate();
      return;
    }
  } else {
    if (selection == index++) {
      if (HABIT_TIMER.isRunningFor(habitId))
        HABIT_TIMER.pause(habitId);
      else if (!HABIT_TIMER.hasOtherRunning(habitId))
        HABIT_TIMER.resume(habitId);
      requestUpdate();
      return;
    }
    const HabitDefinition* habit = HABIT_SHEEP.findHabit(habitId);
    if (habit && habit->type == HabitType::Pomodoro && HABIT_TIMER.phaseFor(habitId) == HabitTimer::Phase::ShortBreak &&
        !HABIT_TIMER.hasOtherRunning(habitId)) {
      if (selection == index++) {
        HABIT_TIMER.skipShortBreak(habitId);
        selection = 0;
        requestUpdate();
        return;
      }
    }
    if (selection == index++) {
      const char* options[] = {tr(STR_CANCEL), tr(STR_HABIT_STOP_LOG)};
      addMinutesPopup.show(tr(STR_HABIT_END_SESSION), options, 2, 0, [this](int choice) {
        if (choice == 1) HABIT_TIMER.stopAndLog(habitId);
        requestUpdate();
      });
      requestUpdate();
      return;
    }
  }

  if (selection == index++) {
    showAddMinutes();
    return;
  }

  const HabitDefinition* habit = HABIT_SHEEP.findHabit(habitId);
  if (habit && habit->readingIntegration) {
    if (selection == index++) {
      continueReading();
      return;
    }
    if (selection == index) {
      activityManager.goToFileBrowser("/");
      return;
    }
  }
}

void HabitDurationActivity::loop() {
  RenderLock lock;
  if (habitClock.changed()) requestUpdate();
  if (addMinutesPopup.isActive()) {
    addMinutesPopup.handleInput(mappedInput, [this] { requestUpdate(); });
    return;
  }

  if (HABIT_SHEEP.isEnabled() && showHabitReward(addMinutesPopup, &habitId)) {
    requestUpdate();
    return;
  }
  const auto labels = actionLabels();
  if (labels.count == 0) return;
  selection = std::clamp(selection, 0, labels.count - 1);

  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    finish();
    return;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::NavPrevious)) {
    selection = (selection - 1 + labels.count) % labels.count;
    requestUpdate();
    return;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::NavNext)) {
    selection = (selection + 1) % labels.count;
    requestUpdate();
    return;
  }

  int row = -1;
  const HabitDefinition* currentHabit = HABIT_SHEEP.findHabit(habitId);
  const bool pomodoro = currentHabit && currentHabit->type == HabitType::Pomodoro;
  const TimerLayout layout = pomodoro ? pomodoroLayout(renderer, labels.count) : durationLayout(renderer, labels.count);
  const Rect safe = UITheme::getInstance().getScreenSafeArea(renderer, true, false);
  const auto touch = mappedInput.rowTouch(row, layout.actionsY, layout.rowHeight, labels.count, safe.x + SIDE_PAD,
                                          safe.x + safe.width - SIDE_PAD, layout.rowHeight);
  if (touch == MappedInputManager::RowTouch::Tap) {
    selection = row;
    activate();
    return;
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    activate();
    return;
  }

  if (HABIT_TIMER.isForHabit(habitId)) {
    const HabitDefinition* habit = HABIT_SHEEP.findHabit(habitId);
    const auto phase = HABIT_TIMER.phaseFor(habitId);
    const bool running = HABIT_TIMER.isRunningFor(habitId);
    const uint32_t elapsed = HABIT_TIMER.elapsedSecondsFor(habitId);
    const uint32_t target =
        habit && habit->type == HabitType::Pomodoro
            ? static_cast<uint32_t>(phase == HabitTimer::Phase::Focus        ? habit->targetMinutes
                                    : phase == HabitTimer::Phase::ShortBreak ? habit->shortBreakMinutes
                                                                             : habit->longBreakMinutes) *
                  60
            : 0;
    const int minute = static_cast<int>(target > 0 && phase != HabitTimer::Phase::Focus
                                            ? (target > elapsed ? (target - elapsed + 59) / 60 : 0)
                                            : elapsed / 60);
    if (minute != lastRenderedMinute || static_cast<int>(phase) != lastRenderedPhase ||
        running != lastRenderedRunning) {
      lastRenderedMinute = minute;
      lastRenderedPhase = static_cast<int>(phase);
      lastRenderedRunning = running;
      requestUpdate();
    }
  }
}

void HabitDurationActivity::render(RenderLock&&) {
  renderer.clearScreen();
  const Rect safe = UITheme::getInstance().getScreenSafeArea(renderer, true, false);
  const int screenW = safe.width, center = safe.x + screenW / 2;
  const auto& metrics = UITheme::getInstance().getMetrics();
  const Rect header{safe.x, safe.y + metrics.topPadding, screenW, metrics.headerHeight};
  const HabitDefinition* habit = HABIT_SHEEP.findHabit(habitId);
  if (!habit) {
    GUI.drawHeader(renderer, header, "Habit");
    renderer.drawCenteredText(NOTOSANS_14_FONT_ID, 180, "Habit not found");
    renderer.displayBuffer();
    return;
  }

  GUI.drawHeader(renderer, header, habit->name.c_str(), nullptr, true, true);

  auto progress = HABIT_EVENTS.progressForToday(habitId);
  if (HABIT_TIMER.isForHabit(habitId) && HABIT_TIMER.phaseFor(habitId) == HabitTimer::Phase::Focus)
    progress.durationSeconds += HABIT_TIMER.elapsedSecondsFor(habitId);
  uint64_t periodSeconds = HABIT_EVENTS.durationSecondsForPeriod(*habit);
  if (HABIT_TIMER.isForHabit(habitId) && habit->type == HabitType::Duration)
    periodSeconds += HABIT_TIMER.elapsedSecondsFor(habitId);

  char progressText[40];
  const bool pomodoro = habit->type == HabitType::Pomodoro;
  const TimerLayout layout =
      pomodoro ? pomodoroLayout(renderer, actionLabels().count) : durationLayout(renderer, actionLabels().count);
  const int headerBottom = safe.y + metrics.topPadding + metrics.headerHeight;
  habitUi::icon(renderer, habitUi::iconFor(*habit), renderer.getScreenHeight() <= 600 ? safe.x + SIDE_PAD : center - 24,
                headerBottom + 6, 48);
  if (pomodoro) {
    snprintf(progressText, sizeof(progressText), tr(STR_HABIT_FOCUS_CYCLE),
             static_cast<unsigned>(HABIT_TIMER.focusesUntilLongBreak(habitId)),
             static_cast<unsigned>(progress.pomodoroSessions));
    const auto phase = HABIT_TIMER.phaseFor(habitId);
    const char* phaseLabel = phase == HabitTimer::Phase::Focus        ? tr(STR_HABIT_FOCUS)
                             : phase == HabitTimer::Phase::ShortBreak ? tr(STR_HABIT_SHORT_BREAK)
                                                                      : tr(STR_HABIT_LONG_BREAK);
    habitUi::centeredText(renderer, SMALL_FONT_ID, layout.phaseY, phaseLabel);
    const uint32_t elapsed = HABIT_TIMER.elapsedSecondsFor(habitId);
    const uint32_t target = static_cast<uint32_t>(phase == HabitTimer::Phase::Focus        ? habit->targetMinutes
                                                  : phase == HabitTimer::Phase::ShortBreak ? habit->shortBreakMinutes
                                                                                           : habit->longBreakMinutes) *
                            60;
    const uint32_t shown = phase == HabitTimer::Phase::Focus ? elapsed / 60
                           : target > elapsed                ? (target - elapsed + 59) / 60
                                                             : 0;
    char unit[24];
    snprintf(unit, sizeof(unit), "/ %u %s", static_cast<unsigned>(target / 60), tr(STR_HABIT_MINUTES_ABBR));
    habitUi::number(renderer, center, layout.digitsY, shown, unit, renderer.getScreenHeight() <= 600 ? 58 : 100);
    habitUi::centeredText(renderer, SMALL_FONT_ID, layout.progressY, progressText);
    habitUi::progress(renderer, safe.x + SIDE_PAD, layout.barY, screenW - SIDE_PAD * 2, elapsed, target);
  } else {
    const uint32_t sessions = static_cast<uint32_t>(periodSeconds / (habit->targetMinutes * 60UL));
    habitUi::centeredText(renderer, SMALL_FONT_ID, headerBottom + (renderer.getScreenHeight() <= 600 ? 18 : 64),
                          habit->period == HabitPeriod::Weekly ? tr(STR_HABIT_THIS_WEEK) : tr(STR_HABIT_TODAY));
    snprintf(progressText, sizeof(progressText), "/ %u %s", static_cast<unsigned>(habit->targetCount),
             tr(STR_HABIT_SESSIONS));
    habitUi::number(renderer, center, layout.digitsY, sessions, progressText,
                    renderer.getScreenHeight() <= 600 ? 58 : 100);
    char timeText[80];
    snprintf(timeText, sizeof(timeText), tr(STR_HABIT_SESSION_TIME), static_cast<unsigned long>(periodSeconds / 60),
             static_cast<unsigned>(habit->targetMinutes));
    habitUi::centeredText(renderer, SMALL_FONT_ID, layout.digitsY + (renderer.getScreenHeight() <= 600 ? 68 : 112),
                          timeText);
    habitUi::progress(renderer, safe.x + SIDE_PAD, layout.barY, screenW - SIDE_PAD * 2, sessions, habit->targetCount);
  }

  const int barX = safe.x + SIDE_PAD;
  const int barW = screenW - SIDE_PAD * 2;
  const int sheepTop = layout.barY + 22, sheepH = layout.actionsY - sheepTop - 8;
  if (sheepH > 55)
    habitUi::sheep(renderer, center - 85, sheepTop, 170, sheepH, HABIT_TIMER.isRunningFor(habitId) ? 0 : 12);

  const auto labels = actionLabels();
  for (int i = 0; i < labels.count; ++i) {
    const int y = layout.actionsY + i * layout.rowHeight;
    habitUi::frame(renderer, barX, y + 3, barW, layout.rowHeight - 6, i == selection);
    const auto shown = renderer.truncatedText(NOTOSANS_14_FONT_ID, labels.items[i], barW - 32);
    renderer.drawText(NOTOSANS_14_FONT_ID, barX + 16,
                      y + (layout.rowHeight - renderer.getLineHeight(NOTOSANS_14_FONT_ID)) / 2, shown.c_str());
  }

  if (addMinutesPopup.processRender(renderer, mappedInput)) return;

  const auto hints = mappedInput.mapLabels(tr(STR_BACK), tr(STR_SELECT), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, hints.btn1, hints.btn2, hints.btn3, hints.btn4);
  renderer.displayBuffer(HalDisplay::FAST_REFRESH);
}
