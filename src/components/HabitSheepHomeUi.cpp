#include "HabitSheepHomeUi.h"

#include <Arduino.h>
#include <Bitmap.h>
#include <GfxRenderer.h>
#include <HalClock.h>
#include <HalPowerManager.h>
#include <HalStorage.h>
#include <Memory.h>

#include <algorithm>
#include <cstdio>

#include "CrossPointSettings.h"
#include "HabitSheepStore.h"
#include "MappedInputManager.h"
#include "RecentBooksStore.h"
#include "SheepScene.h"
#include "SheepStateStore.h"
#include "components/HabitUi.h"
#include "components/UITheme.h"
#include "components/icons/blocks.h"
#include "components/icons/book.h"
#include "components/icons/folder.h"
#include "components/icons/library.h"
#include "components/icons/settings2.h"
#include "components/icons/transfer.h"
#include "fontIds.h"

namespace {
constexpr int PAD = 24, HEADER = 52, DOCK = 74;
constexpr int ORDER[] = {1, 2, 3, 10, 0, 4, 5, 6, 7, 8, 9};
struct Layout {
  int habitTop, habitHeight, statusTop, sheepTop, sheepHeight;
};
Layout layout(const GfxRenderer& r) {
  const int band = r.getScreenHeight() <= 600 ? 108 : 156;
  const int status = HEADER + band;
  return {HEADER, band, status, status + 48, r.getScreenHeight() - DOCK - status - 48};
}
const uint8_t* dockIcon(int index) {
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
    default:
      return Settings2Icon;
  }
}
int move(int value, int direction) {
  int pos = 0;
  for (int i = 0; i < 11; ++i)
    if (ORDER[i] == value) pos = i;
  do {
    pos = (pos + direction + 11) % 11;
  } while (!HABIT_SHEEP.isEnabled() && (ORDER[pos] == 10 || (ORDER[pos] >= 1 && ORDER[pos] <= 3)));
  return ORDER[pos];
}
void header(const GfxRenderer& r, bool sleeping) {
  tm local{};
  if (halClock.localTime(local)) {
    char date[24];
    strftime(date, sizeof(date), "%a %d %b", &local);
    r.drawText(SMALL_FONT_ID, PAD, 18, date);
    if (!sleeping) {
      char clock[10];
      if (halClock.formatTime(clock, sizeof(clock), SETTINGS.clockFormat == 1))
        r.drawCenteredText(SMALL_FONT_ID, 18, clock);
    }
  }
  const auto& m = GUI.getMetrics();
  GUI.drawBatteryLeft(r, Rect{r.getScreenWidth() - PAD - m.batteryWidth, 18, m.batteryWidth, m.batteryHeight},
                      !sleeping);
}
}  // namespace

void HabitSheepHomeUi::setSelection(int value) { selection = std::clamp(value, 0, SELECTION_COUNT - 1); }
void HabitSheepHomeUi::nudgeSheep(uint8_t action) {
  sheepNudge = action + 1;
  nudgeStartedMs = millis();
}
bool HabitSheepHomeUi::expireNudge() {
  if (sheepNudge && millis() - nudgeStartedMs >= 10000) {
    sheepNudge = 0;
    return true;
  }
  return false;
}
int HabitSheepHomeUi::nextSelection(int value) { return move(value, 1); }
int HabitSheepHomeUi::previousSelection(int value) { return move(value, -1); }
HabitSheepHomeUi::Action HabitSheepHomeUi::actionForSelection(int value) {
  return value >= 0 && value < SELECTION_COUNT ? static_cast<Action>(value + 1) : Action::None;
}

int HabitSheepHomeUi::selectedAction(MappedInputManager& input) const {
  const auto l = layout(renderer);
  const int w = renderer.getScreenWidth(), h = renderer.getScreenHeight();
  if (HABIT_SHEEP.isEnabled()) {
    if (input.wasTapInRect(PAD, l.habitTop, w - PAD * 2, l.habitHeight - 30)) {
      int x = 0, y = 0;
      if (input.wasScreenTapped(x, y)) return 1 + std::clamp((x - PAD) / ((w - PAD * 2) / 3), 0, 2);
    }
    if (input.wasTapInRect(w / 2, l.statusTop, w / 2 - PAD, 48)) return 10;
    if (input.wasTapInRect(PAD, l.sheepTop, w - PAD * 2, l.sheepHeight)) return 0;
  } else if (input.wasTapInRect(PAD, HEADER, w - PAD * 2, h - DOCK - HEADER))
    return 0;
  int col = -1;
  const auto touch = input.colTouch(col, 0, w / 6, 6, h - DOCK, h, w / 6);
  return touch == MappedInputManager::RowTouch::Tap ? 4 + col : -1;
}

