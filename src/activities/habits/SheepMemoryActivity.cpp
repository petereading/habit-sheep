#include "SheepMemoryActivity.h"

#include <HalDisplay.h>
#include <esp_system.h>

#include "I18n.h"
#include "SheepStateStore.h"
#include "components/HabitUi.h"
#include "components/UITheme.h"
#include "fontIds.h"

void SheepMemoryActivity::onEnter() {
  Activity::onEnter();
  game.reset(esp_random() | 1U);
}

void SheepMemoryActivity::select() {
  if (game.complete()) {
    game.reset(esp_random() | 1U);
  } else if (game.hasMiss()) {
    game.hideMiss();
  } else if (game.reveal(selection) == SheepMemoryGame::Result::Finished) {
    SHEEP_STATE.recordInteraction();
  }
  requestUpdate();
}

void SheepMemoryActivity::loop() {
  RenderLock lock;
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    finish();
    return;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::NavPrevious)) {
    selection = (selection + 7) % 8;
    requestUpdate();
  } else if (mappedInput.wasReleased(MappedInputManager::Button::NavNext)) {
    selection = (selection + 1) % 8;
    requestUpdate();
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) select();
  const int cardW = (renderer.getScreenWidth() - 48) / 4;
  const int cardH = (renderer.getScreenHeight() - 220) / 2;
  for (int i = 0; i < 8; ++i) {
    if (mappedInput.wasTapInRect(24 + (i % 4) * cardW, 110 + (i / 4) * cardH, cardW - 8, cardH - 8)) {
      selection = i;
      select();
      break;
    }
  }
}

void SheepMemoryActivity::render(RenderLock&&) {
  renderer.clearScreen();
  renderer.drawCenteredText(UI_12_FONT_ID, 20, tr(STR_SHEEP_MEMORY));
  renderer.drawCenteredText(SMALL_FONT_ID, 65,
                            game.complete()  ? tr(STR_SHEEP_MEMORY_DONE)
                            : game.hasMiss() ? tr(STR_SHEEP_MEMORY_HIDE)
                                             : tr(STR_SHEEP_MEMORY_HELP));
  const int cardW = (renderer.getScreenWidth() - 48) / 4;
  const int cardH = (renderer.getScreenHeight() - 220) / 2;
  for (int i = 0; i < 8; ++i) {
    const int x = 24 + (i % 4) * cardW;
    const int y = 110 + (i / 4) * cardH;
    renderer.drawRoundedRect(x, y, cardW - 8, cardH - 8, selection == i ? 3 : 1, 8, true);
    const int cx = x + (cardW - 8) / 2;
    const int cy = y + (cardH - 8) / 2;
    if (game.shown(i)) {
      habitUi::sheep(renderer, x + 4, y + 4, cardW - 16, cardH - 16, 0, game.value(i));
    } else {
      habitUi::icon(renderer, 15, cx - 24, cy - 24, 48);
    }
  }
  if (game.complete()) {
    const int w = renderer.getScreenWidth() - 64, y = renderer.getScreenHeight() / 2 - 100;
    renderer.fillRoundedRect(32, y, w, 180, 12, Color::White);
    habitUi::popupFrame(renderer, 32, y, w, 180);
    habitUi::sheep(renderer, renderer.getScreenWidth() / 2 - 60, y + 16, 120, 90, 2);
    UITheme::drawCenteredWrappedText(renderer, Rect{48, y + 112, w - 32, 58}, SMALL_FONT_ID, tr(STR_SHEEP_MEMORY_DONE),
                                     2);
  }
  const auto hints = mappedInput.mapLabels(tr(STR_BACK), tr(STR_SELECT), tr(STR_DIR_LEFT), tr(STR_DIR_RIGHT));
  GUI.drawButtonHints(renderer, hints.btn1, hints.btn2, hints.btn3, hints.btn4);
  renderer.displayBuffer(HalDisplay::FAST_REFRESH);
}
