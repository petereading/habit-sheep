#include "HabitUi.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include <algorithm>
#include <cstdio>
#include <cstring>

#include "HabitEventLog.h"
#include "HabitTimer.h"
#include "fontIds.h"
#include "icons/habitArt.generated.h"

namespace {
using ArtBitmap = habitArt::Bitmap;
constexpr ArtBitmap ICONS[] = {
    habitArt::habit_reading, habitArt::habit_focus,        habitArt::habit_writing,  habitArt::habit_study,
    habitArt::habit_walking, habitArt::habit_running,      habitArt::habit_strength, habitArt::habit_water,
    habitArt::habit_rest,    habitArt::habit_meditation,   habitArt::habit_food,     habitArt::habit_cleaning,
    habitArt::habit_music,   habitArt::habit_painting,     habitArt::habit_plant,    habitArt::habit_general,
    habitArt::habit_family,  habitArt::habit_relationship, habitArt::habit_money,    habitArt::habit_phone,
    habitArt::habit_flag,    habitArt::habit_target,       habitArt::habit_check,    habitArt::habit_sun};
constexpr ArtBitmap SHEEP[] = {habitArt::sheep_00, habitArt::sheep_01, habitArt::sheep_02, habitArt::sheep_03,
                               habitArt::sheep_04, habitArt::sheep_05, habitArt::sheep_06, habitArt::sheep_07,
                               habitArt::sheep_08, habitArt::sheep_09, habitArt::sheep_10, habitArt::sheep_11,
                               habitArt::sheep_12, habitArt::sheep_13, habitArt::sheep_14, habitArt::sheep_15,
                               habitArt::sheep_16, habitArt::sheep_17};
constexpr ArtBitmap PAIRS[] = {habitArt::pair_00, habitArt::pair_01, habitArt::pair_02, habitArt::pair_03};
constexpr ArtBitmap GRASS_STOCK[] = {habitArt::grass_stock_0, habitArt::grass_stock_1, habitArt::grass_stock_2,
                                     habitArt::grass_stock_3};
constexpr StrId NAMES[] = {
    StrId::STR_ICON_READING, StrId::STR_ICON_FOCUS,        StrId::STR_ICON_WRITING,  StrId::STR_ICON_STUDY,
    StrId::STR_ICON_WALKING, StrId::STR_ICON_RUNNING,      StrId::STR_ICON_STRENGTH, StrId::STR_ICON_WATER,
    StrId::STR_ICON_REST,    StrId::STR_ICON_MEDITATION,   StrId::STR_ICON_FOOD,     StrId::STR_ICON_CLEANING,
    StrId::STR_ICON_MUSIC,   StrId::STR_ICON_PAINTING,     StrId::STR_ICON_PLANT,    StrId::STR_ICON_STAR,
    StrId::STR_ICON_FAMILY,  StrId::STR_ICON_RELATIONSHIP, StrId::STR_ICON_MONEY,    StrId::STR_ICON_PHONE,
    StrId::STR_ICON_FLAG,    StrId::STR_ICON_TARGET,       StrId::STR_ICON_CHECK,    StrId::STR_ICON_SUN};

void ink(const GfxRenderer& renderer, const ArtBitmap& bitmap, int x, int y, int width, int height) {
  if (width <= 0 || height <= 0) return;
  const int stride = (bitmap.width + 7) / 8;
  for (int row = 0; row < height; ++row) {
    const int sourceY = row * bitmap.height / height;
    int run = -1;
    for (int col = 0; col <= width; ++col) {
      const int sourceX = col * bitmap.width / width;
      const bool black = col < width && (bitmap.data[sourceY * stride + sourceX / 8] & (0x80 >> (sourceX % 8)));
      if (black && run < 0) run = col;
      if (!black && run >= 0) {
        renderer.drawLine(x + run, y + row, x + col - 1, y + row, true);
        run = -1;
      }
    }
  }
}
}  // namespace

