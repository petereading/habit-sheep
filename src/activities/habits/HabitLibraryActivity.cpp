#include "HabitLibraryActivity.h"

#include <GfxRenderer.h>
#include <Memory.h>
#include <esp_random.h>

#include <algorithm>
#include <cstdio>
#include <utility>

#include "HabitHistoryActivity.h"
#include "HabitIconActivity.h"
#include "I18n.h"
#include "activities/util/IntervalSelectionActivity.h"
#include "activities/util/KeyboardEntryActivity.h"
#include "components/HabitUi.h"
#include "components/UITheme.h"

namespace fui = freeink::ui;

HabitLibraryActivity::HabitLibraryActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
    : UiListActivity("HabitLibrary", renderer, mappedInput) {}

void HabitLibraryActivity::onEnter() {
  habitUi::applyOrientation(renderer);
  popup.setHabitStyle();
  UiListActivity::onEnter();
  rebuildRows();
}

void HabitLibraryActivity::loop() {
  RenderLock lock;
  UiListActivity::loop();
}

void HabitLibraryActivity::rebuildRows() {
  labels.clear();
  subtitles.clear();
  rows.clear();

  const auto& habits = HABIT_SHEEP.getHabits();
  labels.reserve(habits.size() + 1);
  subtitles.reserve(habits.size() + 1);
  rows.reserve(habits.size() + 1);

  for (const auto& habit : habits) {
    labels.push_back(habit.name);
    if (habit.type == HabitType::Completion) {
      subtitles.emplace_back(std::to_string(habit.targetCount) + " / " +
                             (habit.period == HabitPeriod::Weekly ? tr(STR_HABIT_WEEKLY) : tr(STR_HABIT_DAILY)));
    } else if (habit.type == HabitType::Pomodoro) {
      subtitles.emplace_back(std::to_string(habit.targetMinutes) + "/" + std::to_string(habit.shortBreakMinutes) + "/" +
                             std::to_string(habit.longBreakMinutes) + " min");
    } else {
      std::string text = std::to_string(habit.targetCount) + " " + tr(STR_HABIT_SESSIONS) + " / " +
                         (habit.period == HabitPeriod::Weekly ? tr(STR_HABIT_WEEKLY) : tr(STR_HABIT_DAILY)) + " · " +
                         std::to_string(habit.targetMinutes) + " " + tr(STR_HABIT_MINUTES_ABBR);
      subtitles.push_back(std::move(text));
    }
  }
  if (habits.size() < HabitSheepStore::MAX_HABITS) {
    labels.emplace_back("+ Add habit");
    subtitles.emplace_back("");
  }

  for (size_t i = 0; i < labels.size(); ++i) {
    fui::ListItem item;
    item.label = labels[i].c_str();
    item.subtitle = subtitles[i].empty() ? nullptr : subtitles[i].c_str();
    item.actionValue = static_cast<int16_t>(i);
    rows.push_back(item);
  }
  if (!rows.empty()) nav.selected = std::min(nav.selected.load(), static_cast<int>(rows.size()) - 1);
}

void HabitLibraryActivity::startAddHabit() {
  auto handler = [this](const ActivityResult& result) {
    RenderLock lock;
    if (result.isCancelled) return;
    const auto& kb = std::get<KeyboardResult>(result.data);
    if (kb.text.empty()) return;
    pendingName = kb.text;
    chooseNewHabitType();
  };
  auto keyboard =
      makeUniqueNoThrow<KeyboardEntryActivity>(renderer, mappedInput, "Habit name", "", 64, InputType::Text);
  if (!keyboard) {
    LOG_ERR("HABIT", "OOM: habit keyboard");
    return;
  }
  startActivityForResult(std::move(keyboard), handler);
}

void HabitLibraryActivity::chooseNewHabitType() {
  pendingPeriod = HabitPeriod::Daily;
  pendingTargetCount = 1;
  const char* OPTIONS[] = {"Completion", "Duration", tr(STR_HABIT_POMODORO)};
  popup.show("Habit type", OPTIONS, 3, 0, [this](const int index) {
    pendingType = index == 2 ? HabitType::Pomodoro : index == 1 ? HabitType::Duration : HabitType::Completion;
    if (pendingType != HabitType::Completion)
      chooseDurationTarget();
    else
      chooseCompletionPeriod();
  });
  requestUpdate();
}

