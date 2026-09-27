#include "HabitSheepSettingsActivity.h"

#include <GfxRenderer.h>
#include <Memory.h>

#include <algorithm>

#include "HabitSheepStore.h"
#include "activities/ActivityManager.h"
#include "activities/habits/ActiveHabitsActivity.h"
#include "activities/habits/HabitLibraryActivity.h"
#include "activities/util/KeyboardEntryActivity.h"
#include "components/UITheme.h"

namespace fui = freeink::ui;

HabitSheepSettingsActivity::HabitSheepSettingsActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
    : UiListActivity("HabitSheepSettings", renderer, mappedInput) {
  rows[0].label = "Sheep name";
  rows[1].label = "Active habits";
  rows[2].label = "Habit library";
  rows[3].label = "Sleep sheep scene";
  rows[4].label = "CrossPoint settings";
  for (int i = 0; i < ROW_COUNT; ++i) rows[i].actionValue = static_cast<int16_t>(i);
}

void HabitSheepSettingsActivity::onEnter() {
  UiListActivity::onEnter();
  refreshRows();
}

void HabitSheepSettingsActivity::refreshRows() {
  values[0] = HABIT_SHEEP.getSheepName().empty() ? "Not named" : HABIT_SHEEP.getSheepName();

  const int activeCount =
      static_cast<int>(std::count_if(HABIT_SHEEP.getActiveHabitIds().begin(), HABIT_SHEEP.getActiveHabitIds().end(),
                                     [](const std::string& id) { return !id.empty(); }));
  values[1] = std::to_string(activeCount) + " / 3 selected";
  values[2] = std::to_string(HABIT_SHEEP.getHabits().size()) + " / 9 saved";
  values[3] = HABIT_SHEEP.isSleepSceneEnabled() ? "On" : "Off";
  values[4] = "Reader, display, network & system";

  for (int i = 0; i < ROW_COUNT; ++i) rows[i].subtitle = values[i].c_str();
}

void HabitSheepSettingsActivity::activateIndex(const int index) {
  nav.selected = index;
  app.clearTapFlash();

  if (index == 0) {
    auto handler = [this](const ActivityResult& result) {
      if (!result.isCancelled) {
        const auto& kb = std::get<KeyboardResult>(result.data);
        if (!kb.text.empty()) HABIT_SHEEP.setSheepName(kb.text);
        refreshRows();
        requestUpdate();
      }
    };
    startActivityForResult(
        std::make_unique<KeyboardEntryActivity>(renderer, mappedInput, "Name your sheep", HABIT_SHEEP.getSheepName(),
                                                HabitSheepStore::MAX_NAME_BYTES, InputType::Text),
        handler);
  } else if (index == 1) {
    activityManager.pushActivity(std::make_unique<ActiveHabitsActivity>(renderer, mappedInput));
  } else if (index == 2) {
    activityManager.pushActivity(std::make_unique<HabitLibraryActivity>(renderer, mappedInput));
  } else if (index == 3) {
    HABIT_SHEEP.setSleepSceneEnabled(!HABIT_SHEEP.isSleepSceneEnabled());
    refreshRows();
    requestUpdate();
  } else if (index == 4) {
    activityManager.goToSettings();
  }
}

void HabitSheepSettingsActivity::buildScreen(UiScreen& screen) {
  refreshRows();
  const auto& metrics = UITheme::getInstance().getMetrics();
  const Rect safe = UITheme::getInstance().getScreenSafeArea(renderer, true, false);
  screen.setContentMarginFromScreen(
      fui::Insets{static_cast<int16_t>(safe.y + metrics.topPadding + metrics.headerHeight),
                  static_cast<int16_t>(renderer.getScreenWidth() - (safe.x + safe.width)),
                  static_cast<int16_t>(renderer.getScreenHeight() - (safe.y + safe.height) + metrics.buttonHintsHeight),
                  static_cast<int16_t>(safe.x)});
  screen.spacer(static_cast<int16_t>(metrics.verticalSpacing));

  fui::ListProps props;
  props.items = rows.data();
  props.count = ROW_COUNT;
  props.action = ACTION_ROW;
  props.inputMask = fui::InputTouch;
  syncListViewport(screen, props);
  screen.list(props);
}
