#include "HabitDurationActivity.h"

#include <GfxRenderer.h>
#include <HalDisplay.h>
#include <Memory.h>

#include <algorithm>
#include <cstdio>
#include <utility>

#include "HabitEventLog.h"
#include "HabitSheepStore.h"
#include "HabitTimer.h"
#include "I18n.h"
#include "RecentBooksStore.h"
#include "activities/ActivityManager.h"
#include "components/UITheme.h"
#include "fontIds.h"

namespace {
constexpr int SIDE_PAD = 24;
constexpr int CONTENT_TOP = 190;
constexpr int ROW_H = 58;
}  // namespace

HabitDurationActivity::HabitDurationActivity(GfxRenderer& renderer, MappedInputManager& mappedInput,
                                             std::string habitIdValue)
    : Activity("HabitDuration", renderer, mappedInput), habitId(std::move(habitIdValue)) {}

void HabitDurationActivity::onEnter() {
  Activity::onEnter();
  selection = 0;
  lastRenderedMinute = -1;
  requestUpdate();
}

HabitDurationActivity::ActionLabels HabitDurationActivity::actionLabels() const {
  ActionLabels labels;
  if (HABIT_TIMER.isForHabit(habitId)) {
    if (HABIT_TIMER.isRunningFor(habitId))
      labels.items[labels.count++] = "Pause timer";
    else if (HABIT_TIMER.hasOtherRunning(habitId))
      labels.items[labels.count++] = tr(STR_HABIT_PAUSE_OTHER);
    else
      labels.items[labels.count++] = "Resume timer";
    labels.items[labels.count++] = "Stop & log";
  } else if (HABIT_TIMER.isRunning()) {
    labels.items[labels.count++] = tr(STR_HABIT_PAUSE_OTHER);
  } else {
    labels.items[labels.count++] = "Start timer";
  }
  labels.items[labels.count++] = "+ Add minutes";

  const HabitDefinition* habit = HABIT_SHEEP.findHabit(habitId);
  if (habit && habit->readingIntegration) {
    labels.items[labels.count++] = "Continue reading";
    labels.items[labels.count++] = "Browse files";
  }
  return labels;
}

