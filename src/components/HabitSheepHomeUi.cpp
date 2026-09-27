#include "HabitSheepHomeUi.h"

#include <GfxRenderer.h>
#include <HalPowerManager.h>

#include <algorithm>
#include <cstdio>

#include "HabitSheepStore.h"
#include "HabitEventLog.h"
#include "HabitTimer.h"
#include "MappedInputManager.h"
#include "components/icons/blocks.h"
#include "components/icons/book.h"
#include "components/icons/folder.h"
#include "components/icons/library.h"
#include "components/icons/settings2.h"
#include "components/icons/transfer.h"
#include "components/themes/BaseTheme.h"
#include "fontIds.h"

namespace {
constexpr int SIDE_PAD = 22;
constexpr int HEADER_H = 52;
constexpr int DOCK_H = 74;
constexpr int HABIT_ROW_H = 62;
constexpr int ICON_SIZE = 32;
constexpr int DOCK_COUNT = 6;

const uint8_t* dockIcon(const int index) {
  switch (index) {
    case 0:
      return BookIcon;
    case 1:
      return FolderIcon;
    case 2:
      return LibraryIcon;
    case 3:
      return BlocksIcon;
    case 4:
      return TransferIcon;
    case 5:
      return Settings2Icon;
    default:
      return nullptr;
  }
}
}  // namespace

void HabitSheepHomeUi::setSelection(const int value) { selection = std::clamp(value, 0, SELECTION_COUNT - 1); }

int HabitSheepHomeUi::nextSelection(const int value) const { return (value + 1 + SELECTION_COUNT) % SELECTION_COUNT; }

int HabitSheepHomeUi::previousSelection(const int value) const {
  return (value - 1 + SELECTION_COUNT) % SELECTION_COUNT;
}

HabitSheepHomeUi::Action HabitSheepHomeUi::actionForSelection(const int value) const {
  if (value < 0 || value >= SELECTION_COUNT) return Action::None;
  return static_cast<Action>(value + 1);
}

int HabitSheepHomeUi::selectedAction(MappedInputManager& input) const {
  const int screenW = renderer.getScreenWidth();
  const int screenH = renderer.getScreenHeight();
  const int habitsTop = screenH - DOCK_H - 3 * HABIT_ROW_H;
  const int sheepTop = HEADER_H;
  const int sheepHeight = std::max(0, habitsTop - sheepTop);

  if (input.wasTapInRect(0, sheepTop, screenW, sheepHeight)) return 0;

  int row = -1;
  const auto habitTouch = input.rowTouch(row, habitsTop, HABIT_ROW_H, 3, SIDE_PAD, screenW - SIDE_PAD, HABIT_ROW_H);
  if (habitTouch == MappedInputManager::RowTouch::Tap) return 1 + row;

  int col = -1;
  const int slotW = screenW / DOCK_COUNT;
  const auto dockTouch = input.colTouch(col, 0, slotW, DOCK_COUNT, screenH - DOCK_H, screenH, slotW);
  if (dockTouch == MappedInputManager::RowTouch::Tap) return 4 + col;

  return -1;
}

void HabitSheepHomeUi::drawSheep(const int x, const int y, const int width, const int height, const char* name) const {
  if (selection == 0) renderer.drawRoundedRect(x, y, width, height, 2, 16, true);

  const int cx = x + width / 2;
  const int bodyW = std::min(150, width / 2);
  const int bodyH = 82;
  const int bodyX = cx - bodyW / 2;
  const int bodyY = y + std::max(42, height / 2 - 55);

  renderer.drawRoundedRect(bodyX, bodyY, bodyW, bodyH, 3, 28, true);
  renderer.fillRoundedRect(bodyX + bodyW - 34, bodyY + 22, 46, 48, 14, Color::Black);
  renderer.fillRect(bodyX + 24, bodyY + bodyH - 2, 8, 28, true);
  renderer.fillRect(bodyX + bodyW - 42, bodyY + bodyH - 2, 8, 28, true);
  renderer.fillRect(bodyX + bodyW - 20, bodyY + 37, 4, 4, false);
  renderer.fillRect(bodyX + bodyW - 7, bodyY + 37, 4, 4, false);

  if (sheepNudge == 1) {
    renderer.drawLine(bodyX + bodyW + 15, bodyY + 10, bodyX + bodyW + 28, bodyY + 2, 2, true);
    renderer.drawLine(bodyX + bodyW + 16, bodyY + 20, bodyX + bodyW + 31, bodyY + 20, 2, true);
  } else if (sheepNudge == 2) {
    renderer.drawLine(bodyX - 10, bodyY + 18, bodyX - 24, bodyY + 8, 2, true);
    renderer.drawLine(bodyX - 9, bodyY + 28, bodyX - 25, bodyY + 30, 2, true);
  }

  const char* label = (name && *name) ? name : "Habit Sheep";
  const auto shown = renderer.truncatedText(UI_12_FONT_ID, label, width - 24);
  const int textW = renderer.getTextWidth(UI_12_FONT_ID, shown.c_str());
  renderer.drawText(UI_12_FONT_ID, x + (width - textW) / 2, y + 14, shown.c_str());
}

