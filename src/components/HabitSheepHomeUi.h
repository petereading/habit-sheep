#pragma once

#include <cstdint>

class GfxRenderer;
class HabitSheepStore;
class MappedInputManager;
struct RecentBook;

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

  void setSelection(int value);
  static int nextSelection(int value);
  static int previousSelection(int value);
  static Action actionForSelection(int value);
  int selectedAction(MappedInputManager& input) const;
  int longPressedHabit(MappedInputManager& input) const;
  void renderUi(const HabitSheepStore& store, bool showDock = true, const RecentBook* book = nullptr) const;
  void renderSleepUi(const HabitSheepStore& store) const;

  void nudgeSheep() { sheepNudge = static_cast<uint8_t>((sheepNudge + 1) % 3); }

 private:
  GfxRenderer& renderer;
  int selection = 0;
  uint8_t sheepNudge = 0;

  void drawSheep(int x, int y, int width, int height, const char* name, bool showSelection = true) const;
  void drawPasture(int x, int y, int width, int height) const;
  void drawHabitRows(const HabitSheepStore& store, int top, int height) const;
  void drawDock(int top, int height) const;
};