namespace habitUi {
uint8_t iconFor(const HabitDefinition& habit) {
  return habit.icon < 24 ? habit.icon : habit.type == HabitType::Pomodoro ? 1 : habit.readingIntegration ? 0 : 15;
}
const char* iconName(uint8_t value) { return I18N.get(NAMES[value < 24 ? value : 15]); }
void icon(const GfxRenderer& r, uint8_t value, int x, int y, int size) {
  ink(r, ICONS[value < 24 ? value : 15], x, y, size, size);
}
void sheep(const GfxRenderer& r, int x, int y, int width, int height, uint8_t pose, uint8_t variant) {
  const int w = std::min(width, height * 4 / 3), h = w * 3 / 4;
  ink(r, variant < 4 ? PAIRS[variant] : SHEEP[pose % 18], x + (width - w) / 2, y + (height - h) / 2, w, h);
}
void grass(const GfxRenderer& r, int x, int y, int size) { ink(r, habitArt::grass, x, y, size, size); }
void grassStock(const GfxRenderer& r, int right, int y, int size, uint8_t stock) {
  constexpr int COUNT = 7, GAP = 4;
  const int left = right - COUNT * size - (COUNT - 1) * GAP;
  for (int i = 0; i < COUNT; ++i) {
    const int blades = std::clamp(static_cast<int>(stock) - i * 3, 0, 3);
    ink(r, GRASS_STOCK[blades], left + i * (size + GAP), y, size, size);
  }
}
void hearts(const GfxRenderer& r, int x, int y, int size, uint8_t mood) {
  for (int i = 0; i < 5; ++i)
    ink(r, i < mood ? habitArt::heart : habitArt::heart_empty, x + i * (size + 6), y, size, size);
}
void interaction(const GfxRenderer& r, int action, int x, int y, int size) {
  const ArtBitmap& b = action == 0 ? habitArt::pet : action == 1 ? habitArt::call : habitArt::play;
  ink(r, b, x, y, size, size);
}
void frame(const GfxRenderer& r, int x, int y, int w, int h, bool focused) {
  r.drawRoundedRect(x, y, w, h, focused ? 2 : 1, 12, true);
}
void popupFrame(const GfxRenderer& r, int x, int y, int w, int h) {
  r.drawRoundedRect(x, y, w, h, 2, 12, true);
  r.drawRoundedRect(x + 5, y + 5, w - 10, h - 10, 1, 8, true);
}
void number(const GfxRenderer& r, int center, int top, uint32_t value, const char* unit, int height) {
  static constexpr uint8_t DIGITS[] = {0x3f, 0x06, 0x5b, 0x4f, 0x66, 0x6d, 0x7d, 0x07, 0x7f, 0x6f};
  char text[8];
  snprintf(text, sizeof(text), "%lu", static_cast<unsigned long>(std::min<uint32_t>(9999, value)));
  const int width = height / 2, thick = std::max(3, height / 10), count = static_cast<int>(strlen(text));
  const int start = center - (count * (width + 8) + r.getTextWidth(NOTOSANS_14_FONT_ID, unit) + 10) / 2;
  for (int i = 0; i < count; ++i) {
    const int x = start + i * (width + 8), half = height / 2;
    const uint8_t mask = DIGITS[text[i] - '0'];
    if (mask & 1) r.fillRect(x + thick, top, width - thick * 2, thick);
    if (mask & 2) r.fillRect(x + width - thick, top + thick, thick, half - thick);
    if (mask & 4) r.fillRect(x + width - thick, top + half, thick, half - thick);
    if (mask & 8) r.fillRect(x + thick, top + height - thick, width - thick * 2, thick);
    if (mask & 16) r.fillRect(x, top + half, thick, half - thick);
    if (mask & 32) r.fillRect(x, top + thick, thick, half - thick);
    if (mask & 64) r.fillRect(x + thick, top + half - thick / 2, width - thick * 2, thick);
  }
  r.drawText(NOTOSANS_14_FONT_ID, start + count * (width + 8) + 10, top + height - r.getLineHeight(NOTOSANS_14_FONT_ID),
             unit);
}
void progress(const GfxRenderer& r, int x, int y, int w, uint32_t value, uint32_t target) {
  r.drawRoundedRect(x, y, w, 14, 2, 6, true);
  const int fill =
      target ? static_cast<int>(std::min<uint64_t>(w - 6, static_cast<uint64_t>(value) * (w - 6) / target)) : 0;
  if (fill) r.fillRoundedRect(x + 3, y + 3, fill, 8, 3, Color::Black);
}
void habitProgress(const HabitDefinition& habit, char* text, unsigned size) {
  auto p = HABIT_EVENTS.progressForToday(habit.id);
  if (habit.type == HabitType::Completion) {
    const unsigned count =
        habit.period == HabitPeriod::Weekly ? HABIT_EVENTS.completionCountForWeek(habit.id) : p.completionCount;
    snprintf(text, size, "%u / %u %s", count, habit.targetCount, tr(STR_HABIT_TIMES));
  } else if (habit.type == HabitType::Pomodoro) {
    snprintf(text, size, "%u / %u %s", p.pomodoroSessions, habit.sessionsPerCycle, tr(STR_HABIT_SESSIONS));
  } else {
    if (HABIT_TIMER.isForHabit(habit.id)) p.durationSeconds += HABIT_TIMER.elapsedSecondsFor(habit.id);
    snprintf(text, size, "%lu / %u %s", static_cast<unsigned long>(p.durationSeconds / 60), habit.targetMinutes,
             tr(STR_HABIT_MINUTES_ABBR));
  }
}
}  // namespace habitUi
