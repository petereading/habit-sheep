#include "HabitReset.h"

#include <HalStorage.h>
#include <Logging.h>

#include <cstring>

#include "HabitEventLog.h"
#include "HabitSheepStore.h"
#include "HabitTimer.h"
#include "SheepStateStore.h"

namespace {
constexpr const char* STAGE = "/.crosspoint/habit_reset_stage";
constexpr const char* JOURNAL = "/.crosspoint/habit_reset_pending";
constexpr const char* COMMITTED = "/.crosspoint/habit_reset_committed";
constexpr const char* MARKER_TMP = "/.crosspoint/habit_reset_marker.tmp";
struct Target {
  const char* live;
  const char* staged;
  const char* backup;
  bool directory;
};
constexpr Target TARGETS[] = {{"/.crosspoint/habit_sheep.json", "/.crosspoint/habit_reset_stage/config.json",
                               "/.crosspoint/habit_reset_config.old", false},
                              {"/.crosspoint/habit_sheep_state.json", "/.crosspoint/habit_reset_stage/sheep.json",
                               "/.crosspoint/habit_reset_sheep.old", false},
                              {"/.crosspoint/habit_timers.json", "/.crosspoint/habit_reset_stage/timers.json",
                               "/.crosspoint/habit_reset_timers.old", false},
                              {"/.crosspoint/habit_events", "/.crosspoint/habit_reset_stage/events",
                               "/.crosspoint/habit_reset_events.old", true}};
bool pending = false;
bool erase(const char* path, bool directory) {
  return !Storage.exists(path) || (directory ? Storage.removeDir(path) : Storage.remove(path));
}
bool marker(const char* path, const char* text, size_t size) {
  {
    auto file = Storage.open(MARKER_TMP, O_WRONLY | O_CREAT | O_TRUNC);
    if (!file || file.write(reinterpret_cast<const uint8_t*>(text), size) != size) return false;
    file.flush();
  }
  char check[8]{};
  {
    HalFile file;
    if (!Storage.openFileForRead("HABIT", MARKER_TMP, file) ||
        file.read(check, sizeof(check)) != static_cast<int>(size) || memcmp(check, text, size))
      return false;
  }
  return Storage.rename(MARKER_TMP, path);
}
bool reload() {
  if (!HABIT_SHEEP.loadFromFile() || !SHEEP_STATE.loadFromFile() || !HABIT_TIMER.loadFromFile()) return false;
  HABIT_SHEEP.notifyReset();
  HABIT_EVENTS.clearCacheAfterReset();
  return true;
}
}  // namespace
bool habitResetPending() { return pending; }

bool recoverHabitReset() {
  if (!Storage.exists(JOURNAL)) {
    pending = false;
    return true;
  }
  pending = true;
  char journal[8]{};
  {
    HalFile file;
    if (!Storage.openFileForRead("HABIT", JOURNAL, file) || file.read(journal, sizeof(journal)) != 5 ||
        journal[0] != 'R')
      return false;
  }
  for (int i = 1; i < 5; ++i)
    if (journal[i] != '0' && journal[i] != '1') return false;
  bool committed = false;
  if (Storage.exists(COMMITTED)) {
    HalFile file;
    char ch[2]{};
    if (!Storage.openFileForRead("HABIT", COMMITTED, file) || file.read(ch, sizeof(ch)) != 1 || ch[0] != 'C')
      return false;
    committed = true;
  }
  for (int i = 0; i < 4; ++i) {
    const auto& target = TARGETS[i];
    if (committed) {
      if (!erase(target.backup, target.directory)) return false;
    } else if (Storage.exists(target.backup)) {
      if (!erase(target.live, target.directory) || !Storage.rename(target.backup, target.live)) return false;
    } else if (journal[i + 1] == '0' && !erase(target.live, target.directory))
      return false;
  }
  if (!erase(STAGE, true) || !erase(MARKER_TMP, false)) return false;
  // Keep the committed marker until the journal is gone: cleanup must never become rollback.
  if (!Storage.remove(JOURNAL)) return false;
  pending = false;
  erase(COMMITTED, false);
  return true;
}

bool resetHabits() {
  if (!recoverHabitReset()) return false;
  // Orphan preparation files precede the journal and have never touched live data.
  if (!erase(STAGE, true) || !erase(COMMITTED, false) || !Storage.ensureDirectoryExists(STAGE)) return false;
  for (const auto& target : TARGETS)
    if (Storage.exists(target.backup)) return false;
  if (!HabitSheepStore::writeDefaults(TARGETS[0].staged) || !SheepStateStore::writeDefaults(TARGETS[1].staged) ||
      !HabitTimer::writeDefaults(TARGETS[2].staged) || !Storage.ensureDirectoryExists(TARGETS[3].staged))
    return false;
  char journal[5] = {'R', '0', '0', '0', '0'};
  for (int i = 0; i < 4; ++i)
    if (Storage.exists(TARGETS[i].live)) journal[i + 1] = '1';
  if (!marker(JOURNAL, journal, sizeof(journal))) return false;
  pending = true;
  for (const auto& target : TARGETS) {
    if ((Storage.exists(target.live) && !Storage.rename(target.live, target.backup)) ||
        !Storage.rename(target.staged, target.live)) {
      if (!recoverHabitReset()) LOG_ERR("HABIT", "Reset rollback pending; repair SD and restart");
      return false;
    }
  }
  if (!marker(COMMITTED, "C", 1)) {
    if (!recoverHabitReset()) LOG_ERR("HABIT", "Reset rollback pending; repair SD and restart");
    return false;
  }
  if (!reload()) {
    LOG_ERR("HABIT", "Reset committed; reload pending after restart");
    return false;
  }
  return recoverHabitReset();
}
