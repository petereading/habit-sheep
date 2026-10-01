#include "GrassHistoryActivity.h"

#include <GfxRenderer.h>
#include <HalClock.h>
#include <HalDisplay.h>

#include <cstdio>
#include <ctime>

#include "I18n.h"
#include "SheepStateStore.h"
#include "components/UITheme.h"
#include "fontIds.h"

void GrassHistoryActivity::onEnter() {
  Activity::onEnter();
  SHEEP_STATE.settleDay();
}

void GrassHistoryActivity::loop() {
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    finish();
  } else if (mappedInput.wasReleased(MappedInputManager::Button::NavPrevious) && page > 0) {
    --page;
    requestUpdate();
  } else if (mappedInput.wasReleased(MappedInputManager::Button::NavNext) && page < 1) {
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
  renderer.drawCenteredText(NOTOSANS_14_FONT_ID, top, stock);
  renderer.drawCenteredText(SMALL_FONT_ID, top + 34, tr(STR_GRASS_HISTORY_HINT));

  tm today{};
  if (!halClock.isAvailable() || !halClock.localTime(today)) {
    renderer.drawCenteredText(NOTOSANS_14_FONT_ID, top + 65, tr(STR_GRASS_CLOCK_UNAVAILABLE));
  } else {
    const bool compact = renderer.getScreenHeight() <= 600;
    const int rowH = compact ? 40 : 68;
    const int listTop = top + (compact ? 68 : 75);
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
      renderer.drawText(NOTOSANS_14_FONT_ID, left, y, date);
      char amounts[28];
      if (entry.day)
        snprintf(amounts, sizeof(amounts), tr(STR_GRASS_HISTORY_ROW), static_cast<unsigned>(entry.earned),
                 static_cast<unsigned>(entry.eaten));
      else
        snprintf(amounts, sizeof(amounts), "%s", tr(STR_GRASS_NO_ENTRY));
      renderer.drawText(NOTOSANS_14_FONT_ID, right - renderer.getTextWidth(NOTOSANS_14_FONT_ID, amounts), y, amounts);
      if (i < 6) renderer.drawLine(left, y + rowH - 8, right, y + rowH - 8, true);
    }
  }
  const auto hints = mappedInput.mapLabels(tr(STR_BACK), "", tr(STR_GRASS_NEWER), tr(STR_GRASS_OLDER));
  GUI.drawButtonHints(renderer, hints.btn1, hints.btn2, hints.btn3, hints.btn4);
  renderer.displayBuffer(HalDisplay::FAST_REFRESH);
}
