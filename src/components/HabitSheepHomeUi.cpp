#include "HabitSheepHomeUi.h"

#include <Bitmap.h>
#include <GfxRenderer.h>
#include <HalClock.h>
#include <HalPowerManager.h>
#include <HalStorage.h>
#include <Logging.h>
#include <Memory.h>

#include <algorithm>
#include <cstdio>
#include <ctime>

#include "CrossPointSettings.h"
#include "HabitEventLog.h"
#include "HabitSheepStore.h"
#include "HabitTimer.h"
#include "I18n.h"
#include "MappedInputManager.h"
#include "RecentBooksStore.h"
#include "SheepStateStore.h"
#include "components/UITheme.h"
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

int HabitSheepHomeUi::nextSelection(const int value) {
  int next = value;
  do {
    next = (next + 1 + SELECTION_COUNT) % SELECTION_COUNT;
  } while (next >= 1 && next <= 3 && (!HABIT_SHEEP.isEnabled() || HABIT_SHEEP.getActiveHabitIds()[next - 1].empty()));
  return next;
}

int HabitSheepHomeUi::previousSelection(const int value) {
  int previous = value;
  do {
    previous = (previous - 1 + SELECTION_COUNT) % SELECTION_COUNT;
  } while (previous >= 1 && previous <= 3 &&
           (!HABIT_SHEEP.isEnabled() || HABIT_SHEEP.getActiveHabitIds()[previous - 1].empty()));
  return previous;
}

HabitSheepHomeUi::Action HabitSheepHomeUi::actionForSelection(const int value) {
  if (value < 0 || value >= SELECTION_COUNT) return Action::None;
  return static_cast<Action>(value + 1);
}

int HabitSheepHomeUi::selectedAction(MappedInputManager& input) const {
  const int screenW = renderer.getScreenWidth();
  const int screenH = renderer.getScreenHeight();
  const int habitsTop = screenH - DOCK_H - 3 * HABIT_ROW_H;
  const int sheepTop = HEADER_H;
  const int sheepHeight = std::max(0, habitsTop - sheepTop);

  if (input.wasTapInRect(0, sheepTop, screenW, HABIT_SHEEP.isEnabled() ? sheepHeight : screenH - DOCK_H - sheepTop))
    return 0;

  int row = -1;
  const auto habitTouch = input.rowTouch(row, habitsTop, HABIT_ROW_H, 3, SIDE_PAD, screenW - SIDE_PAD, HABIT_ROW_H);
  if (HABIT_SHEEP.isEnabled() && habitTouch == MappedInputManager::RowTouch::Tap &&
      !HABIT_SHEEP.getActiveHabitIds()[row].empty())
    return 1 + row;

  int col = -1;
  const int slotW = screenW / DOCK_COUNT;
  const auto dockTouch = input.colTouch(col, 0, slotW, DOCK_COUNT, screenH - DOCK_H, screenH, slotW);
  if (dockTouch == MappedInputManager::RowTouch::Tap) return 4 + col;

  return -1;
}

int HabitSheepHomeUi::longPressedHabit(MappedInputManager& input) const {
  if (!HABIT_SHEEP.isEnabled()) return -1;
  const int screenH = renderer.getScreenHeight();
  const int habitsTop = screenH - DOCK_H - 3 * HABIT_ROW_H;

  int x = 0;
  int y = 0;
  if (input.wasScreenLongPress(x, y) && x >= SIDE_PAD && x < renderer.getScreenWidth() - SIDE_PAD && y >= habitsTop &&
      y < habitsTop + 3 * HABIT_ROW_H) {
    const int slot = (y - habitsTop) / HABIT_ROW_H;
    return HABIT_SHEEP.getActiveHabitIds()[slot].empty() ? -1 : slot;
  }

  if (selection >= 1 && selection <= 3 && input.wasLongPressed(MappedInputManager::Button::Confirm, 700)) {
    return HABIT_SHEEP.getActiveHabitIds()[selection - 1].empty() ? -1 : selection - 1;
  }
  return -1;
}

