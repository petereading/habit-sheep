#pragma once

#include <cstdint>

class GfxRenderer;
struct HabitDefinition;

namespace habitUi {
void applyOrientation(GfxRenderer& renderer);
void centeredText(const GfxRenderer& renderer, int font, int y, const char* text);
uint8_t iconFor(const HabitDefinition& habit);
const char* iconName(uint8_t icon);
void icon(const GfxRenderer& renderer, uint8_t icon, int x, int y, int size);
void sheep(const GfxRenderer& renderer, int x, int y, int width, int height, uint8_t pose, uint8_t variant = 255,
           bool mirrored = false, uint8_t effectStep = 2);
void grass(const GfxRenderer& renderer, int x, int y, int size);
void grassStock(const GfxRenderer& renderer, int right, int y, int size, uint8_t stock);
void hearts(const GfxRenderer& renderer, int x, int y, int size, uint8_t mood);
void interaction(const GfxRenderer& renderer, int action, int x, int y, int size);
void frame(const GfxRenderer& renderer, int x, int y, int width, int height, bool focused = true);
void popupFrame(const GfxRenderer& renderer, int x, int y, int width, int height);
void number(const GfxRenderer& renderer, int center, int top, uint32_t value, const char* unit, int height = 88);
void progress(const GfxRenderer& renderer, int x, int y, int width, uint32_t value, uint32_t target);
void habitProgress(const HabitDefinition& habit, char* text, unsigned size);
}  // namespace habitUi
