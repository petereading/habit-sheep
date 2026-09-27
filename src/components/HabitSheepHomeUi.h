#pragma once

#include <cstdint>

class GfxRenderer;
class HabitSheepStore;
class MappedInputManager;

class HabitSheepHomeUi {
 public:
  enum class Action : uint8_t {
    None = 0,
    Sheep,
    Habit1,
    Habit2,
    Habit3,
    ContinueReading,
    BrowseFiles,
    Library,
    Opds,
    Transfer,
    Settings,
  };

  static constexpr int SELECTION_COUNT = 10;

  explicit HabitSheepHomeUi(GfxRenderer& renderer) : renderer(renderer) {}

  void begin(bool hasContinueReadingValue) { hasContinueReading = hasContinueReadingValue; }
  void setSelection(int value);
  int nextSelection(int value) const;
  int previousSelection(int value) const;
  Action actionForSelection(int value) const;
  int selectedAction(MappedInputManager& input) const;
  int longPressedHabit(MappedInputManager& input) const;
  void renderUi(const HabitSheepStore& store) const;

  void nudgeSheep() { sheepNudge = static_cast<uint8_t>((sheepNudge + 1) % 3); }

 private:
  GfxRenderer& renderer;
  int selection = 0;
  bool hasContinueReading = false;
  uint8_t sheepNudge = 0;

  void drawSheep(int x, int y, int width, int height, const char* name) const;
  void drawHabitRows(const HabitSheepStore& store, int top, int height) const;
  void drawDock(int top, int height) const;
};
