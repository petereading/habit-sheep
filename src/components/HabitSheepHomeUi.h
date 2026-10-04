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
    GrassHistory,
  };

  static constexpr int SELECTION_COUNT = 11;

  explicit HabitSheepHomeUi(GfxRenderer& renderer) : renderer(renderer) {}

  void setSelection(int value);
  static int nextSelection(int value);
  static int previousSelection(int value);
  static Action actionForSelection(int value);
  int selectedAction(MappedInputManager& input) const;
  int longPressedHabit(MappedInputManager& input) const;
  void renderUi(const HabitSheepStore& store, bool showDock = true, const RecentBook* book = nullptr) const;
  void renderSleepUi(const HabitSheepStore& store) const;

  void nudgeSheep(uint8_t action);
  bool expireNudge();

 private:
  GfxRenderer& renderer;
  int selection = 0;
  uint8_t sheepNudge = 0;
  unsigned long nudgeStartedMs = 0;

  void drawSheep(int x, int y, int width, int height, bool showSelection = true) const;
  void drawHabitRows(const HabitSheepStore& store, int top, int height, bool passive = false) const;
  void drawDock(int top, int height) const;
};
