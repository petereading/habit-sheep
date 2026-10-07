#pragma once
#include <array>
#include <cstddef>
#include <cstdint>

class SheepSudoku {
 public:
  void reset(uint32_t seed) {
    randomState = seed ? seed : 1;
    std::array<uint8_t, 4> symbols{1, 2, 3, 4}, rows{0, 1, 2, 3}, columns{0, 1, 2, 3};
    shuffle(symbols);
    permuteLines(rows);
    permuteLines(columns);
    const bool transpose = random() & 1U;
    for (uint8_t r = 0; r < 4; ++r)
      for (uint8_t c = 0; c < 4; ++c) {
        const uint8_t a = rows[transpose ? c : r], b = columns[transpose ? r : c];
        answer[r * 4 + c] = symbols[(a * 2 + a / 2 + b) % 4];
      }
    clues = answer;
    std::array<uint8_t, 16> order{};
    for (uint8_t i = 0; i < 16; ++i) order[i] = i;
    shuffle(order);
    uint8_t remaining = 16;
    for (auto at : order) {
      if (remaining <= 6) break;
      const uint8_t old = clues[at];
      clues[at] = 0;
      if (countSolutions(clues) == 1)
        --remaining;
      else
        clues[at] = old;
    }
    board = clues;
    hasUndo = false;
  }
  uint8_t value(uint8_t at) const { return at < 16 ? board[at] : 0; }
  bool fixed(uint8_t at) const { return at < 16 && clues[at]; }
  bool complete() const { return board == answer; }
  bool set(uint8_t at, uint8_t value) {
    if (at >= 16 || value > 4 || fixed(at) || board[at] == value || complete()) return false;
    previous = board;
    hasUndo = true;
    board[at] = value;
    return true;
  }
  bool canUndo() const { return hasUndo && !complete(); }
  void undo() {
    if (!canUndo()) return;
    board = previous;
    hasUndo = false;
  }
  uint8_t hintCell() const {
    for (uint8_t i = 0; i < 16; ++i)
      if (board[i] && board[i] != answer[i]) return i;
    for (uint8_t i = 0; i < 16; ++i)
      if (!board[i]) return i;
    return 255;
  }
  uint8_t hintValue(uint8_t at) const { return at < 16 ? answer[at] : 0; }
  const std::array<uint8_t, 16>& givens() const { return clues; }
  static uint8_t countSolutions(std::array<uint8_t, 16> cells) {
    std::array<uint8_t, 16> empty{};
    uint8_t count = 0, found = 0;
    for (uint8_t i = 0; i < 16; ++i) {
      if (!cells[i])
        empty[count++] = i;
      else if (cells[i] > 4 || !valid(cells, i, cells[i]))
        return 0;
    }
    if (!count) return 1;
    int depth = 0;
    while (depth >= 0) {
      const uint8_t at = empty[depth];
      uint8_t value = cells[at] + 1;
      while (value <= 4 && !valid(cells, at, value)) ++value;
      if (value > 4) {
        cells[at] = 0;
        --depth;
      } else {
        cells[at] = value;
        if (depth + 1 == count) {
          if (++found == 2) return found;
        } else
          cells[empty[++depth]] = 0;
      }
    }
    return found;
  }

 private:
  std::array<uint8_t, 16> answer{}, clues{}, board{}, previous{};
  uint32_t randomState = 1;
  bool hasUndo = false;
  static bool valid(const std::array<uint8_t, 16>& cells, uint8_t at, uint8_t value) {
    const uint8_t r = at / 4, c = at % 4;
    for (uint8_t i = 0; i < 16; ++i)
      if (i != at && cells[i] == value && (i / 4 == r || i % 4 == c || (i / 8 == r / 2 && i % 4 / 2 == c / 2)))
        return false;
    return true;
  }
  uint32_t random() {
    randomState ^= randomState << 13;
    randomState ^= randomState >> 17;
    randomState ^= randomState << 5;
    return randomState;
  }
  template <std::size_t N>
  void shuffle(std::array<uint8_t, N>& values) {
    for (std::size_t i = N - 1; i > 0; --i) {
      const auto j = random() % (i + 1);
      const auto old = values[i];
      values[i] = values[j];
      values[j] = old;
    }
  }
  void permuteLines(std::array<uint8_t, 4>& values) {
    if (random() & 1U) values = {2, 3, 0, 1};
    for (uint8_t i = 0; i < 4; i += 2)
      if (random() & 1U) {
        const auto old = values[i];
        values[i] = values[i + 1];
        values[i + 1] = old;
      }
  }
};
