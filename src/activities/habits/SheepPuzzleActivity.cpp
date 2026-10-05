#include "SheepPuzzleActivity.h"

#include <HalDisplay.h>
#include <esp_system.h>

#include "I18n.h"
#include "SheepStateStore.h"
#include "components/HabitUi.h"
#include "components/UITheme.h"
#include "fontIds.h"

void SheepPuzzleActivity::onEnter() {
  habitUi::applyOrientation(renderer);
  puzzle.reset(mode, esp_random());
  Activity::onEnter();
}
void SheepPuzzleActivity::choose() {
  if (puzzle.complete())
    puzzle.reset(mode, esp_random());
  else {
    puzzle.choose(selection);
    if (puzzle.complete()) SHEEP_STATE.recordInteraction();
  }
  requestUpdate();
}
void SheepPuzzleActivity::loop() {
  RenderLock lock;
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    finish();
    return;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::NavPrevious)) {
    selection = (selection + 3) % 4;
    requestUpdate();
  } else if (mappedInput.wasReleased(MappedInputManager::Button::NavNext)) {
    selection = (selection + 1) % 4;
    requestUpdate();
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) choose();
  const Rect safe = UITheme::getInstance().getScreenSafeArea(renderer, true, false);
  const int stepX = (safe.width - 48) / 2, stepY = (safe.height - 170) / 2;
  for (uint8_t i = 0; i < 4; ++i) {
    if (mappedInput.wasTapInRect(safe.x + 24 + (i % 2) * stepX, safe.y + 140 + (i / 2) * stepY, stepX - 8, stepY - 8)) {
      selection = i;
      choose();
      break;
    }
  }
}
void SheepPuzzleActivity::render(RenderLock&&) {
  renderer.clearScreen();
  const Rect safe = UITheme::getInstance().getScreenSafeArea(renderer, true, false);
  const bool order = mode == SheepPuzzle::Mode::Order;
  const bool remember = mode == SheepPuzzle::Mode::Remember;
  habitUi::centeredText(renderer, UI_12_FONT_ID, safe.y + 12,
                        order      ? tr(STR_SHEEP_ORDER)
                        : remember ? tr(STR_SHEEP_REMEMBER)
                                   : tr(STR_SHEEP_DIFFERENT));
  const char* hint = puzzle.complete()     ? tr(STR_SHEEP_GAME_DONE)
                     : puzzle.hasMistake() ? tr(STR_SHEEP_GAME_RETRY)
                     : order               ? tr(STR_SHEEP_ORDER_HELP)
                     : remember ? (puzzle.showing() ? tr(STR_SHEEP_REMEMBER_SHOW) : tr(STR_SHEEP_REMEMBER_FIND))
                                : tr(STR_SHEEP_DIFFERENT_HELP);
  UITheme::drawCenteredWrappedText(renderer, Rect{safe.x + 24, safe.y + 43, safe.width - 48, 42}, SMALL_FONT_ID, hint,
                                   2);
  if (order) {
    for (uint8_t i = 0; i < 4; ++i)
      habitUi::sheep(renderer, safe.x + 24 + i * (safe.width - 48) / 4, safe.y + 88, (safe.width - 48) / 4, 44, 0, i);
  } else if (remember && !puzzle.showing())
    habitUi::sheep(renderer, safe.x + safe.width / 2 - 40, safe.y + 88, 80, 44, 0, puzzle.targetValue());
  const int stepX = (safe.width - 48) / 2, stepY = (safe.height - 170) / 2;
  for (uint8_t i = 0; i < 4; ++i) {
    const int x = safe.x + 24 + (i % 2) * stepX, y = safe.y + 140 + (i / 2) * stepY;
    if (selection == i) habitUi::frame(renderer, x, y, stepX - 8, stepY - 8);
    if (puzzle.picked() == i) habitUi::icon(renderer, 22, x + 8, y + 8, 24);
    if (!remember || puzzle.showing() || puzzle.complete())
      habitUi::sheep(renderer, x + 10, y + 8, stepX - 28, stepY - 28, 0, puzzle.value(i));
    else
      habitUi::icon(renderer, 15, x + (stepX - 56) / 2, y + (stepY - 56) / 2, 48);
  }
  const auto hints = mappedInput.mapLabels(tr(STR_BACK), tr(STR_SELECT), tr(STR_DIR_LEFT), tr(STR_DIR_RIGHT));
  GUI.drawButtonHints(renderer, hints.btn1, hints.btn2, hints.btn3, hints.btn4);
  renderer.displayBuffer(HalDisplay::FAST_REFRESH);
}