void HabitSheepHomeUi::drawHabitRows(const HabitSheepStore& store, const int top, const int height) const {
  const int screenW = renderer.getScreenWidth();
  const auto& active = store.getActiveHabitIds();

  for (int i = 0; i < 3; ++i) {
    const int y = top + i * height;
    const bool selected = selection == 1 + i;
    if (selected) renderer.drawRoundedRect(SIDE_PAD, y + 4, screenW - SIDE_PAD * 2, height - 8, 2, 10, true);

    const HabitDefinition* habit = active[i].empty() ? nullptr : store.findHabit(active[i]);
    const char* name = habit ? habit->name.c_str() : "Set habit";
    const auto shown = renderer.truncatedText(NOTOSANS_14_FONT_ID, name, screenW - SIDE_PAD * 2 - 105);
    renderer.drawText(NOTOSANS_14_FONT_ID, SIDE_PAD + 16, y + 18, shown.c_str());

    if (!habit) {
      renderer.drawText(SMALL_FONT_ID, screenW - SIDE_PAD - 52, y + 22, "+");
    } else if (habit->type == HabitType::Completion) {
      const auto progress = HABIT_EVENTS.progressForToday(habit->id);
      renderer.drawRoundedRect(screenW - SIDE_PAD - 42, y + 15, 22, 22, 2, 4, true);
      if (progress.completed) {
        renderer.drawLine(screenW - SIDE_PAD - 37, y + 27, screenW - SIDE_PAD - 31, y + 33, 2, true);
        renderer.drawLine(screenW - SIDE_PAD - 31, y + 33, screenW - SIDE_PAD - 22, y + 20, 2, true);
      }
    } else {
      auto progress = HABIT_EVENTS.progressForToday(habit->id);
      if (HABIT_TIMER.isForHabit(habit->id)) progress.durationSeconds += HABIT_TIMER.elapsedSeconds();
      const uint32_t minutes = progress.durationSeconds / 60;

      char target[24];
      snprintf(target, sizeof(target), "%lu/%um", static_cast<unsigned long>(minutes),
               static_cast<unsigned>(habit->targetMinutes));
      const int targetW = renderer.getTextWidth(SMALL_FONT_ID, target);
      renderer.drawText(SMALL_FONT_ID, screenW - SIDE_PAD - 16 - targetW, y + 15, target);

      const int barX = screenW - SIDE_PAD - 104;
      const int barY = y + 39;
      const int barW = 86;
      renderer.drawRect(barX, barY, barW, 7, true);
      const uint32_t targetSeconds = static_cast<uint32_t>(habit->targetMinutes) * 60;
      const int fill = targetSeconds == 0 ? 0 : std::min<int>(barW - 4, progress.durationSeconds * (barW - 4) / targetSeconds);
      if (fill > 0) renderer.fillRect(barX + 2, barY + 2, fill, 3, true);
    }
  }
}

void HabitSheepHomeUi::drawDock(const int top, const int height) const {
  const int screenW = renderer.getScreenWidth();
  const int slotW = screenW / DOCK_COUNT;
  renderer.drawLine(0, top, screenW, top, true);

  for (int i = 0; i < DOCK_COUNT; ++i) {
    const int slotX = i * slotW;
    const int iconX = slotX + (slotW - ICON_SIZE) / 2;
    const int iconY = top + (height - ICON_SIZE) / 2;
    if (selection == 4 + i) renderer.drawRoundedRect(slotX + 7, top + 8, slotW - 14, height - 16, 2, 10, true);
    const uint8_t* icon = dockIcon(i);
    if (icon) renderer.drawIcon(icon, iconX, iconY, ICON_SIZE);
  }
}

void HabitSheepHomeUi::renderUi(const HabitSheepStore& store) const {
  const int screenW = renderer.getScreenWidth();
  const int screenH = renderer.getScreenHeight();
  const int habitsTop = screenH - DOCK_H - 3 * HABIT_ROW_H;
  const int sheepTop = HEADER_H;
  const int sheepHeight = std::max(0, habitsTop - sheepTop);

  const uint16_t rawBattery = powerManager.getBatteryPercentage();
  const uint16_t bucket = static_cast<uint16_t>(std::min(100, ((rawBattery + 5) / 10) * 10));
  GUI.fillBatteryIcon(renderer, Rect{screenW - 42, 16, 24, 18}, bucket);

  drawSheep(SIDE_PAD, sheepTop + 4, screenW - SIDE_PAD * 2, sheepHeight - 8, store.getSheepName().c_str());
  drawHabitRows(store, habitsTop, HABIT_ROW_H);
  drawDock(screenH - DOCK_H, DOCK_H);
}