void HabitLibraryActivity::chooseCompletionPeriod() {
  const char* options[] = {tr(STR_HABIT_DAILY), tr(STR_HABIT_WEEKLY)};
  popup.show(tr(STR_HABIT_PERIOD), options, 2, 0, [this](const int selected) {
    pendingPeriod = selected == 1 ? HabitPeriod::Weekly : HabitPeriod::Daily;
    chooseCompletionTarget();
  });
  requestUpdate();
}

void HabitLibraryActivity::chooseCompletionTarget() {
  auto picker = makeUniqueNoThrow<IntervalSelectionActivity>(
      renderer, mappedInput, "HabitTarget",
      pendingType == HabitType::Duration ? StrId::STR_HABIT_SESSION_TARGET : StrId::STR_HABIT_TARGET_COUNT, 1, 1, 99, 1,
      5);
  if (!picker) {
    LOG_ERR("HABIT", "OOM: target picker");
    return;
  }
  startActivityForResult(std::move(picker), [this](const ActivityResult& result) {
    RenderLock lock;
    if (result.isCancelled) return;
    pendingTargetCount = std::get<IntervalResult>(result.data).value;
    if (pendingType == HabitType::Duration)
      chooseReadingIntegration();
    else
      savePendingHabit(false);
  });
}

void HabitLibraryActivity::chooseDurationTarget() {
  auto picker = makeUniqueNoThrow<IntervalSelectionActivity>(
      renderer, mappedInput, "HabitSessionLength",
      pendingType == HabitType::Pomodoro ? StrId::STR_HABIT_FOCUS_DURATION : StrId::STR_HABIT_SESSION_LENGTH,
      pendingType == HabitType::Pomodoro ? 25 : 30, 1, 1440, 1, 15, StrId::STR_HABIT_MINUTE_VALUE);
  if (!picker) {
    LOG_ERR("HABIT", "OOM: session picker");
    return;
  }
  startActivityForResult(std::move(picker), [this](const ActivityResult& result) {
    RenderLock lock;
    if (result.isCancelled) return;
    pendingTargetMinutes = std::get<IntervalResult>(result.data).value;
    if (pendingType == HabitType::Duration)
      chooseCompletionPeriod();
    else
      savePendingHabit(false);
  });
}

void HabitLibraryActivity::chooseReadingIntegration() {
  const char* OPTIONS[] = {tr(STR_HABIT_TIMER_ONLY), tr(STR_HABIT_READING_SHORTCUTS)};
  popup.show(tr(STR_HABIT_READER_LINKS), tr(STR_HABIT_READING_HELP), OPTIONS, 2, 0,
             [this](const int index) { savePendingHabit(index == 1); });
  requestUpdate();
}

void HabitLibraryActivity::savePendingHabit(const bool readingIntegration) {
  char id[32];
  snprintf(id, sizeof(id), "h-%08lX-%08lX", static_cast<unsigned long>(esp_random()),
           static_cast<unsigned long>(esp_random()));

  HabitDefinition habit;
  habit.id = id;
  habit.name = pendingName;
  habit.type = pendingType;
  habit.targetMinutes = pendingType == HabitType::Completion ? 0 : pendingTargetMinutes;
  habit.readingIntegration = pendingType == HabitType::Duration && readingIntegration;
  habit.period = pendingPeriod;
  habit.targetCount = pendingTargetCount;
  auto picker = makeUniqueNoThrow<HabitIconActivity>(renderer, mappedInput, habitUi::iconFor(habit));
  if (!picker) {
    LOG_ERR("HABIT", "OOM: icon picker");
    return;
  }
  startActivityForResult(std::move(picker), [this, habit](const ActivityResult& result) mutable {
    RenderLock lock;
    if (result.isCancelled) return;
    habit.icon = static_cast<uint8_t>(std::get<IntervalResult>(result.data).value);
    if (HABIT_SHEEP.upsertHabit(habit)) {
      // Fill the first empty active slot. Users can immediately use the habit,
      // while still retaining explicit 3-slot control in Active Habits.
      auto active = HABIT_SHEEP.getActiveHabitIds();
      for (int i = 0; i < static_cast<int>(active.size()); ++i) {
        if (active[i].empty()) {
          HABIT_SHEEP.setActiveHabit(i, habit.id);
          break;
        }
      }
    }
    pendingName.clear();
    rebuildRows();
    requestUpdate();
  });
}