void HabitSheepHomeUi::drawPasture(const int x, const int y, const int width, const int height) const {
  const int groundY = y + height - 28;
  renderer.drawLine(x + 8, groundY, x + width - 8, groundY, 2, true);

  const int level = std::min<int>(8, SHEEP_STATE.getGrassStock());
  const int span = std::max(48, width - 56);
  for (int i = 0; i < level; ++i) {
    const int px = x + 28 + (i * 47) % span;
    const int py = groundY - 5 - (i % 2) * 7;
    renderer.drawLine(px, py, px, py - 14, true);
    renderer.drawLine(px, py - 11, px - 5, py - 16, true);
    renderer.drawLine(px, py - 11, px + 5, py - 16, true);
    if (i >= 4) renderer.fillRect(px - 2, py - 20, 5, 5, true);
  }
}

void HabitSheepHomeUi::drawSheep(const int x, const int y, const int width, const int height, const char* name,
                                 const bool showSelection) const {
  if (showSelection && selection == 0) renderer.drawRoundedRect(x, y, width, height, 2, 16, true);

  tm local{};
  const bool hasTime = halClock.isAvailable() && halClock.localTime(local);
  const bool sleeping = SHEEP_STATE.isResting() || (hasTime && (local.tm_hour >= 22 || local.tm_hour < 7));
  const int wander = hasTime && !sleeping ? ((local.tm_min / 10) % 3 - 1) * 12 : 0;
  const int cx = x + width / 2 + wander + (SHEEP_STATE.getBondPoints() >= 5 ? std::min(24, width / 12) : 0);
  const int bodyW = std::min(150, width / 2);
  const int bodyH = std::min(82, std::max(42, height - 122));
  const int bodyX = cx - bodyW / 2;
  const int bodyY = y + std::max(72, height / 2 - 45);

  drawPasture(x, y, width, height);
  if (SHEEP_STATE.isForaging()) {
    const int signW = std::min(280, width - 30);
    const int signX = cx - signW / 2;
    const int groundY = y + height - 28;
    renderer.fillRect(cx - 3, bodyY + 58, 6, std::max(0, groundY - bodyY - 58), true);
    renderer.drawRoundedRect(signX, bodyY - 6, signW, std::min(78, height - 92), 2, 10, true);
    const char* title = tr(STR_SHEEP_FORAGING);
    const char* hint = tr(STR_SHEEP_RETURN_HINT);
    renderer.drawText(UI_12_FONT_ID, cx - renderer.getTextWidth(UI_12_FONT_ID, title) / 2, bodyY + 4, title);
    const auto shown = renderer.truncatedText(SMALL_FONT_ID, hint, signW - 16);
    renderer.drawText(SMALL_FONT_ID, cx - renderer.getTextWidth(SMALL_FONT_ID, shown.c_str()) / 2, bodyY + 43,
                      shown.c_str());
    for (int i = 0; i < 3; ++i) renderer.fillRect(cx + 60 + i * 19, groundY - 8 - i * 8, 9, 4, true);
  } else {
    renderer.drawRoundedRect(bodyX, bodyY, bodyW, bodyH, 3, 28, true);
    renderer.fillRoundedRect(bodyX + bodyW - 34, bodyY + 22, 46, std::min(48, bodyH - 22), 14, Color::Black);
    renderer.fillRect(bodyX + 24, bodyY + bodyH - 2, 8, 20, true);
    renderer.fillRect(bodyX + bodyW - 42, bodyY + bodyH - 2, 8, 20, true);
    if (sleeping) {
      renderer.drawLine(bodyX + bodyW - 20, bodyY + 39, bodyX + bodyW - 16, bodyY + 39, false);
      renderer.drawLine(bodyX + bodyW - 7, bodyY + 39, bodyX + bodyW - 3, bodyY + 39, false);
    } else {
      renderer.fillRect(bodyX + bodyW - 20, bodyY + 37, 4, 4, false);
      renderer.fillRect(bodyX + bodyW - 7, bodyY + 37, 4, 4, false);
    }

    if (sheepNudge == 1) {
      renderer.drawLine(bodyX + bodyW + 15, bodyY + 10, bodyX + bodyW + 28, bodyY + 2, 2, true);
      renderer.drawLine(bodyX + bodyW + 16, bodyY + 20, bodyX + bodyW + 31, bodyY + 20, 2, true);
    } else if (sheepNudge == 2) {
      renderer.drawLine(bodyX - 10, bodyY + 18, bodyX - 24, bodyY + 8, 2, true);
      renderer.drawLine(bodyX - 9, bodyY + 28, bodyX - 25, bodyY + 30, 2, true);
    }
  }

  // Five heart outlines; fill only the current mood, independent of friendship.
  static constexpr uint16_t heartRows[] = {0x3180, 0x7bc0, 0xffe0, 0xffe0, 0xffe0,
                                           0x7fc0, 0x3f80, 0x1f00, 0x0e00, 0x0400};
  for (int heart = 0; heart < 5; ++heart) {
    for (int row = 0; row < 10; ++row) {
      for (int col = 0; col < 11; ++col) {
        const uint16_t bit = 0x8000 >> col;
        if (!(heartRows[row] & bit)) continue;
        const bool edge = row == 0 || row == 9 || col == 0 || col == 10 || !(heartRows[row] & (bit << 1)) ||
                          !(heartRows[row] & (bit >> 1)) || !(heartRows[row - 1] & bit) || !(heartRows[row + 1] & bit);
        if (heart < SHEEP_STATE.getMood() || edge)
          renderer.fillRect(x + 14 + heart * 25 + col * 2, y + 44 + row * 2, 2, 2, true);
      }
    }
  }
  renderer.drawText(SMALL_FONT_ID, x + 150, y + 42, sleeping ? tr(STR_SHEEP_RESTING) : tr(STR_SHEEP_MOOD));
  char grass[28];
  snprintf(grass, sizeof(grass), tr(STR_SHEEP_GRASS_STOCK), static_cast<unsigned>(SHEEP_STATE.getGrassStock()),
           static_cast<unsigned>(SheepStateStore::GRASS_CAP));
  const int grassW = renderer.getTextWidth(SMALL_FONT_ID, grass);
  const char* label = (name && *name) ? name : tr(STR_HABIT_SHEEP);
  const auto shown = renderer.truncatedText(UI_12_FONT_ID, label, std::max(30, width - grassW - 36));
  renderer.drawText(UI_12_FONT_ID, x + 12, y + 14, shown.c_str());
  renderer.drawText(SMALL_FONT_ID, x + width - 12 - grassW, y + 14, grass);
}

