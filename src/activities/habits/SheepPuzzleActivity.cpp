#include "SheepPuzzleActivity.h"

#include <HalDisplay.h>
#include <esp_system.h>

#include <algorithm>

#include "I18n.h"
#include "SheepStateStore.h"
#include "components/HabitUi.h"
#include "components/UITheme.h"
#include "fontIds.h"

namespace {
struct GameLayout {
  int x, y, size, controlsY, controlsH;
};
GameLayout layout(const GfxRenderer& r, bool maze) {
  const Rect safe = UITheme::getInstance().getScreenSafeArea(r, true, false);
  const int controlsH = maze && r.getScreenHeight() > 600 ? 100 : 50;
  const int size = std::min(safe.width - 48, safe.height - 122 - controlsH);
  return {safe.x + (safe.width - size) / 2, safe.y + 104, size, safe.y + safe.height - controlsH - 8, controlsH};
}
void directionArrow(const GfxRenderer& r, int x, int y, int direction, bool allowed) {
  const int thickness = allowed ? 3 : 1;
  const int dx = direction == 1 ? 1 : direction == 3 ? -1 : 0;
  const int dy = direction == 2 ? 1 : direction == 0 ? -1 : 0;
  r.drawLine(x - dx * 14, y - dy * 14, x + dx * 14, y + dy * 14, thickness, true);
  r.drawLine(x + dx * 14, y + dy * 14, x + dx * 4 + dy * 10, y + dy * 4 - dx * 10, thickness, true);
  r.drawLine(x + dx * 14, y + dy * 14, x + dx * 4 - dy * 10, y + dy * 4 + dx * 10, thickness, true);
  if (!allowed) {
    r.drawLine(x - 4, y + 12, x + 4, y + 20, true);
    r.drawLine(x + 4, y + 12, x - 4, y + 20, true);
  }
}
}  // namespace
void SheepPuzzleActivity::onEnter() {
  habitUi::applyOrientation(renderer);
  puzzle.reset(mode, esp_random());
  selection = 0;
  if (!selectable(selection)) moveSelection(1);
  Activity::onEnter();
}
int SheepPuzzleActivity::optionCount() const {
  return mode == SheepPuzzle::Mode::TurnSheep ? 11 : mode == SheepPuzzle::Mode::Maze ? 5 : 4;
}
bool SheepPuzzleActivity::selectable(int index) const {
  if (mode == SheepPuzzle::Mode::Maze && index < 4) return puzzle.canMove(index);
  if (mode == SheepPuzzle::Mode::TurnSheep && index == 9) return puzzle.canUndo();
  return true;
}
void SheepPuzzleActivity::moveSelection(int delta) {
  for (int n = 0; n < optionCount(); ++n) {
    selection = (selection + optionCount() + delta) % optionCount();
    if (selectable(selection)) break;
  }
}
void SheepPuzzleActivity::choose() {
  if (puzzle.complete() || (mode == SheepPuzzle::Mode::Maze && selection == 4) ||
      (mode == SheepPuzzle::Mode::TurnSheep && selection == 10)) {
    puzzle.reset(mode, esp_random());
    steps = 0;
    selection = 0;
  } else if (mode == SheepPuzzle::Mode::TurnSheep && selection == 9)
    puzzle.undo();
  else if (selectable(selection)) {
    if (mode == SheepPuzzle::Mode::Maze) {
      direction = selection;
      ++steps;
    }
    puzzle.choose(selection);
    if (puzzle.complete()) SHEEP_STATE.recordInteraction();
  }
  if (!selectable(selection)) moveSelection(1);
  requestUpdate();
}
void SheepPuzzleActivity::loop() {
  RenderLock lock;
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    finish();
    return;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::NavPrevious)) {
    moveSelection(-1);
    requestUpdate();
  } else if (mappedInput.wasReleased(MappedInputManager::Button::NavNext)) {
    moveSelection(1);
    requestUpdate();
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    choose();
    return;
  }
  const Rect safe = UITheme::getInstance().getScreenSafeArea(renderer, true, false);
  if (puzzle.complete()) {
    if (mappedInput.wasTapInRect(safe.x, safe.y, safe.width, safe.height)) choose();
    return;
  }
  auto g = layout(renderer, mode == SheepPuzzle::Mode::Maze);
  if (mode == SheepPuzzle::Mode::Remember) {
    g.y += 56;
    g.size -= 56;
  }
  const int columns = mode == SheepPuzzle::Mode::TurnSheep ? 3 : 2, tile = g.size / columns;
  if (mode != SheepPuzzle::Mode::Maze) {
    const int count = mode == SheepPuzzle::Mode::TurnSheep ? 9 : 4;
    for (int i = 0; i < count; ++i)
      if (mappedInput.wasTapInRect(g.x + i % columns * tile, g.y + i / columns * tile, tile, tile)) {
        selection = i;
        choose();
        return;
      }
  }
  if (mode == SheepPuzzle::Mode::TurnSheep) {
    for (int i = 0; i < 2; ++i)
      if (mappedInput.wasTapInRect(safe.x + 24 + i * (safe.width - 48) / 2, g.controlsY, (safe.width - 48) / 2,
                                   g.controlsH)) {
        selection = 9 + i;
        choose();
        return;
      }
  } else if (mode == SheepPuzzle::Mode::Maze && mappedInput.hasTouch()) {
    const int width = (safe.width - 48) / 5;
    for (int i = 0; i < 5; ++i)
      if (mappedInput.wasTapInRect(safe.x + 24 + i * width, g.controlsY, width, g.controlsH)) {
        selection = i;
        choose();
        return;
      }
  }
}
void SheepPuzzleActivity::render(RenderLock&&) {
  renderer.clearScreen();
  const Rect safe = UITheme::getInstance().getScreenSafeArea(renderer, true, false);
  const bool maze = mode == SheepPuzzle::Mode::Maze, turns = mode == SheepPuzzle::Mode::TurnSheep;
  habitUi::centeredText(renderer, UI_12_FONT_ID, safe.y + 12,
                        maze    ? tr(STR_SHEEP_MAZE)
                        : turns ? tr(STR_SHEEP_TURN)
                                : tr(STR_SHEEP_REMEMBER));
  const char* hint = maze                  ? tr(STR_SHEEP_MAZE_HELP)
                     : turns               ? tr(STR_SHEEP_TURN_HELP)
                     : puzzle.hasMistake() ? tr(STR_SHEEP_GAME_RETRY)
                     : puzzle.showing()    ? tr(STR_SHEEP_REMEMBER_SHOW)
                                           : tr(STR_SHEEP_REMEMBER_FIND);
  UITheme::drawCenteredWrappedText(renderer, Rect{safe.x + 24, safe.y + 43, safe.width - 48, 48}, SMALL_FONT_ID, hint,
                                   2);
  auto g = layout(renderer, maze);
  if (mode == SheepPuzzle::Mode::Remember) {
    g.y += 56;
    g.size -= 56;
  }
  if (maze) {
    const int cell = g.size / SheepPuzzle::MAZE_SIZE, size = cell * SheepPuzzle::MAZE_SIZE;
    constexpr int WALL = 5;
    renderer.drawRect(g.x, g.y, size, size, WALL, true);
    for (uint8_t i = 0; i < SheepPuzzle::MAZE_CELLS; ++i) {
      const int x = g.x + i % SheepPuzzle::MAZE_SIZE * cell, y = g.y + i / SheepPuzzle::MAZE_SIZE * cell;
      if (puzzle.mazeWalls(i) & 1) renderer.drawLine(x, y, x + cell, y, WALL, true);
      if (puzzle.mazeWalls(i) & 8) renderer.drawLine(x, y, x, y + cell, WALL, true);
      if (i == SheepPuzzle::MAZE_SIZE - 1) {
        renderer.drawLine(x + 4, y + cell / 3, x + cell / 2, y + 4, 2, true);
        renderer.drawLine(x + cell / 2, y + 4, x + cell - 4, y + cell / 3, 2, true);
        renderer.drawRect(x + 6, y + cell / 3, cell - 12, cell * 2 / 3 - 5, 1, true);
      }
      if (i == puzzle.mazePosition())
        habitUi::sheep(renderer, x + 3, y + 3, cell - 6, cell - 6, steps % 2 ? 4 : 0, 255, direction == 3);
    }
    const int width = (safe.width - 48) / 5;
    for (int i = 0; i < 5; ++i) {
      const int x = safe.x + 24 + i * width;
      if (mappedInput.hasTouch() || i == selection)
        habitUi::frame(renderer, x, g.controlsY, width - 6, g.controlsH - 8, i == selection);
      if (i < 4) {
        directionArrow(renderer, x + (width - 6) / 2, g.controlsY + (g.controlsH - 8) / 2 - 4, i, puzzle.canMove(i));
        continue;
      }
      const char* label = tr(STR_SHEEP_RESTART);
      const auto text = renderer.truncatedText(SMALL_FONT_ID, label, width - 12);
      renderer.drawText(SMALL_FONT_ID, x + (width - 6 - renderer.getTextWidth(SMALL_FONT_ID, text.c_str())) / 2,
                        g.controlsY + (g.controlsH - 8 - renderer.getLineHeight(SMALL_FONT_ID)) / 2, text.c_str());
    }
  } else {
    const int columns = turns ? 3 : 2, tile = g.size / columns, count = turns ? 9 : 4;
    for (int i = 0; i < count; ++i) {
      const int x = g.x + i % columns * tile, y = g.y + i / columns * tile;
      if (selection == i && !mappedInput.hasTouch()) habitUi::frame(renderer, x + 2, y + 2, tile - 4, tile - 4);
      if (turns)
        habitUi::facingSheep(renderer, x + 8, y + 8, tile - 16, tile - 16, !puzzle.backFacing(i));
      else if (puzzle.showing() || puzzle.complete())
        habitUi::sheep(renderer, x + 8, y + 8, tile - 16, tile - 16, 0, puzzle.value(i));
      else
        habitUi::icon(renderer, 15, x + tile / 2 - 20, y + tile / 2 - 20, 40);
    }
    if (!turns && !puzzle.showing())
      habitUi::sheep(renderer, safe.x + safe.width / 2 - 40, safe.y + 104, 80, 48, 0, puzzle.targetValue());
    if (turns) {
      const int width = (safe.width - 48) / 2;
      for (int i = 0; i < 2; ++i) {
        const int x = safe.x + 24 + i * width;
        habitUi::frame(renderer, x, g.controlsY, width - 8, g.controlsH - 8, selection == i + 9);
        const char* label = i ? tr(STR_SHEEP_RESTART) : tr(STR_SHEEP_UNDO);
        renderer.drawText(SMALL_FONT_ID, x + (width - 8 - renderer.getTextWidth(SMALL_FONT_ID, label)) / 2,
                          g.controlsY + 10, label);
      }
    }
  }
  if (puzzle.complete()) {
    const int w = safe.width - 64, y = safe.y + safe.height / 2 - 94;
    renderer.fillRoundedRect(safe.x + 32, y, w, 188, 12, Color::White);
    habitUi::popupFrame(renderer, safe.x + 32, y, w, 188);
    if (turns)
      habitUi::facingSheep(renderer, safe.x + safe.width / 2 - 60, y + 12, 120, 90, true);
    else
      habitUi::sheep(renderer, safe.x + safe.width / 2 - 60, y + 12, 120, 90, 18);
    UITheme::drawCenteredWrappedText(renderer, Rect{safe.x + 48, y + 110, w - 32, 65}, SMALL_FONT_ID,
                                     turns ? tr(STR_SHEEP_TURN_DONE) : tr(STR_SHEEP_GAME_DONE), 2);
  }
  const auto hints = mappedInput.mapLabels(tr(STR_BACK), tr(STR_SELECT), tr(STR_HABIT_PREVIOUS), tr(STR_HABIT_NEXT));
  GUI.drawButtonHints(renderer, hints.btn1, hints.btn2, hints.btn3, hints.btn4);
  renderer.displayBuffer(HalDisplay::FAST_REFRESH);
}
