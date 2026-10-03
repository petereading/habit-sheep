#include "HabitIconActivity.h"

#include <HalDisplay.h>

#include <algorithm>

#include "components/HabitUi.h"
#include "components/UITheme.h"
#include "fontIds.h"

void HabitIconActivity::loop() {
  RenderLock lock;
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    setResult(ActivityResult{});
    result.isCancelled = true;
    finish();
    return;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::NavPrevious)) {
    selected = (selected + 23) % 24;
    requestUpdate();
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::NavNext)) {
    selected = (selected + 1) % 24;
    requestUpdate();
  }
  const int w = renderer.getScreenWidth(), h = renderer.getScreenHeight(),
            top = GUI.getMetrics().headerHeight + GUI.getMetrics().topPadding + 20;
  const int rowH = (h - top - 115) / 3, step = (w - 48) / 4;
  for (int i = 0; i < 12; ++i) {
    if (mappedInput.wasTapInRect(24 + (i % 4) * step, top + (i / 4) * rowH, step, rowH)) {
      selected = (selected / 12) * 12 + i;
      setResult(IntervalResult{selected});
      finish();
      return;
    }
  }
  if (mappedInput.wasTapInRect(24, h - 110, 100, 40) || mappedInput.wasTapInRect(w - 124, h - 110, 100, 40)) {
    selected = (selected + 12) % 24;
    requestUpdate();
    return;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    setResult(IntervalResult{selected});
    finish();
  }
}
void HabitIconActivity::render(RenderLock&&) {
  renderer.clearScreen();
  const int w = renderer.getScreenWidth(), h = renderer.getScreenHeight();
  const int top = GUI.getMetrics().headerHeight + GUI.getMetrics().topPadding + 20, step = (w - 48) / 4,
            rowH = (h - top - 115) / 3;
  GUI.drawHeader(renderer, Rect{0, GUI.getMetrics().topPadding, w, GUI.getMetrics().headerHeight}, tr(STR_HABIT_ICON));
  for (int i = 0; i < 12; ++i) {
    const int index = (selected / 12) * 12 + i, tile = std::min(90, std::min(step - 12, rowH - 12));
    const int x = 24 + (i % 4) * step + (step - tile) / 2, y = top + (i / 4) * rowH + 8;
    habitUi::frame(renderer, x, y, tile, tile, index == selected);
    habitUi::icon(renderer, index, x + (tile - 48) / 2, y + (tile - 48) / 2, 48);
  }
  renderer.drawCenteredText(NOTOSANS_14_FONT_ID, h - 110, habitUi::iconName(selected));
  renderer.drawText(SMALL_FONT_ID, 24, h - 110, tr(STR_HABIT_PREVIOUS));
  renderer.drawText(SMALL_FONT_ID, w - 110, h - 110, tr(STR_HABIT_NEXT));
  renderer.drawCenteredText(SMALL_FONT_ID, h - 77, selected < 12 ? tr(STR_HABIT_PAGE_ONE) : tr(STR_HABIT_PAGE_TWO));
  const auto hints = mappedInput.mapLabels(tr(STR_BACK), tr(STR_SELECT), tr(STR_DIR_LEFT), tr(STR_DIR_RIGHT));
  GUI.drawButtonHints(renderer, hints.btn1, hints.btn2, hints.btn3, hints.btn4);
  renderer.displayBuffer(HalDisplay::FAST_REFRESH);
}
