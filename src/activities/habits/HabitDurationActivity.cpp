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
#include "RecentBooksStore.h"
#include "activities/ActivityManager.h"
#include "components/UITheme.h"
#include "fontIds.h"

namespace {
constexpr int SIDE_PAD = 24;
constexpr int CONTENT_TOP = 150;
constexpr int ROW_H = 58;
}

HabitDurationActivity::HabitDurationActivity(GfxRenderer& renderer, MappedInputManager& mappedInput,
                                             std::string habitIdValue)
    : Activity("HabitDuration", renderer, mappedInput), habitId(std::move(habitIdValue)) {}

void HabitDurationActivity::onEnter() {
  Activity::onEnter();
  selection = 0;
  lastRenderedMinute = -1;
  requestUpdate();
}

std::vector<std::string> HabitDurationActivity::actionLabels() const {
  std::vector<std::string> labels;
  if (!HABIT_TIMER.isActive()) {
    labels.emplace_back("Start timer");
  } else if (HABIT_TIMER.isForHabit(habitId)) {
    labels.emplace_back(HABIT_TIMER.isRunning() ? "Pause timer" : "Resume timer");
    labels.emplace_back("Stop & log");
  } else {
    labels.emplace_back("Another timer is active");
  }
  labels.emplace_back("+ Add minutes");

  const HabitDefinition* habit = HABIT_SHEEP.findHabit(habitId);
  if (habit && habit->readingIntegration) {
    labels.emplace_back("Continue reading");
    labels.emplace_back("Browse files");
  }
  return labels;
}

void HabitDurationActivity::showAddMinutes() {
  static const char* OPTIONS[] = {"+5 minutes", "+10 minutes", "+15 minutes", "+20 minutes",
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
  for (const auto& book : books) {
    if (!RecentBooksStore::isMissing(book)) {
      activityManager.goToReader(book.path);
      return;
    }
  }
  activityManager.goToFileBrowser("/");
}

void HabitDurationActivity::activate() {
  const auto labels = actionLabels();
  if (selection < 0 || selection >= static_cast<int>(labels.size())) return;

  int index = 0;
  if (!HABIT_TIMER.isActive()) {
    if (selection == index++) {
      HABIT_TIMER.start(habitId);
      requestUpdate();
      return;
    }
  } else if (HABIT_TIMER.isForHabit(habitId)) {
    if (selection == index++) {
      if (HABIT_TIMER.isRunning())
        HABIT_TIMER.pause();
      else
        HABIT_TIMER.resume();
      requestUpdate();
      return;
    }
    if (selection == index++) {
      HABIT_TIMER.stopAndLog();
      requestUpdate();
      return;
    }
  } else {
    if (selection == index++) return;
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
  if (labels.empty()) return;
  selection = std::clamp(selection, 0, static_cast<int>(labels.size()) - 1);

  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    finish();
    return;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Up)) {
    selection = (selection - 1 + labels.size()) % labels.size();
    requestUpdate();
    return;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Down)) {
    selection = (selection + 1) % labels.size();
    requestUpdate();
    return;
  }

  int row = -1;
  const auto touch =
      mappedInput.rowTouch(row, CONTENT_TOP, ROW_H, labels.size(), SIDE_PAD, renderer.getScreenWidth() - SIDE_PAD, ROW_H);
  if (touch == MappedInputManager::RowTouch::Tap) {
    selection = row;
    activate();
    return;
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    activate();
    return;
  }

  if (HABIT_TIMER.isForHabit(habitId) && HABIT_TIMER.isRunning()) {
    const int minute = static_cast<int>(HABIT_TIMER.elapsedSeconds() / 60);
    if (minute != lastRenderedMinute) {
      lastRenderedMinute = minute;
      requestUpdate();
    }
  }
}

void HabitDurationActivity::render(RenderLock&&) {
  renderer.clearScreen();
  const int screenW = renderer.getScreenWidth();
  const int screenH = renderer.getScreenHeight();
  const HabitDefinition* habit = HABIT_SHEEP.findHabit(habitId);
  if (!habit) {
    GUI.drawHeader(renderer, Rect{0, 0, screenW, 90}, "Habit");
    renderer.drawCenteredText(NOTOSANS_14_FONT_ID, 180, "Habit not found");
    renderer.displayBuffer();
    return;
  }

  GUI.drawHeader(renderer, Rect{0, 0, screenW, 90}, habit->name.c_str());

  auto progress = HABIT_EVENTS.progressForToday(habitId);
  if (HABIT_TIMER.isForHabit(habitId)) progress.durationSeconds += HABIT_TIMER.elapsedSeconds();
  const uint32_t minutes = progress.durationSeconds / 60;

  char progressText[40];
  snprintf(progressText, sizeof(progressText), "%lu / %u min", static_cast<unsigned long>(minutes),
           static_cast<unsigned>(habit->targetMinutes));
  renderer.drawCenteredText(NOTOSANS_18_FONT_ID, 104, progressText);

  const int barX = SIDE_PAD;
  const int barW = screenW - SIDE_PAD * 2;
  renderer.drawRect(barX, 134, barW, 8, true);
  const uint32_t targetSeconds = static_cast<uint32_t>(habit->targetMinutes) * 60;
  const int fill = targetSeconds == 0 ? 0 : std::min<int>(barW - 4, progress.durationSeconds * (barW - 4) / targetSeconds);
  if (fill > 0) renderer.fillRect(barX + 2, 136, fill, 4, true);

  const auto labels = actionLabels();
  for (int i = 0; i < static_cast<int>(labels.size()); ++i) {
    const int y = CONTENT_TOP + i * ROW_H;
    if (i == selection) renderer.drawRoundedRect(SIDE_PAD, y + 4, barW, ROW_H - 8, 2, 10, true);
    const auto shown = renderer.truncatedText(NOTOSANS_14_FONT_ID, labels[i].c_str(), barW - 32);
    renderer.drawText(NOTOSANS_14_FONT_ID, SIDE_PAD + 16, y + 18, shown.c_str());
  }

  if (addMinutesPopup.processRender(renderer, mappedInput)) return;

  const auto hints = mappedInput.mapLabels("Back", "Select", "Up", "Down");
  GUI.drawButtonHints(renderer, hints.btn1, hints.btn2, hints.btn3, hints.btn4);
  renderer.displayBuffer(HalDisplay::FAST_REFRESH);
}
