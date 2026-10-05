#include "GrassHistoryActivity.h"

#include <GfxRenderer.h>
#include <HalClock.h>
#include <HalDisplay.h>

#include <cstdio>
#include <ctime>

#include "I18n.h"
#include "SheepStateStore.h"
#include "components/HabitUi.h"
#include "components/UITheme.h"
#include "fontIds.h"

void GrassHistoryActivity::onEnter() {
  habitUi::applyOrientation(renderer);
  Activity::onEnter();
  SHEEP_STATE.settleDay();
}

void GrassHistoryActivity::loop() {
  RenderLock lock;
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    finish();
  } else if (mappedInput.wasReleased(MappedInputManager::Button::NavPrevious) && page > 0) {
    --page;
    requestUpdate();
  } else if (mappedInput.wasReleased(MappedInputManager::Button::NavNext) && page < 1) {
    ++page;
    requestUpdate();
  }
  const Rect safe = UITheme::getInstance().getScreenSafeArea(renderer, true, false);
  const int bottom = safe.y + safe.height;
  if (mappedInput.wasTapInRect(safe.x + 24, bottom - 55, 140, 40) && page > 0) {
    --page;
    requestUpdate();
  }
  if (mappedInput.wasTapInRect(safe.x + safe.width - 164, bottom - 55, 140, 40) && page < 1) {
    ++page;
    requestUpdate();
  }
}

void GrassHistoryActivity::render(RenderLock&&) {
  renderer.clearScreen();
  const auto& metrics = UITheme::getInstance().getMetrics();
  const Rect safe = UITheme::getInstance().getScreenSafeArea(renderer, true, false);
  const int left = safe.x + 25;
  const int right = safe.x + safe.width - 25;
  GUI.drawHeader(renderer, Rect{safe.x, safe.y + metrics.topPadding, safe.width, metrics.headerHeight},
                 tr(STR_GRASS_HISTORY));

  char stock[32];
  snprintf(stock, sizeof(stock), tr(STR_SHEEP_GRASS_STOCK), static_cast<unsigned>(SHEEP_STATE.getGrassStock()),
           static_cast<unsigned>(SheepStateStore::GRASS_CAP));
  const int top = safe.y + metrics.topPadding + metrics.headerHeight + 15;
  habitUi::grass(renderer, left, top, 32);
  habitUi::centeredText(renderer, NOTOSANS_14_FONT_ID, top, stock);
  habitUi::centeredText(renderer, SMALL_FONT_ID, top + 30, tr(STR_GRASS_HISTORY_HINT));
  habitUi::centeredText(renderer, SMALL_FONT_ID, top + 50, tr(STR_GRASS_MEALS));

  tm today{};
  if (!halClock.isAvailable() || !halClock.localTime(today)) {
    renderer.drawCenteredText(NOTOSANS_14_FONT_ID, top + 85, tr(STR_GRASS_CLOCK_UNAVAILABLE));
  } else {
    const bool compact = renderer.getScreenHeight() <= 600;
    const int listTop = top + (compact ? 78 : 85);
    const int rowH = std::min(compact ? 38 : 68, (safe.y + safe.height - 55 - listTop - 12) / 7);
    const int font = compact ? SMALL_FONT_ID : NOTOSANS_14_FONT_ID;
    for (int i = 0; i < 7; ++i) {
      tm day = today;
      day.tm_mday -= page * 7 + i;
      day.tm_hour = 12;
      day.tm_isdst = -1;
      mktime(&day);
      const uint32_t key = static_cast<uint32_t>(day.tm_year + 1900) * 10000 +
                           static_cast<uint32_t>(day.tm_mon + 1) * 100 + static_cast<uint32_t>(day.tm_mday);
      const auto entry = SHEEP_STATE.grassForDay(key);
      char date[18];
      strftime(date, sizeof(date), "%a %d %b", &day);
      const int y = listTop + i * rowH;
      renderer.drawText(font, left, y, date);
      char amounts[28];
      if (entry.day)
        snprintf(amounts, sizeof(amounts), tr(STR_GRASS_HISTORY_ROW), static_cast<unsigned>(entry.earned),
                 static_cast<unsigned>(entry.eaten));
      else
        snprintf(amounts, sizeof(amounts), "%s", tr(STR_GRASS_NO_ENTRY));
      if (entry.paused) renderer.drawText(SMALL_FONT_ID, safe.x + safe.width / 2 - 30, y + 2, tr(STR_GRASS_PAUSED));
      renderer.drawText(font, right - renderer.getTextWidth(font, amounts), y, amounts);
      if (i < 6) renderer.drawLine(left, y + rowH - 2, right, y + rowH - 2, true);
    }
  }
  const int bottom = safe.y + safe.height;
  if (mappedInput.hasTouch()) {
    habitUi::frame(renderer, safe.x + 24, bottom - 55, 140, 38, false);
    habitUi::frame(renderer, safe.x + safe.width - 164, bottom - 55, 140, 38, false);
    renderer.drawText(SMALL_FONT_ID, safe.x + 35, bottom - 47, tr(STR_GRASS_NEWER));
    renderer.drawText(SMALL_FONT_ID, safe.x + safe.width - 153, bottom - 47, tr(STR_GRASS_OLDER));
  }
  const auto hints = mappedInput.mapLabels(tr(STR_BACK), "", tr(STR_GRASS_NEWER), tr(STR_GRASS_OLDER));
  GUI.drawButtonHints(renderer, hints.btn1, hints.btn2, hints.btn3, hints.btn4);
  renderer.displayBuffer(HalDisplay::FAST_REFRESH);
}