void HabitLibraryActivity::changeIcon(const std::string& id) {
  const auto* found = HABIT_SHEEP.findHabit(id);
  if (!found) return;
  auto habit = *found;
  auto picker = makeUniqueNoThrow<HabitIconActivity>(renderer, mappedInput, habitUi::iconFor(habit));
  if (!picker) {
    LOG_ERR("HABIT", "OOM: icon picker");
    return;
  }
  startActivityForResult(std::move(picker), [this, habit](const ActivityResult& result) mutable {
    RenderLock lock;
    if (!result.isCancelled) {
      habit.icon = static_cast<uint8_t>(std::get<IntervalResult>(result.data).value);
      HABIT_SHEEP.upsertHabit(habit);
      rebuildRows();
    }
  });
}

void HabitLibraryActivity::renameHabit(const std::string& habitId) {
  const HabitDefinition* found = HABIT_SHEEP.findHabit(habitId);
  if (!found) return;
  HabitDefinition original = *found;
  auto handler = [this, original](const ActivityResult& result) mutable {
    RenderLock lock;
    if (result.isCancelled) return;
    const auto& kb = std::get<KeyboardResult>(result.data);
    if (kb.text.empty()) return;
    original.name = kb.text;
    HABIT_SHEEP.upsertHabit(original);
    rebuildRows();
    requestUpdate();
  };
  auto keyboard = makeUniqueNoThrow<KeyboardEntryActivity>(renderer, mappedInput, "Rename habit", original.name, 64,
                                                           InputType::Text);
  if (!keyboard) {
    LOG_ERR("HABIT", "OOM: rename keyboard");
    return;
  }
  startActivityForResult(std::move(keyboard), handler);
}

void HabitLibraryActivity::changeTarget(const std::string& habitId) {
  const HabitDefinition* found = HABIT_SHEEP.findHabit(habitId);
  if (!found || found->type == HabitType::Completion) return;
  HabitDefinition original = *found;
  auto picker = makeUniqueNoThrow<IntervalSelectionActivity>(
      renderer, mappedInput, "HabitSessionLength",
      original.type == HabitType::Pomodoro ? StrId::STR_HABIT_FOCUS_DURATION : StrId::STR_HABIT_SESSION_LENGTH,
      original.targetMinutes, 1, 1440, 1, 15, StrId::STR_HABIT_MINUTE_VALUE);
  if (!picker) {
    LOG_ERR("HABIT", "OOM: session picker");
    return;
  }
  startActivityForResult(std::move(picker), [this, original](const ActivityResult& result) mutable {
    RenderLock lock;
    if (result.isCancelled) return;
    original.targetMinutes = std::get<IntervalResult>(result.data).value;
    HABIT_SHEEP.upsertHabit(original);
    rebuildRows();
    requestUpdate();
  });
}

void HabitLibraryActivity::changeCompletionPeriod(const std::string& habitId) {
  const HabitDefinition* found = HABIT_SHEEP.findHabit(habitId);
  if (!found || found->type == HabitType::Pomodoro) return;
  HabitDefinition updated = *found;
  const char* options[] = {tr(STR_HABIT_DAILY), tr(STR_HABIT_WEEKLY)};
  popup.show(tr(STR_HABIT_PERIOD), options, 2, updated.period == HabitPeriod::Weekly ? 1 : 0,
             [this, updated](const int selected) mutable {
               updated.period = selected == 1 ? HabitPeriod::Weekly : HabitPeriod::Daily;
               HABIT_SHEEP.upsertHabit(updated);
               rebuildRows();
               requestUpdate();
             });
  requestUpdate();
}

void HabitLibraryActivity::changeCompletionTarget(const std::string& habitId) {
  const HabitDefinition* found = HABIT_SHEEP.findHabit(habitId);
  if (!found || found->type == HabitType::Pomodoro) return;
  HabitDefinition updated = *found;
  auto picker = makeUniqueNoThrow<IntervalSelectionActivity>(
      renderer, mappedInput, "HabitTarget",
      updated.type == HabitType::Duration ? StrId::STR_HABIT_SESSION_TARGET : StrId::STR_HABIT_TARGET_COUNT,
      updated.targetCount, 1, 99, 1, 5);
  if (!picker) {
    LOG_ERR("HABIT", "OOM: target picker");
    return;
  }
  startActivityForResult(std::move(picker), [this, updated](const ActivityResult& result) mutable {
    RenderLock lock;
    if (result.isCancelled) return;
    updated.targetCount = std::get<IntervalResult>(result.data).value;
    HABIT_SHEEP.upsertHabit(updated);
    rebuildRows();
    requestUpdate();
  });
}

