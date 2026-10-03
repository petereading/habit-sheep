#pragma once

#include <array>
#include <cstdint>

class SheepMemoryGame {
 public:
  enum class Result : uint8_t { Ignored, Revealed, Match, Miss, Finished };
  void reset(uint32_t seed) {
    for (uint8_t i = 0; i < 8; ++i) cards[i] = i / 2;
    for (uint8_t i = 7; i > 0; --i) {
      seed ^= seed << 13;
      seed ^= seed >> 17;
      seed ^= seed << 5;
      const uint8_t j = seed % (i + 1);
      const uint8_t value = cards[i];
      cards[i] = cards[j];
      cards[j] = value;
    }
    matched = 0;
    first = second = 8;
  }
  bool shown(uint8_t card) const { return card < 8 && ((matched & (1 << card)) || card == first || card == second); }
  uint8_t value(uint8_t card) const { return card < 8 ? cards[card] : 0; }
  bool complete() const { return matched == 255; }
  bool hasMiss() const { return second < 8; }
  void hideMiss() {
    if (hasMiss()) first = second = 8;
  }
  Result reveal(uint8_t card) {
    if (card >= 8 || hasMiss() || shown(card) || complete()) return Result::Ignored;
    if (first == 8) {
      first = card;
      return Result::Revealed;
    }
    if (cards[first] == cards[card]) {
      matched |= (1 << first) | (1 << card);
      first = 8;
      return complete() ? Result::Finished : Result::Match;
    }
    second = card;
    return Result::Miss;
  }

 private:
  std::array<uint8_t, 8> cards{};
  uint8_t matched = 0;
  uint8_t first = 8;
  uint8_t second = 8;
};
