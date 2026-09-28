#include "SheepStateStore.h"

#include <algorithm>
#include <climits>

void SheepStateStore::toJson(JsonDocument& doc) const {
  doc["schema"] = 2;
  doc["bondPoints"] = bondPoints;
  doc["pasturePoints"] = pasturePoints;
  doc["durationRemainderSeconds"] = durationRemainderSeconds;
}

bool SheepStateStore::fromJson(JsonVariantConst doc) {
  bondPoints = doc["bondPoints"] | static_cast<uint32_t>(0);
  pasturePoints = doc["pasturePoints"] | static_cast<uint32_t>(0);
  durationRemainderSeconds = std::min<uint16_t>(doc["durationRemainderSeconds"] | static_cast<uint16_t>(0), 299);
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
  if (seconds == 0) return;
  const uint64_t total = static_cast<uint64_t>(durationRemainderSeconds) + seconds;
  const uint32_t earned = static_cast<uint32_t>(total / 300);
  durationRemainderSeconds = static_cast<uint16_t>(total % 300);
  pasturePoints = std::min<uint32_t>(UINT32_MAX - earned, pasturePoints) + earned;
  saveToFile();
}
