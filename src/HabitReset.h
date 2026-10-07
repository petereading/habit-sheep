#pragma once
// All reset paths belong to Habits; CrossPoint settings and books are excluded.
bool resetHabits();
bool recoverHabitReset();
bool habitResetPending();

class HabitResetConfirmation {
 public:
  void begin() { stage = 1; }
  bool second() const { return stage == 2; }
  bool choose(bool confirmed) {
    if (!confirmed) {
      stage = 0;
      return false;
    }
    if (stage == 1) {
      stage = 2;
      return false;
    }
    if (stage == 2) {
      stage = 0;
      return true;
    }
    return false;
  }

 private:
  unsigned char stage = 0;
};