int HabitSheepHomeUi::longPressedHabit(MappedInputManager& input) const {
  if (!HABIT_SHEEP.isEnabled()) return -1;
  const auto l = layout(renderer);
  int x = 0, y = 0;
  if (input.wasScreenLongPress(x, y) && x >= PAD && x < renderer.getScreenWidth() - PAD && y >= l.habitTop &&
      y < l.statusTop)
    return std::clamp((x - PAD) / ((renderer.getScreenWidth() - PAD * 2) / 3), 0, 2);
  if (selection >= 1 && selection <= 3 && input.wasLongPressed(MappedInputManager::Button::Confirm, 700))
    return selection - 1;
  return -1;
}

void HabitSheepHomeUi::drawPasture(int x, int y, int width, int height) const {
  renderer.drawLine(x + 16, y + height - 20, x + width - 16, y + height - 20, 2, true);
}

void HabitSheepHomeUi::drawSheep(int x, int y, int width, int height, const char* name, bool showSelection) const {
  drawPasture(x, y, width, height);
  if (showSelection && selection == 0) habitUi::frame(renderer, x, y, width, height);
  tm local{};
  halClock.localTime(local);
  if (SHEEP_STATE.isForaging()) {
    const int sw = std::min(340, width - 40), sx = x + (width - sw) / 2, sy = y + height / 3;
    renderer.drawLine(x + width / 2, sy + 90, x + width / 2, y + height - 20, 4, true);
    renderer.fillRoundedRect(sx, sy, sw, 90, 10, Color::White);
    habitUi::frame(renderer, sx, sy, sw, 90, false);
    renderer.drawText(NOTOSANS_14_FONT_ID, sx + 12, sy + 10, tr(STR_SHEEP_FORAGING));
    const auto hint = renderer.truncatedText(SMALL_FONT_ID, tr(STR_SHEEP_RETURN_HINT), sw - 24);
    renderer.drawText(SMALL_FONT_ID, sx + 12, sy + 52, hint.c_str());
  } else {
    uint8_t pose = sheepScene::pose(local, !showSelection, SHEEP_STATE.isResting(), SHEEP_STATE.ateCurrentMeal(local));
    if (showSelection && sheepNudge) pose = sheepNudge == 1 ? 2 : 3;
    const int maxW = std::min(width - 36, 380), maxH = std::min(height - 40, maxW * 3 / 4);
    const int bondOffset = showSelection && SHEEP_STATE.getBondPoints() >= 5 ? std::min(16, (width - maxW) / 2) : 0;
    habitUi::sheep(renderer, x + (width - maxW) / 2 + bondOffset, y + height - maxH - 24, maxW, maxH, pose);
  }
  if (name && *name) {
    const auto label = renderer.truncatedText(SMALL_FONT_ID, name, width - 32);
    renderer.drawText(SMALL_FONT_ID, x + 16, y + 8, label.c_str());
  }
}

void HabitSheepHomeUi::drawHabitRows(const HabitSheepStore& store, int top, int height, bool passive) const {
  const int step = (renderer.getScreenWidth() - PAD * 2) / 3;
  const int tile = std::min(passive ? 72 : 90, height - 38);
  for (int i = 0; i < 3; ++i) {
    const auto* habit = store.findHabit(store.getActiveHabitIds()[i]);
    const int x = PAD + i * step + (step - tile) / 2;
    if (habit) {
      if (!passive) habitUi::frame(renderer, x, top + 2, tile, tile, selection == i + 1);
      habitUi::icon(renderer, habitUi::iconFor(*habit), x + (tile - 48) / 2, top + 2 + (tile - 48) / 2, 48);
      if (passive) {
        const auto name = renderer.truncatedText(SMALL_FONT_ID, habit->name.c_str(), step - 12);
        renderer.drawText(SMALL_FONT_ID,
                          PAD + i * step + (step - renderer.getTextWidth(SMALL_FONT_ID, name.c_str())) / 2,
                          top + tile + 6, name.c_str());
        char progress[40];
        habitUi::habitProgress(*habit, progress, sizeof(progress));
        const auto shown = renderer.truncatedText(SMALL_FONT_ID, progress, step - 8);
        renderer.drawText(SMALL_FONT_ID,
                          PAD + i * step + (step - renderer.getTextWidth(SMALL_FONT_ID, shown.c_str())) / 2,
                          top + tile + 32, shown.c_str());
      }
    } else if (!passive) {
      // Erase alternating edge sections while retaining the rounded corners.
      renderer.drawRoundedRect(x, top + 2, tile, tile, 1, 12, true);
      for (int offset = 12; offset < tile - 12; offset += 14) {
        renderer.drawLine(x + offset, top + 2, x + std::min(offset + 7, tile - 12), top + 2, false);
        renderer.drawLine(x + offset, top + tile + 2, x + std::min(offset + 7, tile - 12), top + tile + 2, false);
        renderer.drawLine(x, top + offset + 2, x, top + std::min(offset + 7, tile - 12) + 2, false);
        renderer.drawLine(x + tile, top + offset + 2, x + tile, top + std::min(offset + 7, tile - 12) + 2, false);
      }
      if (selection == i + 1) habitUi::frame(renderer, x - 5, top - 3, tile + 10, tile + 10);
    }
  }
  if (!passive) {
    const int slot = selection >= 1 && selection <= 3 ? selection - 1 : 0;
    const auto* habit = store.findHabit(store.getActiveHabitIds()[slot]);
    char label[100];
    char progress[40];
    if (selection == 10)
      snprintf(label, sizeof(label), "%s", tr(STR_GRASS_HISTORY));
    else if (habit) {
      habitUi::habitProgress(*habit, progress, sizeof(progress));
      snprintf(label, sizeof(label), "%s · %s", habit->name.c_str(), progress);
    } else
      snprintf(label, sizeof(label), "%s", tr(STR_HABIT_CHOOSE));
    const auto shown = renderer.truncatedText(NOTOSANS_14_FONT_ID, label, renderer.getScreenWidth() - PAD * 2);
    renderer.drawCenteredText(NOTOSANS_14_FONT_ID, top + height - 32, shown.c_str());
  }
}

