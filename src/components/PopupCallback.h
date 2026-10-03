#pragma once

#include <functional>
#include <utility>

inline void invokePopupChoice(std::function<void(int)>& slot, int choice) {
  auto callback = std::move(slot);
  if (callback) callback(choice);
}
