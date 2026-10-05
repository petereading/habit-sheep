#include "SheepMemoryActivity.h"

#include <HalDisplay.h>
#include <esp_system.h>

#include "I18n.h"
#include "SheepStateStore.h"
#include "components/HabitUi.h"
#include "components/UITheme.h"
#include "fontIds.h"

void SheepMemoryActivity::onEnter() {
  habitUi::applyOrientation(renderer);
  game.reset(esp_random() | 1U, 6);
  Activity::onEnter();
}

void SheepMemoryActivity::select() {
  if (game.complete()) {
    game.reset(esp_random() | 1U, 6);
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
    selection = (selection + 5) % 6;
    requestUpdate();
  } else if (mappedInput.wasReleased(MappedInputManager::Button::NavNext)) {
    selection = (selection + 1) % 6;
    requestUpdate();
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) select();
  const Rect safe = UITheme::getInstance().getScreenSafeArea(renderer, true, false);
  const int cardW = (safe.width - 48) / 3;
  const int cardH = (safe.height - 180) / 2;
  for (int i = 0; i < 6; ++i) {
    if (mappedInput.wasTapInRect(safe.x + 24 + (i % 3) * cardW, safe.y + 110 + (i / 3) * cardH, cardW - 8, cardH - 8)) {
      selection = i;
      select();
      break;
    }
  }
}

void SheepMemoryActivity::render(RenderLock&&) {
  renderer.clearScreen();
  const Rect safe = UITheme::getInstance().getScreenSafeArea(renderer, true, false);
  habitUi::centeredText(renderer, UI_12_FONT_ID, safe.y + 20, tr(STR_SHEEP_PAIRS));
  habitUi::centeredText(renderer, SMALL_FONT_ID, safe.y + 65,
                        game.complete()  ? tr(STR_SHEEP_PAIRS_DONE)
                        : game.hasMiss() ? tr(STR_SHEEP_PAIRS_HIDE)
                                         : tr(STR_SHEEP_PAIRS_HELP));
  const int cardW = (safe.width - 48) / 3;
  const int cardH = (safe.height - 180) / 2;
  for (int i = 0; i < 6; ++i) {
    const int x = safe.x + 24 + (i % 3) * cardW;
    const int y = safe.y + 110 + (i / 3) * cardH;
    if (selection == i) habitUi::frame(renderer, x, y, cardW - 8, cardH - 8);
    const int cx = x + (cardW - 8) / 2;
    const int roof = y + 18, doorY = y + 40, doorH = cardH - 54;
    renderer.drawLine(x + 10, roof + 22, cx, roof, 2, true);
    renderer.drawLine(cx, roof, x + cardW - 18, roof + 22, 2, true);
    renderer.drawRect(x + 12, doorY, cardW - 32, doorH, 1, true);
    if (game.shown(i)) {
      habitUi::sheep(renderer, x + 17, doorY + 3, cardW - 42, doorH - 6, 0, game.value(i));
      if (game.isMatched(i)) habitUi::icon(renderer, 22, cx - 10, roof + 2, 20);
    } else {
      renderer.drawLine(cx, doorY + 3, cx, doorY + doorH - 3, true);
      renderer.fillRect(cx - 7, doorY + doorH / 2, 3, 3, true);
      renderer.fillRect(cx + 4, doorY + doorH / 2, 3, 3, true);
    }
  }
  if (game.complete()) {
    const int w = safe.width - 64, y = safe.y + safe.height / 2 - 100;
    renderer.fillRoundedRect(safe.x + 32, y, w, 180, 12, Color::White);
    habitUi::popupFrame(renderer, safe.x + 32, y, w, 180);
    habitUi::sheep(renderer, safe.x + safe.width / 2 - 60, y + 16, 120, 90, 2);
    UITheme::drawCenteredWrappedText(renderer, Rect{safe.x + 48, y + 112, w - 32, 58}, SMALL_FONT_ID,
                                     tr(STR_SHEEP_PAIRS_DONE), 2);
  }
  const auto hints = mappedInput.mapLabels(tr(STR_BACK), tr(STR_SELECT), tr(STR_DIR_LEFT), tr(STR_DIR_RIGHT));
  GUI.drawButtonHints(renderer, hints.btn1, hints.btn2, hints.btn3, hints.btn4);
  renderer.displayBuffer(HalDisplay::FAST_REFRESH);
}