void HabitSheepHomeUi::drawDock(int top, int height) const {
  const int slot = renderer.getScreenWidth() / 6;
  renderer.drawLine(0, top, renderer.getScreenWidth(), top, true);
  for (int i = 0; i < 6; ++i) {
    if (selection == 4 + i) habitUi::frame(renderer, i * slot + 7, top + 8, slot - 14, height - 16);
    renderer.drawIcon(dockIcon(i), i * slot + (slot - 32) / 2, top + (height - 32) / 2, 32);
  }
}

void HabitSheepHomeUi::renderUi(const HabitSheepStore& store, bool showDock, const RecentBook* book) const {
  header(renderer, false);
  const int w = renderer.getScreenWidth(), h = renderer.getScreenHeight();
  if (!store.isEnabled()) {
    const int coverH = std::max(80, std::min(260, h - DOCK - HEADER - 110)), coverW = coverH * 2 / 3;
    const int x = (w - coverW) / 2, y = HEADER + 35;
    renderer.drawCenteredText(SMALL_FONT_ID, HEADER + 4, tr(STR_GRASS_PAUSED));
    bool drawn = false;
    if (book && !book->coverBmpPath.empty()) {
      const auto path = GUI.getCoverThumbPath(book->coverBmpPath, coverH);
      HalFile file;
      if (Storage.openFileForRead("HOME", path.c_str(), file)) {
        auto bitmap = makeUniqueNoThrow<Bitmap>(file);
        if (bitmap && bitmap->parseHeaders() == BmpReaderError::Ok)
          drawn = renderer.drawBitmap(*bitmap, x, y, coverW, coverH);
      }
    }
    if (!drawn) GUI.drawCoverPlaceholder(renderer, Rect{x, y, coverW, coverH});
    const auto title =
        renderer.truncatedText(UI_12_FONT_ID, book ? book->title.c_str() : tr(STR_CONTINUE_READING), w - PAD * 2);
    renderer.drawCenteredText(UI_12_FONT_ID, y + coverH + 18, title.c_str());
    if (selection == 0) habitUi::frame(renderer, PAD, HEADER, w - PAD * 2, h - DOCK - HEADER - 6);
  } else {
    const auto l = layout(renderer);
    drawHabitRows(store, l.habitTop, l.habitHeight);
    habitUi::hearts(renderer, PAD, l.statusTop + 8, std::min(26, (w / 2 - 40) / 5), SHEEP_STATE.getMood());
    if (selection == 10) habitUi::frame(renderer, w / 2, l.statusTop, w / 2 - PAD, 44);
    habitUi::grass(renderer, w / 2 + 12, l.statusTop + 8, 28);
    char stock[24];
    snprintf(stock, sizeof(stock), "%u / %u", SHEEP_STATE.getGrassStock(), SheepStateStore::GRASS_CAP);
    renderer.drawText(NOTOSANS_14_FONT_ID, w / 2 + 50, l.statusTop + 8, stock);
    drawSheep(PAD, l.sheepTop, w - PAD * 2, l.sheepHeight, store.getSheepName().c_str());
  }
  if (showDock) drawDock(h - DOCK, DOCK);
}

void HabitSheepHomeUi::renderSleepUi(const HabitSheepStore& store) const {
  renderer.clearScreen();
  header(renderer, true);
  const int w = renderer.getScreenWidth(), h = renderer.getScreenHeight();
  habitUi::hearts(renderer, PAD, 55, 22, SHEEP_STATE.getMood());
  const int band = 142;
  drawSheep(PAD, 95, w - PAD * 2, h - band - 115, "", false);
  renderer.drawLine(PAD, h - band - 6, w - PAD, h - band - 6, true);
  drawHabitRows(store, h - band, band, true);
}
