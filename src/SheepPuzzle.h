#pragma once

#include <array>
#include <cstdint>

class SheepPuzzle {
 public:
  enum class Mode : uint8_t { Different, Remember, Order };
  void reset(Mode value, uint32_t seed) {
    mode = value;
    seed = seed ? seed : 1;
    for (uint8_t i = 0; i < 4; ++i) cells[i] = i;
    for (uint8_t i = 3; i > 0; --i) {
      seed ^= seed << 13;
      seed ^= seed >> 17;
      seed ^= seed << 5;
      const uint8_t j = seed % (i + 1);
      const uint8_t v = cells[i];
      cells[i] = cells[j];
      cells[j] = v;
    }
    answer = seed % 4;
    target = cells[answer];
    if (mode == Mode::Different) {
      const uint8_t common = (target + 1) % 4;
      cells.fill(common);
      cells[answer] = target;
    }
    if (mode == Mode::Order && ordered()) {
      cells[0] = 1;
      cells[1] = 0;
    }
    preview = mode == Mode::Remember;
    done = wrong = false;
    first = 4;
  }
  void choose(uint8_t index) {
    if (index >= 4 || done) return;
    if (preview) {
      preview = false;
      return;
    }
    if (wrong) {
      wrong = false;
      return;
    }
    if (mode != Mode::Order) {
      done = index == answer;
      wrong = !done;
    } else if (first == 4) {
      first = index;
    } else {
      const uint8_t value = cells[first];
      cells[first] = cells[index];
      cells[index] = value;
      first = 4;
      done = ordered();
    }
  }
  bool ordered() const { return cells[0] == 0 && cells[1] == 1 && cells[2] == 2 && cells[3] == 3; }
  uint8_t value(uint8_t index) const { return index < 4 ? cells[index] : 0; }
  uint8_t targetValue() const { return target; }
  uint8_t picked() const { return first; }
  bool showing() const { return preview; }
  bool complete() const { return done; }
  bool hasMistake() const { return wrong; }

 private:
  std::array<uint8_t, 4> cells{};
  Mode mode = Mode::Different;
  uint8_t answer = 0, target = 0, first = 4;
  bool preview = false, done = false, wrong = false;
};
