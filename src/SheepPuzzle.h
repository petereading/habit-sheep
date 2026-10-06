#pragma once

#include <array>
#include <cstdint>

class SheepPuzzle {
 public:
  enum class Mode : uint8_t { LightsOut, Remember, Maze };
  static constexpr uint8_t MAZE_SIZE = 7, MAZE_CELLS = MAZE_SIZE * MAZE_SIZE;

  void reset(Mode value, uint32_t seed) {
    mode = value;
    randomState = seed ? seed : 1;
    done = wrong = preview = hasUndo = false;
    if (mode == Mode::Maze)
      generateMaze();
    else if (mode == Mode::LightsOut) {
      lights = solution = 0;
      for (uint8_t i = 0; i < 9; ++i) {
        if (random() & 1U) {
          lights ^= lightMask(i);
          solution ^= 1U << i;
        }
      }
      if (!lights) {
        lights ^= lightMask(4);
        solution ^= 1U << 4;
      }
    } else {
      for (uint8_t i = 0; i < 4; ++i) cells[i] = i;
      for (uint8_t i = 3; i > 0; --i) {
        const uint8_t j = random() % (i + 1), v = cells[i];
        cells[i] = cells[j];
        cells[j] = v;
      }
      answer = random() % 4;
      target = cells[answer];
      preview = true;
    }
  }
  void choose(uint8_t index) {
    if (done) return;
    if (mode == Mode::LightsOut && index < 9) {
      previousLights = lights;
      hasUndo = true;
      lights ^= lightMask(index);
      done = lights == 0;
    } else if (mode == Mode::Remember && index < 4) {
      if (preview)
        preview = false;
      else if (wrong)
        wrong = false;
      else {
        done = index == answer;
        wrong = !done;
      }
    } else if (mode == Mode::Maze && canMove(index)) {
      position = neighbor(position, index);
      done = position == MAZE_SIZE - 1;
    }
  }
  void undo() {
    if (mode != Mode::LightsOut || !hasUndo || done) return;
    lights = previousLights;
    hasUndo = false;
  }
  bool canUndo() const { return hasUndo; }
  bool lit(uint8_t index) const { return index < 9 && (lights & (1U << index)); }
  uint16_t lightState() const { return lights; }
  uint16_t solvingPresses() const { return solution; }
  static uint16_t lightMask(uint8_t index) {
    if (index >= 9) return 0;
    uint16_t mask = 1U << index;
    if (index >= 3) mask |= 1U << (index - 3);
    if (index < 6) mask |= 1U << (index + 3);
    if (index % 3) mask |= 1U << (index - 1);
    if (index % 3 < 2) mask |= 1U << (index + 1);
    return mask;
  }
  bool canMove(uint8_t direction) const { return direction < 4 && !(walls[position] & (1U << direction)); }
  uint8_t mazeWalls(uint8_t index) const { return index < MAZE_CELLS ? walls[index] : 15; }
  uint8_t mazePosition() const { return position; }
  static uint8_t neighbor(uint8_t index, uint8_t direction) {
    return direction == 0   ? index - MAZE_SIZE
           : direction == 1 ? index + 1
           : direction == 2 ? index + MAZE_SIZE
                            : index - 1;
  }
  uint8_t value(uint8_t index) const { return index < 4 ? cells[index] : 0; }
  uint8_t targetValue() const { return target; }
  bool showing() const { return preview; }
  bool complete() const { return done; }
  bool hasMistake() const { return wrong; }

 private:
  std::array<uint8_t, 4> cells{};
  std::array<uint8_t, MAZE_CELLS> walls{};
  Mode mode = Mode::LightsOut;
  uint32_t randomState = 1;
  uint16_t lights = 0, previousLights = 0, solution = 0;
  uint8_t answer = 0, target = 0, position = 0;
  bool preview = false, done = false, wrong = false, hasUndo = false;
  uint32_t random() {
    randomState ^= randomState << 13;
    randomState ^= randomState >> 17;
    randomState ^= randomState << 5;
    return randomState;
  }
  void generateMaze() {
    // Bounded traversal stack avoids recursion and dynamic allocation.
    uint8_t stack[MAZE_CELLS];
    walls.fill(15);
    uint8_t depth = 1;
    stack[0] = 0;
    walls[0] |= 16;
    while (depth) {
      const uint8_t at = stack[depth - 1];
      uint8_t choices[4], count = 0;
      for (uint8_t direction = 0; direction < 4; ++direction) {
        if ((direction == 0 && at < MAZE_SIZE) || (direction == 1 && at % MAZE_SIZE == MAZE_SIZE - 1) ||
            (direction == 2 && at >= MAZE_CELLS - MAZE_SIZE) || (direction == 3 && at % MAZE_SIZE == 0))
          continue;
        if (!(walls[neighbor(at, direction)] & 16)) choices[count++] = direction;
      }
      if (!count) {
        --depth;
        continue;
      }
      const uint8_t direction = choices[random() % count], next = neighbor(at, direction);
      walls[at] &= ~(1U << direction);
      walls[next] &= ~(1U << ((direction + 2) % 4));
      walls[next] |= 16;
      stack[depth++] = next;
    }
    for (auto& cell : walls) cell &= 15;
    position = MAZE_CELLS - MAZE_SIZE;
  }
};