void HabitDurationActivity::showAddMinutes() {
  static const char* OPTIONS[] = {"+5 minutes",  "+10 minutes", "+15 minutes", "+20 minutes",
                                  "+30 minutes", "+45 minutes", "+60 minutes"};
  addMinutesPopup.show("Add time", OPTIONS, 7, 2, [this](const int selected) {
    static constexpr uint16_t MINUTES[] = {5, 10, 15, 20, 30, 45, 60};
    if (selected < 0 || selected >= 7) return;
    HABIT_EVENTS.appendDurationSeconds(habitId, static_cast<uint32_t>(MINUTES[selected]) * 60,
                                       HabitEventSource::Manual);
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
    if (selection == index++) {
      HABIT_TIMER.stopAndLog(habitId);
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
  if (addMinutesPopup.isActive()) {
    addMinutesPopup.handleInput(mappedInput, [this] { requestUpdate(); });
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
  const auto touch = mappedInput.rowTouch(row, CONTENT_TOP, ROW_H, labels.count, SIDE_PAD,
                                          renderer.getScreenWidth() - SIDE_PAD, ROW_H);
  if (touch == MappedInputManager::RowTouch::Tap) {
    selection = row;
    activate();
    return;
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    activate();
    return;
  }

  if (HABIT_TIMER.isRunningFor(habitId)) {
    const int minute = static_cast<int>(HABIT_TIMER.elapsedSecondsFor(habitId) / 60);
    if (minute != lastRenderedMinute) {
      lastRenderedMinute = minute;
      requestUpdate();
    }
  }
}

void HabitDurationActivity::render(RenderLock&&) {
  renderer.clearScreen();
  const int screenW = renderer.getScreenWidth();
  const auto& metrics = UITheme::getInstance().getMetrics();
  const Rect header{0, metrics.topPadding, screenW, metrics.headerHeight};
  const HabitDefinition* habit = HABIT_SHEEP.findHabit(habitId);
  if (!habit) {
    GUI.drawHeader(renderer, header, "Habit");
    renderer.drawCenteredText(NOTOSANS_14_FONT_ID, 180, "Habit not found");
    renderer.displayBuffer();
    return;
  }

  GUI.drawHeader(renderer, header, habit->name.c_str());

  auto progress = HABIT_EVENTS.progressForToday(habitId);
  if (HABIT_TIMER.isForHabit(habitId) && HABIT_TIMER.phaseFor(habitId) == HabitTimer::Phase::Focus)
    progress.durationSeconds += HABIT_TIMER.elapsedSecondsFor(habitId);
  const uint32_t minutes = progress.durationSeconds / 60;

  char progressText[40];
  if (habit->type == HabitType::Pomodoro)
    snprintf(progressText, sizeof(progressText), tr(STR_HABIT_FOCUS_PROGRESS),
             static_cast<unsigned>(progress.pomodoroSessions), static_cast<unsigned>(habit->sessionsPerCycle));
  else
    snprintf(progressText, sizeof(progressText), "%lu / %u min", static_cast<unsigned long>(minutes),
             static_cast<unsigned>(habit->targetMinutes));
  renderer.drawCenteredText(NOTOSANS_18_FONT_ID, 111, progressText);

  if (habit->type == HabitType::Pomodoro && HABIT_TIMER.isForHabit(habitId)) {
    const auto phase = HABIT_TIMER.phaseFor(habitId);
    const char* label = phase == HabitTimer::Phase::Focus        ? tr(STR_HABIT_FOCUS)
                        : phase == HabitTimer::Phase::ShortBreak ? tr(STR_HABIT_SHORT_BREAK)
                                                                 : tr(STR_HABIT_LONG_BREAK);
    const uint16_t target = phase == HabitTimer::Phase::Focus        ? habit->targetMinutes
                            : phase == HabitTimer::Phase::ShortBreak ? habit->shortBreakMinutes
                                                                     : habit->longBreakMinutes;
    const uint32_t elapsed = HABIT_TIMER.elapsedSecondsFor(habitId);
    char phaseText[40];
    snprintf(phaseText, sizeof(phaseText), tr(STR_HABIT_PHASE_PROGRESS), label,
             static_cast<unsigned long>(elapsed / 60), static_cast<unsigned>(target));
    renderer.drawCenteredText(NOTOSANS_14_FONT_ID, 145, phaseText);
  }

  const int barX = SIDE_PAD;
  const int barW = screenW - SIDE_PAD * 2;
  renderer.drawRect(barX, 172, barW, 8, true);
  const uint32_t targetSeconds =
      static_cast<uint32_t>(habit->type == HabitType::Pomodoro ? habit->sessionsPerCycle : habit->targetMinutes * 60UL);
  const uint32_t achieved = habit->type == HabitType::Pomodoro ? progress.pomodoroSessions : progress.durationSeconds;
  const int fill = targetSeconds == 0 ? 0
                                      : static_cast<int>(std::min<uint64_t>(
                                            barW - 4, static_cast<uint64_t>(achieved) * (barW - 4) / targetSeconds));
  if (fill > 0) renderer.fillRect(barX + 2, 174, fill, 4, true);

  const auto labels = actionLabels();
  for (int i = 0; i < labels.count; ++i) {
    const int y = CONTENT_TOP + i * ROW_H;
    if (i == selection) renderer.drawRoundedRect(SIDE_PAD, y + 4, barW, ROW_H - 8, 2, 10, true);
    const auto shown = renderer.truncatedText(NOTOSANS_14_FONT_ID, labels.items[i], barW - 32);
    renderer.drawText(NOTOSANS_14_FONT_ID, SIDE_PAD + 16, y + 18, shown.c_str());
  }

  if (addMinutesPopup.processRender(renderer, mappedInput)) return;

  const auto hints = mappedInput.mapLabels("Back", "Select", "Up", "Down");
  GUI.drawButtonHints(renderer, hints.btn1, hints.btn2, hints.btn3, hints.btn4);
  renderer.displayBuffer(HalDisplay::FAST_REFRESH);
}
