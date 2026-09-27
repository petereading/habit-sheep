#include "ActiveHabitsActivity.h"

#include <GfxRenderer.h>

#include "HabitSheepStore.h"
#include "components/UITheme.h"

namespace fui = freeink::ui;

ActiveHabitsActivity::ActiveHabitsActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
    : UiListActivity("ActiveHabits", renderer, mappedInput) {
  rows[0].label = "Slot 1";
  rows[1].label = "Slot 2";
  rows[2].label = "Slot 3";
  for (int i = 0; i < 3; ++i) rows[i].actionValue = static_cast<int16_t>(i);
}

void ActiveHabitsActivity::onEnter() {
  UiListActivity::onEnter();
  refreshRows();
}

void ActiveHabitsActivity::refreshRows() {
  const auto& active = HABIT_SHEEP.getActiveHabitIds();
  for (int i = 0; i < 3; ++i) {
    const HabitDefinition* habit = active[i].empty() ? nullptr : HABIT_SHEEP.findHabit(active[i]);
    values[i] = habit ? habit->name : "Empty";
    rows[i].subtitle = values[i].c_str();
  }
}

void ActiveHabitsActivity::showPicker(const int slot) {
  const auto& active = HABIT_SHEEP.getActiveHabitIds();
  std::vector<std::string> labels;
  pickerIds.clear();
  labels.emplace_back("Empty");
  pickerIds.emplace_back("");

  for (const auto& habit : HABIT_SHEEP.getHabits()) {
    bool usedElsewhere = false;
    for (int i = 0; i < 3; ++i) {
      if (i != slot && active[i] == habit.id) {
        usedElsewhere = true;
        break;
      }
    }
    if (usedElsewhere) continue;
    labels.push_back(habit.name);
    pickerIds.push_back(habit.id);
  }

  std::vector<const char*> options;
  options.reserve(labels.size());
  for (const auto& label : labels) options.push_back(label.c_str());

  int current = 0;
  for (int i = 1; i < static_cast<int>(pickerIds.size()); ++i) {
    if (pickerIds[i] == active[slot]) {
      current = i;
      break;
    }
  }

  picker.show("Choose habit", options.data(), static_cast<int>(options.size()), current, [this, slot](const int index) {
    if (index < 0 || index >= static_cast<int>(pickerIds.size())) return;
    HABIT_SHEEP.setActiveHabit(slot, pickerIds[index]);
    refreshRows();
    requestUpdate();
  });
  requestUpdate();
}

void ActiveHabitsActivity::activateIndex(const int index) {
  nav.selected = index;
  app.clearTapFlash();
  showPicker(index);
}

bool ActiveHabitsActivity::handleCustomInput() {
  return picker.handleInput(mappedInput, [this] { requestUpdate(); });
}

void ActiveHabitsActivity::buildScreen(UiScreen& screen) {
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
  props.count = 3;
  props.action = ACTION_ROW;
  props.inputMask = fui::InputTouch;
  syncListViewport(screen, props);
  screen.list(props);
}

void ActiveHabitsActivity::render(RenderLock&& lock) {
  if (picker.processRender(renderer, mappedInput)) return;
  UiListActivity::render(std::move(lock));
}
