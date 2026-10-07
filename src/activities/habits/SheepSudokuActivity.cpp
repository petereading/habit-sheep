#include "SheepSudokuActivity.h"

#include <HalDisplay.h>
#include <esp_system.h>

#include <algorithm>

#include "I18n.h"
#include "SheepStateStore.h"
#include "components/HabitUi.h"
#include "components/UITheme.h"
#include "fontIds.h"

namespace {
struct Layout {
  int x, y, size, cx, cy, cw, ch;
  bool landscape;
};
Layout layout(const GfxRenderer& r) {
  const Rect s = UITheme::getInstance().getScreenSafeArea(r, true, false);
  const bool landscape = s.width > s.height;
  const int size = (std::min(s.width - (landscape ? 220 : 48), s.height - (landscape ? 122 : 200)) / 4) * 4;
  return {landscape ? s.x + 24 : s.x + (s.width - size) / 2,
          s.y + 104,
          size,
          landscape ? s.x + size + 48 : s.x + 24,
          landscape ? s.y + 120 : s.y + size + 124,
          landscape ? s.width - size - 72 : s.width - 48,
          landscape ? 52 : 48,
          landscape};
}
Rect control(const Layout& g, int index) {
  if (g.landscape) return {g.cx, g.cy + index * 70, g.cw, g.ch};
  return {g.cx + index * g.cw / 3, g.cy, g.cw / 3 - 8, g.ch};
}
Rect picker(const GfxRenderer& r) {
  const Rect s = UITheme::getInstance().getScreenSafeArea(r, true, false);
  const int w = std::min(s.width - 32, 480);
  return {s.x + (s.width - w) / 2, s.y + (s.height - 184) / 2, w, 184};
}
Rect pickOption(const Rect& p, int index) {
  const int w = (p.width - 24) / 5;
  return {p.x + 12 + index * w, p.y + 58, w - 4, 104};
}
}  // namespace
void SheepSudokuActivity::onEnter() {
  habitUi::applyOrientation(renderer);
  game.reset(esp_random());
  selection = 0;
  if (!selectable(selection)) move(1);
  Activity::onEnter();
}
bool SheepSudokuActivity::selectable(int index) const {
  return index < 16 ? !game.fixed(index) : index == 16 ? game.canUndo() : true;
}
void SheepSudokuActivity::move(int delta) {
  if (editing) {
    choice = (choice + 5 + delta) % 5;
    return;
  }
  for (int n = 0; n < 19; ++n) {
    selection = (selection + 19 + delta) % 19;
    if (selectable(selection)) break;
  }
}
void SheepSudokuActivity::applyChoice() {
  const bool changed = game.set(selection, choice < 4 ? choice + 1 : 0);
  editing = suggested = false;
  if (changed && game.complete()) SHEEP_STATE.recordInteraction();
  requestUpdate();
}
void SheepSudokuActivity::choose() {
  if (editing) {
    applyChoice();
    return;
  }
  if (game.complete() || selection == 18) {
    game.reset(esp_random());
    selection = 0;
    if (!selectable(selection)) move(1);
  } else if (selection == 16)
    game.undo();
  else if (selection == 17) {
    selection = game.hintCell();
    if (selection < 16) {
      choice = game.hintValue(selection) - 1;
      editing = suggested = true;
    }
  } else if (selectable(selection)) {
    choice = game.value(selection) ? game.value(selection) - 1 : 0;
    editing = true;
    suggested = false;
  }
  requestUpdate();
}
void SheepSudokuActivity::loop() {
  RenderLock lock;
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    if (editing) {
      editing = suggested = false;
      requestUpdate();
    } else
      finish();
    return;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::NavPrevious)) {
    move(-1);
    requestUpdate();
  } else if (mappedInput.wasReleased(MappedInputManager::Button::NavNext)) {
    move(1);
    requestUpdate();
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    choose();
    return;
  }
  if (editing) {
    const Rect p = picker(renderer);
    for (int i = 0; i < 5; ++i) {
      const Rect a = pickOption(p, i);
      if (mappedInput.wasTapInRect(a.x, a.y, a.width, a.height)) {
        choice = i;
        applyChoice();
        return;
      }
    }
    return;
  }
  const Rect safe = UITheme::getInstance().getScreenSafeArea(renderer, true, false);
  if (game.complete()) {
    if (mappedInput.wasTapInRect(safe.x, safe.y, safe.width, safe.height)) choose();
    return;
  }
  const auto g = layout(renderer);
  const int tile = g.size / 4;
  for (int i = 0; i < 16; ++i)
    if (!game.fixed(i) && mappedInput.wasTapInRect(g.x + i % 4 * tile, g.y + i / 4 * tile, tile, tile)) {
      selection = i;
      choose();
      return;
    }
  for (int i = 0; i < 3; ++i) {
    const Rect a = control(g, i);
    if (selectable(16 + i) && mappedInput.wasTapInRect(a.x, a.y, a.width, a.height)) {
      selection = 16 + i;
      choose();
      return;
    }
  }
}
void SheepSudokuActivity::render(RenderLock&&) {
  renderer.clearScreen();
  const Rect safe = UITheme::getInstance().getScreenSafeArea(renderer, true, false);
  habitUi::centeredText(renderer, UI_12_FONT_ID, safe.y + 12, tr(STR_SHEEP_DOKU));
  UITheme::drawCenteredWrappedText(renderer, {safe.x + 24, safe.y + 43, safe.width - 48, 48}, SMALL_FONT_ID,
                                   tr(STR_SHEEP_DOKU_HELP), 2);
  const auto g = layout(renderer);
  const int tile = g.size / 4;
  renderer.drawRect(g.x, g.y, g.size + 1, g.size + 1, 3, true);
  for (int i = 1; i < 4; ++i) {
    renderer.drawLine(g.x + i * tile, g.y, g.x + i * tile, g.y + g.size, i == 2 ? 3 : 1, true);
    renderer.drawLine(g.x, g.y + i * tile, g.x + g.size, g.y + i * tile, i == 2 ? 3 : 1, true);
  }
  for (int i = 0; i < 16; ++i) {
    const int x = g.x + i % 4 * tile, y = g.y + i / 4 * tile;
    if (game.value(i)) habitUi::sudokuSheep(renderer, x + 8, y + 8, tile - 16, tile - 16, game.value(i) - 1);
    if (game.fixed(i)) renderer.fillRect(x + 7, y + 7, 4, 4, true);
    if (selection == i) habitUi::frame(renderer, x + 3, y + 3, tile - 6, tile - 6);
  }
  for (int i = 0; i < 3; ++i) {
    const Rect a = control(g, i);
    habitUi::frame(renderer, a.x, a.y, a.width, a.height, selection == 16 + i);
    const char* label = i == 0 ? tr(STR_SHEEP_UNDO) : i == 1 ? tr(STR_SHEEP_HINT) : tr(STR_SHEEP_RESTART);
    const auto text = renderer.truncatedText(SMALL_FONT_ID, label, a.width - 12);
    renderer.drawText(SMALL_FONT_ID, a.x + (a.width - renderer.getTextWidth(SMALL_FONT_ID, text.c_str())) / 2, a.y + 10,
                      text.c_str());
  }
  if (editing) {
    const Rect p = picker(renderer);
    renderer.fillRoundedRect(p.x, p.y, p.width, p.height, 12, Color::White);
    habitUi::popupFrame(renderer, p.x, p.y, p.width, p.height);
    UITheme::drawCenteredWrappedText(renderer, {p.x + 12, p.y + 16, p.width - 24, 30}, SMALL_FONT_ID,
                                     suggested ? tr(STR_SHEEP_DOKU_HINT) : tr(STR_SHEEP_DOKU_CHOOSE), 1);
    for (int i = 0; i < 5; ++i) {
      const Rect a = pickOption(p, i);
      habitUi::frame(renderer, a.x, a.y, a.width, a.height, choice == i);
      if (i < 4)
        habitUi::sudokuSheep(renderer, a.x + 3, a.y + 4, a.width - 6, a.height - 8, i);
      else
        UITheme::drawCenteredWrappedText(renderer, {a.x + 3, a.y + 36, a.width - 6, 48}, SMALL_FONT_ID,
                                         tr(STR_SHEEP_DOKU_CLEAR), 2);
    }
  } else if (game.complete()) {
    const int w = safe.width - 64, y = safe.y + safe.height / 2 - 94;
    renderer.fillRoundedRect(safe.x + 32, y, w, 188, 12, Color::White);
    habitUi::popupFrame(renderer, safe.x + 32, y, w, 188);
    habitUi::facingSheep(renderer, safe.x + safe.width / 2 - 60, y + 12, 120, 90, true);
    UITheme::drawCenteredWrappedText(renderer, {safe.x + 48, y + 110, w - 32, 65}, SMALL_FONT_ID,
                                     tr(STR_SHEEP_GAME_DONE), 2);
  }
  const auto hints = mappedInput.mapLabels(tr(STR_BACK), tr(STR_SELECT), tr(STR_HABIT_PREVIOUS), tr(STR_HABIT_NEXT));
  GUI.drawButtonHints(renderer, hints.btn1, hints.btn2, hints.btn3, hints.btn4);
  renderer.displayBuffer(HalDisplay::FAST_REFRESH);
}