void HabitLibraryActivity::changePomodoroBreak(const std::string& habitId, const bool longBreak) {
  const HabitDefinition* found = HABIT_SHEEP.findHabit(habitId);
  if (!found || found->type != HabitType::Pomodoro) return;
  HabitDefinition updated = *found;
  static const char* OPTIONS[] = {"3 minutes", "5 minutes", "10 minutes", "15 minutes", "20 minutes", "30 minutes"};
  static constexpr uint16_t MINUTES[] = {3, 5, 10, 15, 20, 30};
  const uint16_t saved = longBreak ? updated.longBreakMinutes : updated.shortBreakMinutes;
  int current = longBreak ? 3 : 1;
  for (int i = 0; i < 6; ++i) {
    if (MINUTES[i] == saved) current = i;
  }
  popup.show(longBreak ? tr(STR_HABIT_LONG_BREAK) : tr(STR_HABIT_SHORT_BREAK), OPTIONS, 6, current,
             [this, updated, longBreak](const int index) mutable {
               static constexpr uint16_t MINUTES[] = {3, 5, 10, 15, 20, 30};
               if (index < 0 || index >= 6) return;
               if (longBreak)
                 updated.longBreakMinutes = MINUTES[index];
               else
                 updated.shortBreakMinutes = MINUTES[index];
               HABIT_SHEEP.upsertHabit(updated);
               rebuildRows();
               requestUpdate();
             });
  requestUpdate();
}

void HabitLibraryActivity::changePomodoroSessions(const std::string& habitId) {
  const HabitDefinition* found = HABIT_SHEEP.findHabit(habitId);
  if (!found || found->type != HabitType::Pomodoro) return;
  HabitDefinition updated = *found;
  static const char* OPTIONS[] = {"2 sessions", "3 sessions", "4 sessions", "5 sessions", "6 sessions"};
  const int current = updated.sessionsPerCycle >= 2 && updated.sessionsPerCycle <= 6 ? updated.sessionsPerCycle - 2 : 2;
  popup.show(tr(STR_HABIT_FOCUS_CYCLE_LENGTH), OPTIONS, 5, current, [this, updated](const int index) mutable {
    if (index < 0 || index >= 5) return;
    updated.sessionsPerCycle = static_cast<uint8_t>(index + 2);
    HABIT_SHEEP.upsertHabit(updated);
    rebuildRows();
    requestUpdate();
  });
  requestUpdate();
}

void HabitLibraryActivity::toggleReadingIntegration(const std::string& habitId) {
  const HabitDefinition* found = HABIT_SHEEP.findHabit(habitId);
  if (!found || found->type != HabitType::Duration) return;
  HabitDefinition updated = *found;
  updated.readingIntegration = !updated.readingIntegration;
  HABIT_SHEEP.upsertHabit(updated);
  rebuildRows();
  requestUpdate();
}

void HabitLibraryActivity::confirmDelete(const std::string& habitId) {
  static const char* OPTIONS[] = {"Cancel", "Delete"};
  popup.show("Delete habit?", OPTIONS, 2, 0, [this, habitId](const int index) {
    if (index == 1) HABIT_SHEEP.removeHabit(habitId);
    rebuildRows();
    requestUpdate();
  });
  requestUpdate();
}

void HabitLibraryActivity::showHistory(const std::string& habitId) {
  const auto* habit = HABIT_SHEEP.findHabit(habitId);
  if (!habit) return;
  auto history = makeUniqueNoThrow<HabitHistoryActivity>(renderer, mappedInput, *habit);
  if (history)
    activityManager.pushActivity(std::move(history));
  else
    LOG_ERR("HABIT", "OOM: habit history");
}

