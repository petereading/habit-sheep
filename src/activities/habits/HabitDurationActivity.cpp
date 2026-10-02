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
#include "activities/util/KeyboardEntryActivity.h"
#include "components/HabitReward.h"
#include "components/UITheme.h"
#include "fontIds.h"

namespace {
constexpr int SIDE_PAD = 24;
constexpr int ROW_H = 58;

struct TimerLayout {
  int phaseY;
  int digitsY;
  int progressY;
  int barY;
  int actionsY;
  int rowHeight;
};

TimerLayout pomodoroLayout(const GfxRenderer& renderer) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int headerBottom = metrics.topPadding + metrics.headerHeight;
  const bool compact = renderer.getScreenHeight() <= 600;
  const int phaseY = headerBottom + (compact ? 8 : 24);
  const int digitsY = phaseY + 40;
  const int progressY = digitsY + (compact ? 70 : 76);
  const int barY = progressY + (compact ? 38 : 40);
  return {phaseY, digitsY, progressY, barY, barY + (compact ? 18 : 26), compact ? 40 : ROW_H};
}

TimerLayout durationLayout(const GfxRenderer& renderer) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int digitsY = metrics.topPadding + metrics.headerHeight + (renderer.getScreenHeight() <= 600 ? 14 : 26);
  const int barY = digitsY + 80;
  const bool compact = renderer.getScreenHeight() <= 600;
  return {0, digitsY, 0, barY, barY + (compact ? 18 : 26), compact ? 40 : ROW_H};
}

void drawLargeMinutes(const GfxRenderer& renderer, const int centerX, const int top, const uint32_t minutes,
                      const char* unit) {
  static constexpr uint8_t DIGITS[] = {0x3f, 0x06, 0x5b, 0x4f, 0x66, 0x6d, 0x7d, 0x07, 0x7f, 0x6f};
  char text[8];
  snprintf(text, sizeof(text), "%lu", static_cast<unsigned long>(std::min<uint32_t>(minutes, 9999)));
  const int count = static_cast<int>(strlen(text));
  const int start = centerX - (count * 44 + 8 + renderer.getTextWidth(NOTOSANS_14_FONT_ID, unit)) / 2;
  for (int i = 0; i < count; ++i) {
    const uint8_t mask = DIGITS[text[i] - '0'];
    const int x = start + i * 44;
    if (mask & 0x01) renderer.fillRect(x + 6, top, 30, 6);
    if (mask & 0x02) renderer.fillRect(x + 36, top + 5, 6, 23);
    if (mask & 0x04) renderer.fillRect(x + 36, top + 30, 6, 23);
    if (mask & 0x08) renderer.fillRect(x + 6, top + 52, 30, 6);
    if (mask & 0x10) renderer.fillRect(x, top + 30, 6, 23);
    if (mask & 0x20) renderer.fillRect(x, top + 5, 6, 23);
    if (mask & 0x40) renderer.fillRect(x + 6, top + 26, 30, 6);
  }
  renderer.drawText(NOTOSANS_14_FONT_ID, start + count * 44 + 8, top + 30, unit);
}
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
  static const char* OPTIONS[] = {"+5 minutes",  "+10 minutes", "+15 minutes", "+20 minutes",
                                  "+30 minutes", "+45 minutes", "+60 minutes", "Custom minutes"};
  addMinutesPopup.show(tr(STR_HABIT_ADD_MINUTES), OPTIONS, 8, 2, [this](const int selected) {
    static constexpr uint16_t MINUTES[] = {5, 10, 15, 20, 30, 45, 60};
    if (selected == 7) {
      showCustomMinutes();
      return;
    }
    if (selected < 0 || selected >= 7) return;
    HABIT_EVENTS.appendDurationSeconds(habitId, static_cast<uint32_t>(MINUTES[selected]) * 60,
                                       HabitEventSource::Manual);
    requestUpdate();
  });
  requestUpdate();
}

