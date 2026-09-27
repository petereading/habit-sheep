#include "SheepStateStore.h"

#include <algorithm>
#include <climits>

void SheepStateStore::toJson(JsonDocument& doc) const {
  doc["schema"] = 1;
  doc["bondPoints"] = bondPoints;
  doc["pasturePoints"] = pasturePoints;
}

bool SheepStateStore::fromJson(JsonVariantConst doc) {
  bondPoints = doc["bondPoints"] | static_cast<uint32_t>(0);
  pasturePoints = doc["pasturePoints"] | static_cast<uint32_t>(0);
  return true;
}

void SheepStateStore::recordInteraction() {
  if (bondPoints < UINT32_MAX) ++bondPoints;
  saveToFile();
}

void SheepStateStore::recordCompletion() {
  pasturePoints = std::min<uint32_t>(UINT32_MAX - 10, pasturePoints) + 10;
  saveToFile();
}

void SheepStateStore::recordDuration(const uint32_t seconds) {
  const uint32_t earned = seconds / 300;
  if (earned == 0) return;
  pasturePoints = std::min<uint32_t>(UINT32_MAX - earned, pasturePoints) + earned;
  saveToFile();
}