void HabitLibraryActivity::showEditMenu(const std::string& habitId) {
  const HabitDefinition* habit = HABIT_SHEEP.findHabit(habitId);
  if (!habit) return;

  if (habit->type == HabitType::Pomodoro) {
    const char* OPTIONS[] = {"Rename",
                             tr(STR_HABIT_FOCUS_DURATION),
                             tr(STR_HABIT_SHORT_BREAK),
                             tr(STR_HABIT_LONG_BREAK),
                             tr(STR_HABIT_FOCUS_CYCLE_LENGTH),
                             "Delete",
                             tr(STR_HABIT_ICON),
                             tr(STR_HABIT_HISTORY)};
    popup.show(habit->name.c_str(), OPTIONS, 8, 0, [this, habitId](const int index) {
      if (index == 0)
        renameHabit(habitId);
      else if (index == 1)
        changeTarget(habitId);
      else if (index == 2)
        changePomodoroBreak(habitId, false);
      else if (index == 3)
        changePomodoroBreak(habitId, true);
      else if (index == 4)
        changePomodoroSessions(habitId);
      else if (index == 5)
        confirmDelete(habitId);
      else if (index == 6)
        changeIcon(habitId);
      else if (index == 7)
        showHistory(habitId);
    });
  } else if (habit->type == HabitType::Duration) {
    const char* OPTIONS[] = {
        "Rename",           tr(STR_HABIT_SESSION_LENGTH), tr(STR_HABIT_AUTO_READING),   "Delete",
        tr(STR_HABIT_ICON), tr(STR_HABIT_PERIOD),         tr(STR_HABIT_SESSION_TARGET), tr(STR_HABIT_HISTORY)};
    popup.show(habit->name.c_str(), OPTIONS, 8, 0, [this, habitId](const int index) {
      if (index == 0)
        renameHabit(habitId);
      else if (index == 1)
        changeTarget(habitId);
      else if (index == 2)
        toggleReadingIntegration(habitId);
      else if (index == 3)
        confirmDelete(habitId);
      else if (index == 4)
        changeIcon(habitId);
      else if (index == 5)
        changeCompletionPeriod(habitId);
      else if (index == 6)
        changeCompletionTarget(habitId);
      else if (index == 7)
        showHistory(habitId);
    });
  } else {
    const char* OPTIONS[] = {"Rename", tr(STR_HABIT_PERIOD), tr(STR_HABIT_TARGET_COUNT),
                             "Delete", tr(STR_HABIT_ICON),   tr(STR_HABIT_HISTORY)};
    popup.show(habit->name.c_str(), OPTIONS, 6, 0, [this, habitId](const int index) {
      if (index == 0)
        renameHabit(habitId);
      else if (index == 1)
        changeCompletionPeriod(habitId);
      else if (index == 2)
        changeCompletionTarget(habitId);
      else if (index == 3)
        confirmDelete(habitId);
      else if (index == 4)
        changeIcon(habitId);
      else if (index == 5)
        showHistory(habitId);
    });
  }
  requestUpdate();
}

void HabitLibraryActivity::activateIndex(const int index) {
  nav.selected = index;
  app.clearTapFlash();
  const auto& habits = HABIT_SHEEP.getHabits();
  if (index >= 0 && index < static_cast<int>(habits.size())) {
    showEditMenu(habits[index].id);
  } else if (habits.size() < HabitSheepStore::MAX_HABITS && index == static_cast<int>(habits.size())) {
    startAddHabit();
  }
}

bool HabitLibraryActivity::handleCustomInput() {
  return popup.handleInput(mappedInput, [this] { requestUpdate(); });
}

void HabitLibraryActivity::buildScreen(UiScreen& screen) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const Rect safe = UITheme::getInstance().getScreenSafeArea(renderer, true, false);
  screen.setContentMarginFromScreen(
      fui::Insets{static_cast<int16_t>(safe.y + metrics.topPadding + metrics.headerHeight),
                  static_cast<int16_t>(renderer.getScreenWidth() - (safe.x + safe.width)),
                  static_cast<int16_t>(renderer.getScreenHeight() - (safe.y + safe.height) + metrics.buttonHintsHeight),
                  static_cast<int16_t>(safe.x)});
  screen.spacer(static_cast<int16_t>(metrics.verticalSpacing));

  if (rows.empty()) {
    screen.centeredText("Habit library is full", screen.theme().bodyText);
    return;
  }

  fui::ListProps props;
  props.items = rows.data();
  props.count = static_cast<uint16_t>(rows.size());
  props.action = ACTION_ROW;
  props.inputMask = fui::InputTouch;
  syncListViewport(screen, props);
  screen.list(props);
}

void HabitLibraryActivity::render(RenderLock&& lock) {
  if (popup.processRender(renderer, mappedInput)) return;
  UiListActivity::render(std::move(lock));
}