void HabitSheepHomeUi::drawHabitRows(const HabitSheepStore& store, const int top, const int height) const {
  const int screenW = renderer.getScreenWidth();
  const auto& active = store.getActiveHabitIds();

  for (int i = 0; i < 3; ++i) {
    const int y = top + i * height;
    const HabitDefinition* habit = active[i].empty() ? nullptr : store.findHabit(active[i]);
    if (!habit) continue;
    if (selection == 1 + i) renderer.drawRoundedRect(SIDE_PAD, y + 4, screenW - SIDE_PAD * 2, height - 8, 2, 10, true);
    const char* name = habit->name.c_str();
    const auto shown = renderer.truncatedText(NOTOSANS_14_FONT_ID, name, screenW - SIDE_PAD * 2 - 105);
    renderer.drawText(NOTOSANS_14_FONT_ID, SIDE_PAD + 16, y + 10, shown.c_str());

    if (habit->type == HabitType::Completion) {
      const uint16_t count = habit->period == HabitPeriod::Weekly
                                 ? HABIT_EVENTS.completionCountForWeek(habit->id)
                                 : HABIT_EVENTS.progressForToday(habit->id).completionCount;
      char value[24];
      snprintf(value, sizeof(value), "%u/%u %s", static_cast<unsigned>(count),
               static_cast<unsigned>(habit->targetCount),
               habit->period == HabitPeriod::Weekly ? tr(STR_HABIT_WEEK_ABBR) : tr(STR_HABIT_DAY_ABBR));
      const int valueW = renderer.getTextWidth(SMALL_FONT_ID, value);
      renderer.drawText(SMALL_FONT_ID, screenW - SIDE_PAD - 16 - valueW, y + 22, value);
    } else {
      auto progress = HABIT_EVENTS.progressForToday(habit->id);
      if (HABIT_TIMER.isForHabit(habit->id) && HABIT_TIMER.phaseFor(habit->id) == HabitTimer::Phase::Focus)
        progress.durationSeconds += HABIT_TIMER.elapsedSecondsFor(habit->id);
      const uint32_t minutes = progress.durationSeconds / 60;

      char target[24];
      if (habit->type == HabitType::Pomodoro) {
        const auto phase = HABIT_TIMER.phaseFor(habit->id);
        const char phaseLetter = phase == HabitTimer::Phase::Focus ? 'F' : 'B';
        if (HABIT_TIMER.isForHabit(habit->id))
          snprintf(target, sizeof(target), "%u/%u %c%lum", static_cast<unsigned>(progress.pomodoroSessions),
                   static_cast<unsigned>(habit->sessionsPerCycle), phaseLetter,
                   static_cast<unsigned long>(HABIT_TIMER.elapsedSecondsFor(habit->id) / 60));
        else
          snprintf(target, sizeof(target), "%u/%u focus", static_cast<unsigned>(progress.pomodoroSessions),
                   static_cast<unsigned>(habit->sessionsPerCycle));
      } else
        snprintf(target, sizeof(target), "%lu/%um", static_cast<unsigned long>(minutes),
                 static_cast<unsigned>(habit->targetMinutes));
      const int targetW = renderer.getTextWidth(SMALL_FONT_ID, target);
      renderer.drawText(SMALL_FONT_ID, screenW - SIDE_PAD - 16 - targetW, y + 15, target);

      const int barX = screenW - SIDE_PAD - 104;
      const int barY = y + 39;
      const int barW = 86;
      renderer.drawRect(barX, barY, barW, 7, true);
      const uint32_t targetSeconds = habit->type == HabitType::Pomodoro
                                         ? habit->sessionsPerCycle
                                         : static_cast<uint32_t>(habit->targetMinutes) * 60;
      const uint32_t achieved =
          habit->type == HabitType::Pomodoro ? progress.pomodoroSessions : progress.durationSeconds;
      const int fill = targetSeconds == 0
                           ? 0
                           : static_cast<int>(std::min<uint64_t>(
                                 barW - 4, static_cast<uint64_t>(achieved) * (barW - 4) / targetSeconds));
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

void HabitSheepHomeUi::renderUi(const HabitSheepStore& store, const bool showDock, const RecentBook* book) const {
  const int screenW = renderer.getScreenWidth();
  const int screenH = renderer.getScreenHeight();
  const int habitsTop = screenH - DOCK_H - 3 * HABIT_ROW_H;
  const int sheepTop = HEADER_H;
  const int sheepHeight = std::max(0, habitsTop - sheepTop);

  const auto& metrics = UITheme::getInstance().getMetrics();
  char percentageText[8];
  snprintf(percentageText, sizeof(percentageText), "%u%%", static_cast<unsigned>(powerManager.getBatteryPercentage()));
  const int batteryX = screenW - metrics.headerSidePadding - 4 - metrics.batteryWidth - 4 -
                       renderer.getTextWidth(SMALL_FONT_ID, percentageText);
  const int batteryY = metrics.topPadding + (metrics.batteryBarHeight - metrics.batteryHeight) / 2;
  GUI.drawBatteryLeft(renderer, Rect{batteryX, batteryY, metrics.batteryWidth, metrics.batteryHeight}, true);

  struct tm local{};
  if (halClock.isAvailable() && halClock.localTime(local)) {
    char dateText[24];
    strftime(dateText, sizeof(dateText), "%a %d %b", &local);
    renderer.drawText(SMALL_FONT_ID, SIDE_PAD, 18, dateText);
    char clock[10];
    if (halClock.formatTime(clock, sizeof(clock), SETTINGS.clockFormat == 1))
      renderer.drawText(SMALL_FONT_ID, SIDE_PAD + renderer.getTextWidth(SMALL_FONT_ID, dateText) + 18, 18, clock);
  }

  if (!store.isEnabled()) {
    const int coverH = std::min(260, screenH - DOCK_H - HEADER_H - 110);
    const int coverW = coverH * 2 / 3;
    const int coverX = (screenW - coverW) / 2;
    const int coverY = HEADER_H + 35;
    renderer.drawCenteredText(SMALL_FONT_ID, HEADER_H + 4, tr(STR_GRASS_PAUSED));
    bool drawn = false;
    if (book && !book->coverBmpPath.empty()) {
      const auto path = UITheme::getCoverThumbPath(book->coverBmpPath, coverH);
      HalFile file;
      if (Storage.openFileForRead("HOME", path.c_str(), file)) {
        // The bitmap palette exceeds the task stack budget; one temporary object per cover redraw.
        auto bitmap = makeUniqueNoThrow<Bitmap>(file);
        if (!bitmap)
          LOG_ERR("HOME", "OOM: paused book cover");
        else if (bitmap->parseHeaders() == BmpReaderError::Ok)
          drawn = renderer.drawBitmap(*bitmap, coverX, coverY, coverW, coverH);
      }
    }
    if (!drawn) GUI.drawCoverPlaceholder(renderer, Rect{coverX, coverY, coverW, coverH});
    const auto title =
        renderer.truncatedText(UI_12_FONT_ID, book ? book->title.c_str() : tr(STR_CONTINUE_READING), screenW - 48);
    renderer.drawCenteredText(UI_12_FONT_ID, coverY + coverH + 18, title.c_str());
    if (selection == 0)
      renderer.drawRoundedRect(SIDE_PAD, HEADER_H + 2, screenW - 2 * SIDE_PAD, screenH - DOCK_H - HEADER_H - 6, 2, 12,
                               true);
    if (showDock) drawDock(screenH - DOCK_H, DOCK_H);
    return;
  }
  drawSheep(SIDE_PAD, sheepTop + 4, screenW - SIDE_PAD * 2, sheepHeight - 8, store.getSheepName().c_str());
  drawHabitRows(store, habitsTop, HABIT_ROW_H);
  if (showDock) drawDock(screenH - DOCK_H, DOCK_H);
}

void HabitSheepHomeUi::renderSleepUi(const HabitSheepStore& store) const {
  const int screenW = renderer.getScreenWidth();
  const int screenH = renderer.getScreenHeight();
  renderer.clearScreen();

  struct tm local{};
  if (halClock.isAvailable() && halClock.localTime(local)) {
    char dateText[32];
    strftime(dateText, sizeof(dateText), "%A %d %b", &local);
    renderer.drawText(UI_12_FONT_ID, SIDE_PAD, 22, dateText);
  }

  const auto& metrics = UITheme::getInstance().getMetrics();
  GUI.drawBatteryLeft(renderer,
                      Rect{screenW - metrics.headerSidePadding - 4 - metrics.batteryWidth,
                           metrics.topPadding + (metrics.batteryBarHeight - metrics.batteryHeight) / 2,
                           metrics.batteryWidth, metrics.batteryHeight},
                      false);

  const int habitBandH = 150;
  const int sheepTop = 64;
  const int sheepBottom = screenH - habitBandH - 24;
  drawSheep(SIDE_PAD, sheepTop, screenW - SIDE_PAD * 2, sheepBottom - sheepTop, store.getSheepName().c_str(), false);

  const auto& active = store.getActiveHabitIds();
  int row = 0;
  for (int i = 0; i < 3; ++i) {
    if (active[i].empty()) continue;
    const HabitDefinition* habit = store.findHabit(active[i]);
    if (!habit) continue;

    const int y = screenH - habitBandH + row * 42;
    const auto name = renderer.truncatedText(SMALL_FONT_ID, habit->name.c_str(), screenW - SIDE_PAD * 2 - 100);
    renderer.drawText(SMALL_FONT_ID, SIDE_PAD, y, name.c_str());

    const auto progress = HABIT_EVENTS.progressForToday(habit->id);
    char value[24];
    if (habit->type == HabitType::Completion) {
      const uint16_t count = habit->period == HabitPeriod::Weekly ? HABIT_EVENTS.completionCountForWeek(habit->id)
                                                                  : progress.completionCount;
      snprintf(value, sizeof(value), "%u/%u %s", static_cast<unsigned>(count),
               static_cast<unsigned>(habit->targetCount),
               habit->period == HabitPeriod::Weekly ? tr(STR_HABIT_WEEK_ABBR) : tr(STR_HABIT_DAY_ABBR));
    } else if (habit->type == HabitType::Pomodoro) {
      snprintf(value, sizeof(value), "%u/%u focus", static_cast<unsigned>(progress.pomodoroSessions),
               static_cast<unsigned>(habit->sessionsPerCycle));
    } else {
      snprintf(value, sizeof(value), "%lu/%um", static_cast<unsigned long>(progress.durationSeconds / 60),
               static_cast<unsigned>(habit->targetMinutes));
    }
    const int valueW = renderer.getTextWidth(SMALL_FONT_ID, value);
    renderer.drawText(SMALL_FONT_ID, screenW - SIDE_PAD - valueW, y, value);
    ++row;
  }
}
