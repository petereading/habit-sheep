#include "SheepMemoryActivity.h"

#include <HalDisplay.h>
#include <esp_system.h>

#include "I18n.h"
#include "SheepStateStore.h"
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
      // Four sheep silhouettes, identified by their ear marks for monochrome e-ink.
      renderer.drawRoundedRect(cx - 22, cy - 20, 40, 30, 2, 10, true);
      renderer.fillRoundedRect(cx + 10, cy - 12, 18, 22, 6, Color::Black);
      renderer.drawLine(cx - 12, cy + 10, cx - 12, cy + 24, 2, true);
      renderer.drawLine(cx + 8, cy + 10, cx + 8, cy + 24, 2, true);
      char mark[2] = {static_cast<char>('1' + game.value(i)), 0};
      renderer.drawText(SMALL_FONT_ID, cx - 4, cy - 13, mark);
    } else {
      renderer.drawLine(cx, cy - 18, cx, cy + 18, 2, true);
      renderer.drawLine(cx, cy - 4, cx - 12, cy - 15, 2, true);
      renderer.drawLine(cx, cy + 3, cx + 12, cy - 8, 2, true);
    }
  }
  const auto hints = mappedInput.mapLabels(tr(STR_BACK), tr(STR_SELECT), tr(STR_DIR_LEFT), tr(STR_DIR_RIGHT));
  GUI.drawButtonHints(renderer, hints.btn1, hints.btn2, hints.btn3, hints.btn4);
  renderer.displayBuffer(HalDisplay::FAST_REFRESH);
}