void HabitDurationActivity::showCustomMinutes() {
  if (HABIT_TIMER.isRunningFor(habitId)) HABIT_TIMER.pause(habitId);
  auto editor = makeUniqueNoThrow<KeyboardEntryActivity>(renderer, mappedInput, tr(STR_HABIT_ADD_MINUTES), "", 4,
                                                         InputType::Text);
  if (!editor) return;
  startActivityForResult(std::move(editor), [this](const ActivityResult& result) {
    if (result.isCancelled) return;
    const auto& value = std::get<KeyboardResult>(result.data).text;
    if (value.empty()) return;
    char* end = nullptr;
    const unsigned long minutes = strtoul(value.c_str(), &end, 10);
    if (*end != '\0' || minutes == 0 || minutes > 1440) return;
    HABIT_EVENTS.appendDurationSeconds(habitId, static_cast<uint32_t>(minutes) * 60, HabitEventSource::Manual);
    requestUpdate();
  });
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
  const TimerLayout layout = pomodoro ? pomodoroLayout(renderer) : durationLayout(renderer);
  const auto touch = mappedInput.rowTouch(row, layout.actionsY, layout.rowHeight, labels.count, SIDE_PAD,
                                          renderer.getScreenWidth() - SIDE_PAD, layout.rowHeight);
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

  GUI.drawHeader(renderer, header, habit->name.c_str(), nullptr, true, true);

  auto progress = HABIT_EVENTS.progressForToday(habitId);
  if (HABIT_TIMER.isForHabit(habitId) && HABIT_TIMER.phaseFor(habitId) == HabitTimer::Phase::Focus)
    progress.durationSeconds += HABIT_TIMER.elapsedSecondsFor(habitId);
  const uint32_t minutes = progress.durationSeconds / 60;

  char progressText[40];
  const bool pomodoro = habit->type == HabitType::Pomodoro;
  const TimerLayout layout = pomodoro ? pomodoroLayout(renderer) : durationLayout(renderer);
  if (pomodoro) {
    snprintf(progressText, sizeof(progressText), tr(STR_HABIT_FOCUS_PROGRESS),
             static_cast<unsigned>(progress.pomodoroSessions), static_cast<unsigned>(habit->sessionsPerCycle));
    const auto phase = HABIT_TIMER.phaseFor(habitId);
    const char* phaseLabel = phase == HabitTimer::Phase::Focus        ? tr(STR_HABIT_FOCUS)
                             : phase == HabitTimer::Phase::ShortBreak ? tr(STR_HABIT_SHORT_BREAK)
                                                                      : tr(STR_HABIT_LONG_BREAK);
    renderer.drawCenteredText(NOTOSANS_14_FONT_ID, layout.phaseY, phaseLabel);
    const uint32_t elapsed = HABIT_TIMER.elapsedSecondsFor(habitId);
    const uint32_t target = static_cast<uint32_t>(phase == HabitTimer::Phase::Focus        ? habit->targetMinutes
                                                  : phase == HabitTimer::Phase::ShortBreak ? habit->shortBreakMinutes
                                                                                           : habit->longBreakMinutes) *
                            60;
    const uint32_t shown = phase == HabitTimer::Phase::Focus ? elapsed / 60
                           : target > elapsed                ? (target - elapsed + 59) / 60
                                                             : 0;
    drawLargeMinutes(renderer, screenW / 2, layout.digitsY, shown,
                     phase == HabitTimer::Phase::Focus ? tr(STR_HABIT_MINUTES_ABBR) : tr(STR_HABIT_MINUTES_LEFT));
    renderer.drawCenteredText(NOTOSANS_14_FONT_ID, layout.progressY, progressText);
  } else {
    snprintf(progressText, sizeof(progressText), "/ %u %s", static_cast<unsigned>(habit->targetMinutes),
             tr(STR_HABIT_MINUTES_ABBR));
    drawLargeMinutes(renderer, screenW / 2, layout.digitsY, minutes, progressText);
  }

  const int barX = SIDE_PAD;
  const int barW = screenW - SIDE_PAD * 2;
  renderer.drawRect(barX, layout.barY, barW, 8, true);
  const uint32_t targetSeconds =
      static_cast<uint32_t>(habit->type == HabitType::Pomodoro ? habit->sessionsPerCycle : habit->targetMinutes * 60UL);
  const uint32_t achieved = habit->type == HabitType::Pomodoro ? progress.pomodoroSessions : progress.durationSeconds;
  const int fill = targetSeconds == 0 ? 0
                                      : static_cast<int>(std::min<uint64_t>(
                                            barW - 4, static_cast<uint64_t>(achieved) * (barW - 4) / targetSeconds));
  if (fill > 0) renderer.fillRect(barX + 2, layout.barY + 2, fill, 4, true);

  const auto labels = actionLabels();
  for (int i = 0; i < labels.count; ++i) {
    const int y = layout.actionsY + i * layout.rowHeight;
    if (i == selection) renderer.drawRoundedRect(SIDE_PAD, y + 4, barW, layout.rowHeight - 8, 2, 10, true);
    const auto shown = renderer.truncatedText(NOTOSANS_14_FONT_ID, labels.items[i], barW - 32);
    renderer.drawText(NOTOSANS_14_FONT_ID, SIDE_PAD + 16,
                      y + (layout.rowHeight - renderer.getLineHeight(NOTOSANS_14_FONT_ID)) / 2, shown.c_str());
  }

  if (addMinutesPopup.processRender(renderer, mappedInput)) return;

  const auto hints = mappedInput.mapLabels("Back", "Select", "Up", "Down");
  GUI.drawButtonHints(renderer, hints.btn1, hints.btn2, hints.btn3, hints.btn4);
  renderer.displayBuffer(HalDisplay::FAST_REFRESH);
}
