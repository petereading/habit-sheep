#include "ActiveHabitsActivity.h"

#include <GfxRenderer.h>

#include <algorithm>
#include <utility>

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
    const bool usedElsewhere = std::any_of(active.begin(), active.end(),
                                           [&](const std::string& id) { return id == habit.id && id != active[slot]; });
    if (usedElsewhere) continue;
    labels.push_back(habit.name);
    pickerIds.push_back(habit.id);
  }

  std::vector<const char*> options(labels.size());
  std::transform(labels.begin(), labels.end(), options.begin(), [](const std::string& label) { return label.c_str(); });

  const auto currentIt = std::find(pickerIds.begin() + 1, pickerIds.end(), active[slot]);
  const int current = currentIt == pickerIds.end() ? 0 : static_cast<int>(std::distance(pickerIds.begin(), currentIt));

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
