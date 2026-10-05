#include <HalClock.h>
#include <HalStorage.h>
#include <gtest/gtest.h>

#include <cstdlib>
#include <memory>

#include "HabitEventLog.h"
#include "HabitHistoryMath.h"
#include "HabitSheepStore.h"
#include "HabitTimer.h"
#include "SheepMemoryGame.h"
#include "SheepPuzzle.h"
#include "SheepScene.h"
#include "SheepStateStore.h"
#include "components/HabitClock.h"
#include "components/PopupCallback.h"

unsigned long habitTestMillis = 0;
bool habitTestSaveFailure = false;
std::map<std::string, std::string> habitTestSnapshots;
TestClock halClock;
TestStorage Storage;

class HabitSheepTest : public ::testing::Test {
 protected:
  void date(int day) {
    halClock.now = {};
    halClock.now.tm_year = 126;
    halClock.now.tm_mon = 9;
    halClock.now.tm_mday = day;
    halClock.now.tm_hour = 12;
    mktime(&halClock.now);
  }
  void advance(unsigned long seconds) { habitTestMillis += seconds * 1000; }
  void stock(uint8_t amount = 3, uint32_t lastDay = 20261001) {
    JsonDocument doc;
    doc["schema"] = 4;
    doc["mealsProcessed"] = 1;
    doc["eatenToday"] = 1;
    doc["grassStock"] = amount;
    doc["lastFedDay"] = lastDay;
    ASSERT_TRUE(SHEEP_STATE.fromJson(doc));
  }
  void SetUp() override {
    setenv("TZ", "UTC0", 1);
    tzset();
    habitTestMillis = 1;
    habitTestSaveFailure = false;
    habitTestSnapshots.clear();
    Storage.files.clear();
    halClock.available = true;
    date(1);
    HabitEventLog::RewardNotice reward;
    while (HABIT_EVENTS.takeReward(reward)) {
    }
    JsonDocument doc;
    doc["schema"] = 4;
    auto habits = doc["habits"].to<JsonArray>();
    for (const char* type : {"pomodoro", "duration", "completion"}) {
      auto habit = habits.add<JsonObject>();
      habit["id"] = type;
      habit["name"] = type;
      habit["type"] = type;
      habit["targetMinutes"] = 1;
      habit["targetCount"] = 1;
      habit["shortBreakMinutes"] = 1;
      habit["longBreakMinutes"] = 2;
      habit["sessionsPerCycle"] = 2;
    }
    ASSERT_TRUE(HABIT_SHEEP.fromJson(doc));
    JsonDocument empty;
    ASSERT_TRUE(HABIT_TIMER.fromJson(empty));
    stock();
    ASSERT_TRUE(HABIT_EVENTS.refreshToday());
  }
};

TEST_F(HabitSheepTest, SkipShortBreakStartsFocusWithoutDuplicateReward) {
  ASSERT_TRUE(HABIT_TIMER.start("pomodoro"));
  advance(60);
  HABIT_TIMER.tick();
  EXPECT_EQ(HABIT_TIMER.phaseFor("pomodoro"), HabitTimer::Phase::ShortBreak);
  EXPECT_FALSE(HABIT_TIMER.isRunning());
  EXPECT_EQ(SHEEP_STATE.getGrassStock(), 4);
  HabitEventLog::RewardNotice notice;
  ASSERT_TRUE(HABIT_EVENTS.takeReward(notice));
  EXPECT_EQ(notice.grass, 1);
  ASSERT_TRUE(HABIT_TIMER.skipShortBreak("pomodoro"));
  EXPECT_EQ(HABIT_TIMER.phaseFor("pomodoro"), HabitTimer::Phase::Focus);
  EXPECT_TRUE(HABIT_TIMER.isRunning());
  EXPECT_EQ(HABIT_TIMER.elapsedSecondsFor("pomodoro"), 0);
  EXPECT_FALSE(HABIT_EVENTS.takeReward(notice));
  advance(60);
  HABIT_TIMER.tick();
  EXPECT_EQ(HABIT_EVENTS.progressForToday("pomodoro").pomodoroSessions, 2);
  EXPECT_EQ(HABIT_TIMER.phaseFor("pomodoro"), HabitTimer::Phase::LongBreak);
  EXPECT_FALSE(HABIT_TIMER.skipShortBreak("pomodoro"));
}

TEST_F(HabitSheepTest, CanSkipRunningShortBreakAndResumeSavedFocusManually) {
  ASSERT_TRUE(HABIT_TIMER.start("pomodoro"));
  advance(60);
  HABIT_TIMER.tick();
  ASSERT_TRUE(HABIT_TIMER.resume("pomodoro"));
  advance(20);
  ASSERT_TRUE(HABIT_TIMER.skipShortBreak("pomodoro"));
  JsonDocument saved;
  HABIT_TIMER.toJson(saved);
  ASSERT_TRUE(HABIT_TIMER.fromJson(saved));
  EXPECT_EQ(HABIT_TIMER.phaseFor("pomodoro"), HabitTimer::Phase::Focus);
  EXPECT_FALSE(HABIT_TIMER.isRunning());
  EXPECT_EQ(SHEEP_STATE.getGrassStock(), 4);
  ASSERT_TRUE(HABIT_TIMER.resume("pomodoro"));
}

TEST_F(HabitSheepTest, SkipFailureRestoresBreakAndBlocksAnotherRunningTimer) {
  ASSERT_TRUE(HABIT_TIMER.start("pomodoro"));
  advance(60);
  HABIT_TIMER.tick();
  ASSERT_TRUE(HABIT_TIMER.resume("pomodoro"));
  advance(10);
  habitTestSaveFailure = true;
  EXPECT_FALSE(HABIT_TIMER.skipShortBreak("pomodoro"));
  EXPECT_EQ(HABIT_TIMER.phaseFor("pomodoro"), HabitTimer::Phase::ShortBreak);
  EXPECT_TRUE(HABIT_TIMER.isRunning());
  EXPECT_EQ(HABIT_TIMER.elapsedSecondsFor("pomodoro"), 10);
  habitTestSaveFailure = false;
  ASSERT_TRUE(HABIT_TIMER.pause("pomodoro"));
  ASSERT_TRUE(HABIT_TIMER.start("duration"));
  EXPECT_FALSE(HABIT_TIMER.skipShortBreak("pomodoro"));
}

TEST_F(HabitSheepTest, DurationAwardsEachRunningSessionSilentlyBeyondTarget) {
  ASSERT_TRUE(HABIT_TIMER.start("duration"));
  advance(60);
  HABIT_TIMER.tick();
  EXPECT_EQ(HABIT_EVENTS.progressForToday("duration").durationSeconds, 60);
  EXPECT_EQ(SHEEP_STATE.getGrassStock(), 4);
  EXPECT_TRUE(HABIT_TIMER.isRunning());
  HabitEventLog::RewardNotice notice;
  EXPECT_FALSE(HABIT_EVENTS.takeReward(notice));
  advance(60);
  HABIT_TIMER.tick();
  HABIT_TIMER.stopAndLog("duration");
  EXPECT_EQ(HABIT_EVENTS.progressForToday("duration").durationSeconds, 120);
  EXPECT_EQ(SHEEP_STATE.getGrassStock(), 5);
  EXPECT_FALSE(HABIT_EVENTS.takeReward(notice));
}

TEST_F(HabitSheepTest, PastReadingDayCreditsEachNewWholeSessionSilently) {
  date(2);
  ASSERT_TRUE(HABIT_EVENTS.appendDurationSecondsOnDay("duration", 30, "2026-10-01"));
  EXPECT_EQ(SHEEP_STATE.getGrassStock(), 3);
  ASSERT_TRUE(HABIT_EVENTS.appendDurationSecondsOnDay("duration", 30, "2026-10-01"));
  EXPECT_EQ(SHEEP_STATE.grassForDay(20261001).earned, 1);
  EXPECT_EQ(HABIT_EVENTS.progressForToday("duration").durationSeconds, 0);
  HabitEventLog::RewardNotice notice;
  EXPECT_FALSE(HABIT_EVENTS.takeReward(notice));
  ASSERT_TRUE(HABIT_EVENTS.appendDurationSecondsOnDay("duration", 60, "2026-10-01"));
  EXPECT_EQ(SHEEP_STATE.grassForDay(20261001).earned, 2);
  EXPECT_FALSE(HABIT_EVENTS.takeReward(notice));
}

TEST_F(HabitSheepTest, FullStockStillLogsCompletionAndZeroGainNotice) {
  stock(21);
  ASSERT_TRUE(HABIT_EVENTS.appendCompletion("completion"));
  EXPECT_EQ(HABIT_EVENTS.progressForToday("completion").completionCount, 1);
  EXPECT_EQ(SHEEP_STATE.getGrassStock(), 21);
  HabitEventLog::RewardNotice notice;
  ASSERT_TRUE(HABIT_EVENTS.takeReward(notice));
  EXPECT_EQ(notice.grass, 0);
  EXPECT_TRUE(HABIT_EVENTS.appendCompletion("completion"));
  EXPECT_EQ(HABIT_EVENTS.progressForToday("completion").completionCount, 2);
}

TEST_F(HabitSheepTest, WeeklyCountsEarnOneEachIncludingBeyondGoal) {
  auto habit = *HABIT_SHEEP.findHabit("completion");
  habit.period = HabitPeriod::Weekly;
  habit.targetCount = 3;
  ASSERT_TRUE(HABIT_SHEEP.upsertHabit(habit));
  for (int i = 0; i < 3; ++i) ASSERT_TRUE(HABIT_EVENTS.appendCompletion("completion"));
  EXPECT_EQ(SHEEP_STATE.getGrassStock(), 6);
  EXPECT_EQ(SHEEP_STATE.grassForDay(20261001).earned, 3);
  EXPECT_TRUE(HABIT_EVENTS.appendCompletion("completion"));
  EXPECT_EQ(SHEEP_STATE.getGrassStock(), 7);
}

TEST_F(HabitSheepTest, OfflineFeedingHasNoDebtAndReplenishmentReturnsSheep) {
  stock(3);
  date(20);
  ASSERT_TRUE(SHEEP_STATE.settleDay());
  EXPECT_EQ(SHEEP_STATE.getGrassStock(), 0);
  EXPECT_EQ(SHEEP_STATE.grassForDay(20261002).eaten, 1);
  EXPECT_EQ(SHEEP_STATE.grassForDay(20261004).eaten, 0);
  EXPECT_EQ(SHEEP_STATE.grassForDay(20261005).eaten, 0);
  EXPECT_FALSE(SHEEP_STATE.settleDay());
  EXPECT_EQ(SHEEP_STATE.addGrass(1), 1);
  EXPECT_EQ(SHEEP_STATE.getGrassStock(), 0);
  EXPECT_FALSE(SHEEP_STATE.isForaging());
  EXPECT_FALSE(SHEEP_STATE.settleDay());
  date(21);
  ASSERT_TRUE(SHEEP_STATE.settleDay());
  EXPECT_EQ(SHEEP_STATE.getGrassStock(), 0);
}

TEST_F(HabitSheepTest, MigrationClampsPastureAndDoesNotInventHistory) {
  JsonDocument old;
  old["schema"] = 2;
  old["pasturePoints"] = 10000;
  old["bondPoints"] = 42;
  ASSERT_TRUE(SHEEP_STATE.fromJson(old));
  EXPECT_EQ(SHEEP_STATE.getGrassStock(), 21);
  EXPECT_EQ(SHEEP_STATE.getBondPoints(), 42);
  EXPECT_EQ(SHEEP_STATE.grassForDay(20261001).day, 0);
  ASSERT_TRUE(SHEEP_STATE.settleDay());
  EXPECT_EQ(SHEEP_STATE.getGrassStock(), 21);
}

TEST_F(HabitSheepTest, LedgerKeepsNewestFourteenDaysAndSurvivesJsonRoundTrip) {
  for (int i = 1; i <= 16; ++i) {
    date(i);
    SHEEP_STATE.addGrass(3);
  }
  EXPECT_EQ(SHEEP_STATE.grassForDay(20261001).day, 0);
  EXPECT_EQ(SHEEP_STATE.grassForDay(20261002).day, 0);
  EXPECT_EQ(SHEEP_STATE.grassForDay(20261003).earned, 3);
  EXPECT_EQ(SHEEP_STATE.grassForDay(20261016).eaten, 1);
  JsonDocument saved;
  SHEEP_STATE.toJson(saved);
  ASSERT_TRUE(SHEEP_STATE.fromJson(saved));
  EXPECT_EQ(SHEEP_STATE.grassForDay(20261016).earned, 3);
  SHEEP_STATE.addGrass(1, "2026-10-01");
  EXPECT_EQ(SHEEP_STATE.grassForDay(20261003).earned, 3);
}

TEST_F(HabitSheepTest, ClockRefreshesOnlyWhenCalendarMinuteChanges) {
  HabitClock clock;
  EXPECT_TRUE(clock.changed());
  EXPECT_FALSE(clock.changed());
  halClock.now.tm_sec = 59;
  EXPECT_FALSE(clock.changed());
  halClock.now.tm_min = 1;
  EXPECT_TRUE(clock.changed());
  EXPECT_FALSE(clock.changed());
  date(2);
  EXPECT_TRUE(clock.changed());
  halClock.available = false;
  EXPECT_TRUE(clock.changed());
  EXPECT_FALSE(clock.changed());
}

TEST_F(HabitSheepTest, ThreeMealsUseLocalBoundariesExactlyOnceAcrossRestart) {
  stock(21);
  halClock.now.tm_hour = 12;
  EXPECT_FALSE(SHEEP_STATE.settleDay());
  halClock.now.tm_hour = 13;
  EXPECT_TRUE(SHEEP_STATE.settleDay());
  EXPECT_EQ(SHEEP_STATE.getGrassStock(), 20);
  EXPECT_FALSE(SHEEP_STATE.settleDay());
  JsonDocument saved;
  SHEEP_STATE.toJson(saved);
  ASSERT_TRUE(SHEEP_STATE.fromJson(saved));
  EXPECT_FALSE(SHEEP_STATE.settleDay());
  halClock.now.tm_hour = 19;
  EXPECT_TRUE(SHEEP_STATE.settleDay());
  EXPECT_EQ(SHEEP_STATE.getGrassStock(), 19);
  date(2);
  halClock.now.tm_hour = 7;
  EXPECT_TRUE(SHEEP_STATE.settleDay());
  EXPECT_EQ(SHEEP_STATE.getGrassStock(), 19);
  halClock.now.tm_hour = 8;
  EXPECT_TRUE(SHEEP_STATE.settleDay());
  EXPECT_EQ(SHEEP_STATE.getGrassStock(), 18);
}

TEST_F(HabitSheepTest, OneMissedMealRestsAndOnlyAnEntireEmptyDayCostsOneHeart) {
  stock(0);
  halClock.now.tm_hour = 19;
  ASSERT_TRUE(SHEEP_STATE.settleDay());
  EXPECT_TRUE(SHEEP_STATE.isResting());
  EXPECT_EQ(SHEEP_STATE.getMood(), 5);
  date(2);
  ASSERT_TRUE(SHEEP_STATE.settleDay());
  EXPECT_EQ(SHEEP_STATE.getMood(), 5);  // Breakfast on day one was already eaten.
  date(3);
  ASSERT_TRUE(SHEEP_STATE.settleDay());
  EXPECT_EQ(SHEEP_STATE.getMood(), 4);
  EXPECT_FALSE(SHEEP_STATE.settleDay());
  date(20);
  ASSERT_TRUE(SHEEP_STATE.settleDay());
  EXPECT_EQ(SHEEP_STATE.getMood(), 0);
  EXPECT_TRUE(SHEEP_STATE.isForaging());
  SHEEP_STATE.addGrass(3);
  EXPECT_FALSE(SHEEP_STATE.isForaging());
  EXPECT_EQ(SHEEP_STATE.getGrassStock(), 2);
  EXPECT_EQ(SHEEP_STATE.grassForDay(20261020).eaten, 1);
  halClock.now.tm_hour = 19;
  SHEEP_STATE.settleDay();
  EXPECT_LE(SHEEP_STATE.grassForDay(20261020).eaten, 3);
}

TEST_F(HabitSheepTest, PauseFreezesTimersCareAndRewardsWithNoCatchup) {
  stock(21);
  ASSERT_TRUE(HABIT_TIMER.start("duration"));
  advance(20);
  ASSERT_TRUE(HABIT_SHEEP.setEnabled(false));
  EXPECT_FALSE(HABIT_TIMER.isRunning());
  EXPECT_EQ(HABIT_TIMER.elapsedSecondsFor("duration"), 20);
  date(25);
  advance(3600);
  SHEEP_STATE.settleDay();
  EXPECT_EQ(SHEEP_STATE.getGrassStock(), 21);
  EXPECT_EQ(SHEEP_STATE.getMood(), 5);
  EXPECT_FALSE(HABIT_EVENTS.appendCompletion("completion"));
  EXPECT_FALSE(HABIT_EVENTS.appendDurationSecondsOnDay("duration", 60, "2026-10-01"));
  EXPECT_FALSE(HABIT_TIMER.start("pomodoro"));
  EXPECT_FALSE(HABIT_TIMER.resume("duration"));
  ASSERT_TRUE(HABIT_SHEEP.setEnabled(true));
  EXPECT_TRUE(SHEEP_STATE.grassForDay(20261024).paused);
  EXPECT_FALSE(HABIT_TIMER.isRunning());
  EXPECT_FALSE(SHEEP_STATE.settleDay());
  EXPECT_EQ(SHEEP_STATE.getGrassStock(), 21);
  ASSERT_TRUE(HABIT_TIMER.resume("duration"));
  advance(40);
  HABIT_TIMER.tick();
  EXPECT_EQ(HABIT_EVENTS.progressForToday("duration").durationSeconds, 0);
  EXPECT_EQ(HABIT_TIMER.elapsedSecondsFor("duration"), 40);
}

TEST_F(HabitSheepTest, WeekStartChangesCountsWithoutAwardingOldEvents) {
  auto habit = *HABIT_SHEEP.findHabit("completion");
  habit.period = HabitPeriod::Weekly;
  habit.targetCount = 7;
  ASSERT_TRUE(HABIT_SHEEP.upsertHabit(habit));
  date(4);  // Sunday
  ASSERT_TRUE(HABIT_EVENTS.appendCompletion("completion"));
  date(5);  // Monday
  EXPECT_EQ(HABIT_EVENTS.completionCountForWeek("completion"), 0);
  const auto grass = SHEEP_STATE.getGrassStock();
  ASSERT_TRUE(HABIT_SHEEP.setWeekStart(0));
  EXPECT_EQ(HABIT_EVENTS.completionCountForWeek("completion"), 1);
  EXPECT_EQ(SHEEP_STATE.getGrassStock(), grass);
  ASSERT_TRUE(HABIT_SHEEP.setWeekStart(1));
  EXPECT_EQ(HABIT_EVENTS.completionCountForWeek("completion"), 0);
  EXPECT_FALSE(HABIT_SHEEP.setWeekStart(7));
}

TEST_F(HabitSheepTest, ClockUnavailableAndBackwardCorrectionsDoNotFeedTwice) {
  stock(21);
  halClock.available = false;
  EXPECT_FALSE(SHEEP_STATE.settleDay());
  EXPECT_EQ(SHEEP_STATE.getGrassStock(), 21);
  halClock.available = true;
  halClock.now.tm_hour = 19;
  ASSERT_TRUE(SHEEP_STATE.settleDay());
  EXPECT_EQ(SHEEP_STATE.getGrassStock(), 19);
  halClock.now.tm_hour = 8;
  EXPECT_FALSE(SHEEP_STATE.settleDay());
  date(0);
  EXPECT_FALSE(SHEEP_STATE.settleDay());
  date(1);
  halClock.now.tm_hour = 19;
  EXPECT_FALSE(SHEEP_STATE.settleDay());
  EXPECT_EQ(SHEEP_STATE.getGrassStock(), 19);
}

TEST_F(HabitSheepTest, InteractionImprovesBondAtMostOnceDailyAndNeverCreatesFood) {
  const auto grass = SHEEP_STATE.getGrassStock();
  SHEEP_STATE.recordInteraction();
  SHEEP_STATE.recordInteraction();
  EXPECT_EQ(SHEEP_STATE.getBondPoints(), 1);
  EXPECT_EQ(SHEEP_STATE.getGrassStock(), grass);
  date(2);
  SHEEP_STATE.recordInteraction();
  EXPECT_EQ(SHEEP_STATE.getBondPoints(), 2);
}

TEST_F(HabitSheepTest, MemoryGameKeepsMissVisibleUntilAcknowledgedAndFinishesFourPairs) {
  SheepMemoryGame game;
  game.reset(42);
  int counts[4]{};
  for (int i = 0; i < 8; ++i) ++counts[game.value(i)];
  for (int count : counts) EXPECT_EQ(count, 2);
  int different = 1;
  while (game.value(different) == game.value(0)) ++different;
  EXPECT_EQ(game.reveal(0), SheepMemoryGame::Result::Revealed);
  EXPECT_EQ(game.reveal(different), SheepMemoryGame::Result::Miss);
  EXPECT_TRUE(game.shown(0));
  EXPECT_TRUE(game.shown(different));
  EXPECT_EQ(game.reveal(7), SheepMemoryGame::Result::Ignored);
  game.hideMiss();
  EXPECT_FALSE(game.shown(0));
  for (int value = 0; value < 4; ++value) {
    for (int i = 0; i < 8; ++i)
      if (game.value(i) == value) game.reveal(i);
  }
  EXPECT_TRUE(game.complete());
  EXPECT_EQ(game.reveal(0), SheepMemoryGame::Result::Ignored);
}

TEST(SheepPuzzleTest, DifferentAlwaysHasExactlyOneAnswerAndMistakesWaitForConfirmation) {
  for (uint32_t seed = 0; seed < 100; ++seed) {
    SheepPuzzle p;
    p.reset(SheepPuzzle::Mode::Different, seed);
    int frequencies[4]{};
    for (uint8_t i = 0; i < 4; ++i) ++frequencies[p.value(i)];
    uint8_t answer = 0;
    while (frequencies[p.value(answer)] != 1) ++answer;
    p.choose((answer + 1) % 4);
    EXPECT_TRUE(p.hasMistake());
    p.choose(answer);
    EXPECT_FALSE(p.complete());
    EXPECT_FALSE(p.hasMistake());
    p.choose(answer);
    EXPECT_TRUE(p.complete());
  }
}

TEST(HabitHistoryTest, WeeklyRemaindersCarryWithinWeekAndDailyRemaindersDoNot) {
  uint32_t carried = 0;
  EXPECT_EQ(historySessions(1200, 1800, true, true, carried), 0U);
  EXPECT_EQ(historySessions(1200, 1800, true, false, carried), 1U);
  EXPECT_EQ(historySessions(1200, 1800, true, false, carried), 1U);
  EXPECT_EQ(historySessions(1200, 1800, true, true, carried), 0U);
  EXPECT_EQ(historySessions(1200, 1800, false, false, carried), 0U);
  EXPECT_EQ(historySessions(1200, 1800, false, false, carried), 0U);
}

TEST(SheepPuzzleTest, RememberWaitsForUserAndKeepsAllFourPositionsDistinct) {
  SheepPuzzle p;
  p.reset(SheepPuzzle::Mode::Remember, 42);
  bool seen[4]{};
  uint8_t answer = 0;
  for (uint8_t i = 0; i < 4; ++i) {
    EXPECT_FALSE(seen[p.value(i)]);
    seen[p.value(i)] = true;
    if (p.value(i) == p.targetValue()) answer = i;
  }
  EXPECT_TRUE(p.showing());
  p.choose(answer);
  EXPECT_FALSE(p.showing());
  EXPECT_FALSE(p.complete());
  p.choose(answer);
  EXPECT_TRUE(p.complete());
}

TEST(SheepPuzzleTest, OrderingCanBeSolvedBySelectingTwoPositions) {
  for (uint32_t seed = 1; seed < 100; ++seed) {
    SheepPuzzle p;
    p.reset(SheepPuzzle::Mode::Order, seed);
    EXPECT_FALSE(p.ordered());
    for (uint8_t i = 0; i < 4 && !p.complete(); ++i) {
      if (p.value(i) == i) continue;
      uint8_t j = 0;
      while (p.value(j) != i) ++j;
      p.choose(i);
      EXPECT_EQ(p.picked(), i);
      p.choose(j);
    }
    EXPECT_TRUE(p.complete());
    EXPECT_TRUE(p.ordered());
  }
}

TEST(SheepMemoryTest, SixHousesHaveThreeDistinctPairsAndRequireAcknowledgedMiss) {
  SheepMemoryGame game;
  game.reset(123, 6);
  int frequencies[4]{};
  for (uint8_t i = 0; i < 6; ++i) ++frequencies[game.value(i)];
  int pairs = 0;
  for (int n : frequencies) {
    EXPECT_TRUE(n == 0 || n == 2);
    pairs += n == 2;
  }
  EXPECT_EQ(pairs, 3);
  EXPECT_EQ(game.reveal(6), SheepMemoryGame::Result::Ignored);
  for (uint8_t v = 0; v < 4; ++v)
    for (uint8_t i = 0; i < 6; ++i)
      if (game.value(i) == v) game.reveal(i);
  EXPECT_TRUE(game.complete());
  for (uint8_t i = 0; i < 6; ++i) EXPECT_TRUE(game.isMatched(i));
}

TEST(SheepSceneTest, MirrorDirectionStaysStableDuringEachPoseAndMeal) {
  tm local{};
  local.tm_year = 126;
  local.tm_yday = 277;
  local.tm_hour = 13;
  bool both[2]{};
  for (int slot = 0; slot < 12; ++slot) {
    local.tm_hour = 10 + slot / 6;
    local.tm_min = slot % 6 * 10;
    const bool direction = sheepScene::mirrored(local, false, false);
    both[direction] = true;
    for (int minute = 0; minute < 10; ++minute) {
      local.tm_min = slot % 6 * 10 + minute;
      EXPECT_EQ(sheepScene::mirrored(local, false, false), direction);
    }
  }
  EXPECT_TRUE(both[0] && both[1]);
  local.tm_hour = 13;
  local.tm_min = 0;
  const bool mealDirection = sheepScene::mirrored(local, false, true);
  for (int i = 0; i < 5; ++i) {
    local.tm_min = i;
    EXPECT_EQ(sheepScene::mirrored(local, false, true), mealDirection);
  }
}

TEST_F(HabitSheepTest, HistoryReadsOnlySelectedHabitAndHandlesPartialOrCorruptLines) {
  Storage.files["/.crosspoint/habit_events/2026-09-30.jsonl"] =
      "{\"habit_id\":\"other\",\"type\":\"completion\"}\n"
      "{\"habit_id\":\"duration\",\"type\":\"duration\",\"amount\":600}\n"
      "corrupt\n"
      "{\"habit_id\":\"duration\",\"type\":\"pomodoro\",\"amount\":1500}";
  std::array<char, 512> scratch{};
  HabitDailyProgress progress;
  EXPECT_TRUE(HABIT_EVENTS.progressOnDay("duration", "2026-09-30", progress, scratch.data(), scratch.size()));
  EXPECT_EQ(progress.durationSeconds, 2100U);
  EXPECT_EQ(progress.pomodoroSessions, 1);
  EXPECT_EQ(progress.completionCount, 0);
  EXPECT_TRUE(HABIT_EVENTS.progressOnDay("duration", "2026-09-29", progress, scratch.data(), scratch.size()));
  EXPECT_EQ(progress.durationSeconds, 0U);
  EXPECT_FALSE(HABIT_EVENTS.progressOnDay("duration", nullptr, progress, scratch.data(), scratch.size()));
}

TEST_F(HabitSheepTest, FailedStateSaveRetriesOneMealWithoutDoubleConsumption) {
  stock(21);
  halClock.now.tm_hour = 13;
  habitTestSaveFailure = true;
  EXPECT_FALSE(SHEEP_STATE.settleDay());
  EXPECT_EQ(SHEEP_STATE.getGrassStock(), 21);
  EXPECT_EQ(SHEEP_STATE.grassForDay(20261001).eaten, 0);
  habitTestSaveFailure = false;
  EXPECT_TRUE(SHEEP_STATE.settleDay());
  EXPECT_EQ(SHEEP_STATE.getGrassStock(), 20);
  EXPECT_FALSE(SHEEP_STATE.settleDay());
  EXPECT_EQ(SHEEP_STATE.grassForDay(20261001).eaten, 1);
}

TEST_F(HabitSheepTest, FailedPauseSaveLeavesModeEnabledAndTimerRunning) {
  ASSERT_TRUE(HABIT_TIMER.start("duration"));
  advance(10);
  habitTestSaveFailure = true;
  EXPECT_FALSE(HABIT_SHEEP.setEnabled(false));
  EXPECT_TRUE(HABIT_SHEEP.isEnabled());
  EXPECT_TRUE(HABIT_TIMER.isRunning());
  EXPECT_EQ(HABIT_TIMER.elapsedSecondsFor("duration"), 10);
}

TEST_F(HabitSheepTest, CorrectingAnRtcFarInTheFutureRebasesWithoutRetroactiveCharges) {
  stock(21, 20271001);
  ASSERT_TRUE(SHEEP_STATE.settleDay());
  EXPECT_EQ(SHEEP_STATE.getGrassStock(), 21);
  halClock.now.tm_hour = 13;
  ASSERT_TRUE(SHEEP_STATE.settleDay());
  EXPECT_EQ(SHEEP_STATE.getGrassStock(), 20);
}

TEST_F(HabitSheepTest, WeeklyTargetDoesNotMultiplyCountReward) {
  stock(0);
  auto habit = *HABIT_SHEEP.findHabit("completion");
  habit.period = HabitPeriod::Weekly;
  habit.targetCount = 4;
  ASSERT_TRUE(HABIT_SHEEP.upsertHabit(habit));
  for (int i = 0; i < 4; ++i) ASSERT_TRUE(HABIT_EVENTS.appendCompletion("completion"));
  EXPECT_EQ(SHEEP_STATE.getGrassStock(), 4);
  EXPECT_EQ(SHEEP_STATE.grassForDay(20261001).earned, 4);
}

TEST_F(HabitSheepTest, DurationCombinesSourcesAndRewardsWholeSessionsOnly) {
  ASSERT_TRUE(HABIT_EVENTS.appendDurationSeconds("duration", 25, HabitEventSource::Reader));
  ASSERT_TRUE(HABIT_EVENTS.appendDurationSeconds("duration", 25, HabitEventSource::Timer));
  EXPECT_EQ(SHEEP_STATE.getGrassStock(), 3);
  ASSERT_TRUE(HABIT_EVENTS.appendDurationSeconds("duration", 135));
  EXPECT_EQ(SHEEP_STATE.getGrassStock(), 6);
  EXPECT_EQ(HABIT_EVENTS.progressForToday("duration").durationSeconds, 185);
  HabitEventLog::RewardNotice notice;
  EXPECT_FALSE(HABIT_EVENTS.takeReward(notice));
  ASSERT_TRUE(HABIT_EVENTS.refreshToday());
  ASSERT_TRUE(HABIT_EVENTS.appendDurationSeconds("duration", 54));
  EXPECT_EQ(SHEEP_STATE.getGrassStock(), 6);
  ASSERT_TRUE(HABIT_EVENTS.appendDurationSeconds("duration", 1));
  EXPECT_EQ(SHEEP_STATE.getGrassStock(), 7);
}

TEST_F(HabitSheepTest, PomodoroCycleSurvivesMidnightAndRestartWithoutDailyGoalOrBonus) {
  ASSERT_TRUE(HABIT_TIMER.start("pomodoro"));
  EXPECT_EQ(HABIT_TIMER.focusesUntilLongBreak("pomodoro"), 2);
  advance(60);
  HABIT_TIMER.tick();
  EXPECT_EQ(HABIT_TIMER.focusesUntilLongBreak("pomodoro"), 1);
  JsonDocument timer;
  HABIT_TIMER.toJson(timer);
  date(2);
  ASSERT_TRUE(HABIT_TIMER.fromJson(timer));
  EXPECT_FALSE(HABIT_TIMER.isRunning());
  EXPECT_EQ(HABIT_TIMER.focusesUntilLongBreak("pomodoro"), 1);
  ASSERT_TRUE(HABIT_TIMER.skipShortBreak("pomodoro"));
  advance(60);
  HABIT_TIMER.tick();
  EXPECT_EQ(HABIT_TIMER.phaseFor("pomodoro"), HabitTimer::Phase::LongBreak);
  EXPECT_EQ(HABIT_TIMER.focusesUntilLongBreak("pomodoro"), 0);
  EXPECT_EQ(HABIT_EVENTS.progressForToday("pomodoro").pomodoroSessions, 1);
  EXPECT_EQ(SHEEP_STATE.grassForDay(20261001).earned, 1);
  EXPECT_EQ(SHEEP_STATE.grassForDay(20261002).earned, 1);
  ASSERT_TRUE(HABIT_TIMER.resume("pomodoro"));
  advance(120);
  HABIT_TIMER.tick();
  EXPECT_EQ(HABIT_TIMER.focusesUntilLongBreak("pomodoro"), 2);
  EXPECT_EQ(SHEEP_STATE.grassForDay(20261002).earned, 1);
}

TEST_F(HabitSheepTest, WrongHabitTypeCannotEarnPomodoroReward) {
  EXPECT_FALSE(HABIT_EVENTS.appendPomodoroFocus("duration", 60));
  EXPECT_FALSE(HABIT_EVENTS.appendPomodoroFocus("missing", 60));
  EXPECT_EQ(SHEEP_STATE.getGrassStock(), 3);
}

TEST_F(HabitSheepTest, SessionLengthAndGoalEditsNeverRetroactivelyRewardOldTime) {
  ASSERT_TRUE(HABIT_EVENTS.appendDurationSeconds("duration", 120));
  const auto earned = SHEEP_STATE.grassForDay(20261001).earned;
  auto habit = *HABIT_SHEEP.findHabit("duration");
  habit.targetMinutes = 2;
  habit.targetCount = 4;
  ASSERT_TRUE(HABIT_SHEEP.upsertHabit(habit));
  ASSERT_TRUE(HABIT_EVENTS.appendDurationSeconds("duration", 1));
  EXPECT_EQ(SHEEP_STATE.grassForDay(20261001).earned, earned);
  habit.targetMinutes = 1;
  ASSERT_TRUE(HABIT_SHEEP.upsertHabit(habit));
  ASSERT_TRUE(HABIT_EVENTS.appendDurationSeconds("duration", 58));
  EXPECT_EQ(SHEEP_STATE.grassForDay(20261001).earned, earned);
  ASSERT_TRUE(HABIT_EVENTS.appendDurationSeconds("duration", 1));
  EXPECT_EQ(SHEEP_STATE.grassForDay(20261001).earned, earned + 1);
}

TEST_F(HabitSheepTest, DailyRemainderDoesNotCarryIntoNextDate) {
  ASSERT_TRUE(HABIT_EVENTS.appendDurationSeconds("duration", 59));
  date(2);
  ASSERT_TRUE(HABIT_EVENTS.appendDurationSeconds("duration", 1));
  EXPECT_EQ(SHEEP_STATE.grassForDay(20261002).earned, 0);
  ASSERT_TRUE(HABIT_EVENTS.appendDurationSeconds("duration", 59));
  EXPECT_EQ(SHEEP_STATE.grassForDay(20261002).earned, 1);
}

TEST_F(HabitSheepTest, WeeklyRemainderCarriesWithinWeekButNotAcrossBoundary) {
  auto habit = *HABIT_SHEEP.findHabit("duration");
  habit.period = HabitPeriod::Weekly;
  ASSERT_TRUE(HABIT_SHEEP.upsertHabit(habit));
  ASSERT_TRUE(HABIT_EVENTS.appendDurationSeconds("duration", 30));
  date(2);
  EXPECT_EQ(HABIT_EVENTS.durationSecondsForPeriod(habit), 30);
  ASSERT_TRUE(HABIT_EVENTS.appendDurationSeconds("duration", 30));
  EXPECT_EQ(HABIT_EVENTS.durationSecondsForPeriod(habit), 60);
  EXPECT_EQ(SHEEP_STATE.grassForDay(20261002).earned, 1);
  date(4);
  ASSERT_TRUE(HABIT_EVENTS.appendDurationSeconds("duration", 59));
  date(5);
  EXPECT_EQ(HABIT_EVENTS.durationSecondsForPeriod(habit), 0);
  ASSERT_TRUE(HABIT_EVENTS.appendDurationSeconds("duration", 1));
  EXPECT_EQ(SHEEP_STATE.grassForDay(20261005).earned, 0);
  ASSERT_TRUE(HABIT_SHEEP.setWeekStart(0));
  EXPECT_EQ(HABIT_EVENTS.durationSecondsForPeriod(habit), 60);
  ASSERT_TRUE(HABIT_EVENTS.appendDurationSeconds("duration", 1));
  EXPECT_EQ(SHEEP_STATE.grassForDay(20261005).earned, 0);
}

TEST_F(HabitSheepTest, WeeklyBackfilledReadingUsesItsOriginalWeekWithoutDuplicateReward) {
  auto habit = *HABIT_SHEEP.findHabit("duration");
  habit.period = HabitPeriod::Weekly;
  ASSERT_TRUE(HABIT_SHEEP.upsertHabit(habit));
  date(6);
  ASSERT_TRUE(HABIT_EVENTS.appendDurationSecondsOnDay("duration", 30, "2026-10-01"));
  ASSERT_TRUE(HABIT_EVENTS.appendDurationSecondsOnDay("duration", 30, "2026-10-02"));
  EXPECT_EQ(SHEEP_STATE.grassForDay(20261002).earned, 1);
  EXPECT_EQ(HABIT_EVENTS.durationSecondsForPeriod(habit), 0);
  ASSERT_TRUE(HABIT_EVENTS.appendDurationSecondsOnDay("duration", 1, "2026-10-01"));
  EXPECT_EQ(SHEEP_STATE.grassForDay(20261001).earned, 0);
}

TEST_F(HabitSheepTest, FullStockDurationHasNoDeferredRewardAfterEating) {
  stock(21);
  ASSERT_TRUE(HABIT_EVENTS.appendDurationSeconds("duration", 600));
  EXPECT_EQ(SHEEP_STATE.grassForDay(20261001).earned, 0);
  halClock.now.tm_hour = 19;
  SHEEP_STATE.settleDay();
  const auto remaining = SHEEP_STATE.getGrassStock();
  ASSERT_TRUE(HABIT_EVENTS.appendDurationSeconds("duration", 1));
  EXPECT_EQ(SHEEP_STATE.getGrassStock(), remaining);
}

TEST_F(HabitSheepTest, PausedDurationRestoresWithoutDuplicatingLoggedSessions) {
  ASSERT_TRUE(HABIT_TIMER.start("duration"));
  advance(65);
  HABIT_TIMER.tick();
  EXPECT_EQ(SHEEP_STATE.grassForDay(20261001).earned, 1);
  ASSERT_TRUE(HABIT_TIMER.pause("duration"));
  JsonDocument timer;
  HABIT_TIMER.toJson(timer);
  ASSERT_TRUE(HABIT_TIMER.fromJson(timer));
  ASSERT_TRUE(HABIT_TIMER.resume("duration"));
  advance(55);
  HABIT_TIMER.tick();
  EXPECT_EQ(HABIT_EVENTS.progressForToday("duration").durationSeconds, 120);
  EXPECT_EQ(SHEEP_STATE.grassForDay(20261001).earned, 2);
}

TEST_F(HabitSheepTest, RunningDurationSplitsMidnightInsteadOfCarryingDailyRemainder) {
  halClock.now.tm_hour = 23;
  halClock.now.tm_min = 59;
  halClock.now.tm_sec = 30;
  ASSERT_TRUE(HABIT_TIMER.start("duration"));
  advance(60);
  date(2);
  halClock.now.tm_hour = 0;
  halClock.now.tm_sec = 30;
  HABIT_TIMER.tick();
  EXPECT_EQ(HABIT_EVENTS.progressForToday("duration").durationSeconds, 0);
  EXPECT_EQ(HABIT_TIMER.elapsedSecondsFor("duration"), 30);
  EXPECT_EQ(SHEEP_STATE.grassForDay(20261001).earned, 0);
  EXPECT_EQ(SHEEP_STATE.grassForDay(20261002).earned, 0);
  advance(30);
  halClock.now.tm_min = 1;
  halClock.now.tm_sec = 0;
  HABIT_TIMER.tick();
  EXPECT_EQ(HABIT_EVENTS.progressForToday("duration").durationSeconds, 60);
  EXPECT_EQ(SHEEP_STATE.grassForDay(20261002).earned, 1);
}

TEST_F(HabitSheepTest, OrientationPersistsIndependentlyAndRollsBackFailedSaves) {
  EXPECT_EQ(HABIT_SHEEP.getOrientation(), 0);
  for (uint8_t value = 0; value < 4; ++value) {
    ASSERT_TRUE(HABIT_SHEEP.setOrientation(value));
    JsonDocument doc;
    HABIT_SHEEP.toJson(doc);
    ASSERT_TRUE(HABIT_SHEEP.fromJson(doc));
    EXPECT_EQ(HABIT_SHEEP.getOrientation(), value);
  }
  EXPECT_FALSE(HABIT_SHEEP.setOrientation(4));
  habitTestSaveFailure = true;
  EXPECT_FALSE(HABIT_SHEEP.setOrientation(0));
  EXPECT_EQ(HABIT_SHEEP.getOrientation(), 3);
  habitTestSaveFailure = false;
  JsonDocument doc;
  HABIT_SHEEP.toJson(doc);
  doc["orientation"] = 99;
  ASSERT_TRUE(HABIT_SHEEP.fromJson(doc));
  EXPECT_EQ(HABIT_SHEEP.getOrientation(), 0);
}

TEST_F(HabitSheepTest, HabitIconPersistsAndOlderOrInvalidValuesKeepAutomaticDefault) {
  auto habit = *HABIT_SHEEP.findHabit("completion");
  EXPECT_EQ(habit.icon, 255);
  for (uint8_t icon = 0; icon < 24; ++icon) {
    habit.icon = icon;
    ASSERT_TRUE(HABIT_SHEEP.upsertHabit(habit));
    JsonDocument doc;
    HABIT_SHEEP.toJson(doc);
    ASSERT_TRUE(HABIT_SHEEP.fromJson(doc));
    EXPECT_EQ(HABIT_SHEEP.findHabit("completion")->icon, icon);
  }
  habit.icon = 24;
  ASSERT_TRUE(HABIT_SHEEP.upsertHabit(habit));
  EXPECT_EQ(HABIT_SHEEP.findHabit("completion")->icon, 255);
  JsonDocument doc;
  HABIT_SHEEP.toJson(doc);
  doc["habits"][0]["icon"] = 254;
  ASSERT_TRUE(HABIT_SHEEP.fromJson(doc));
  EXPECT_EQ(HABIT_SHEEP.getHabits()[0].icon, 255);
}

TEST_F(HabitSheepTest, EatingSceneRequiresSuccessfullyPersistedCurrentMeal) {
  stock(2);
  halClock.now.tm_hour = 13;
  halClock.now.tm_min = 0;
  EXPECT_FALSE(SHEEP_STATE.ateCurrentMeal(halClock.now));
  habitTestSaveFailure = true;
  EXPECT_FALSE(SHEEP_STATE.settleDay());
  EXPECT_FALSE(SHEEP_STATE.ateCurrentMeal(halClock.now));
  habitTestSaveFailure = false;
  EXPECT_TRUE(SHEEP_STATE.settleDay());
  EXPECT_TRUE(SHEEP_STATE.ateCurrentMeal(halClock.now));
  halClock.now.tm_min = 5;
  EXPECT_FALSE(SHEEP_STATE.ateCurrentMeal(halClock.now));
  stock(0);
  halClock.now.tm_min = 0;
  EXPECT_TRUE(SHEEP_STATE.settleDay());
  EXPECT_TRUE(SHEEP_STATE.isResting());
  EXPECT_FALSE(SHEEP_STATE.ateCurrentMeal(halClock.now));
}

TEST(SheepSceneTest, AwakePosesDoNotRepeatForTwoHoursAndRestHasPriority) {
  tm local{};
  bool seen[12]{};
  for (int minute = 0; minute < 120; minute += 10) {
    local.tm_hour = 10 + minute / 60;
    local.tm_min = minute % 60;
    const auto pose = sheepScene::pose(local, false, false, false);
    EXPECT_LT(pose, 12);
    EXPECT_FALSE(seen[pose]);
    seen[pose] = true;
  }
  for (int minute = 0; minute < 120; minute += 30) {
    local.tm_hour = 10 + minute / 60;
    local.tm_min = minute % 60;
    EXPECT_EQ(sheepScene::pose(local, true, false, false), 12 + minute / 30);
    EXPECT_EQ(sheepScene::pose(local, false, true, false), 12 + minute / 30);
  }
  local.tm_hour = 13;
  local.tm_min = 0;
  EXPECT_EQ(sheepScene::pose(local, true, false, true), 16);
  EXPECT_GE(sheepScene::pose(local, true, true, false), 12);
  EXPECT_LT(sheepScene::pose(local, true, true, false), 16);
  local.tm_min = 5;
  EXPECT_LT(sheepScene::pose(local, true, false, true), 16);
}

TEST(SheepSceneTest, SleepWakeTargetsMealAndFiveMinuteReturnWithoutMinutePolling) {
  tm local{};
  local.tm_hour = 7;
  local.tm_min = 59;
  local.tm_sec = 59;
  EXPECT_EQ(sheepScene::sleepSeconds(local), 1);
  for (int hour : {8, 13, 19}) {
    local.tm_hour = hour;
    local.tm_min = 0;
    local.tm_sec = 0;
    EXPECT_EQ(sheepScene::sleepSeconds(local), 300);
    local.tm_min = 4;
    local.tm_sec = 59;
    EXPECT_EQ(sheepScene::sleepSeconds(local), 1);
    local.tm_min = 5;
    local.tm_sec = 0;
    EXPECT_EQ(sheepScene::sleepSeconds(local), 1500);
  }
  local.tm_hour = 23;
  local.tm_min = 30;
  EXPECT_EQ(sheepScene::sleepSeconds(local), 1800);
}

TEST(PopupCallbackTest, ReplacingCallbackKeepsExecutingCaptureAliveAndPreservesNextChoice) {
  std::function<void(int)> slot;
  auto capture = std::make_shared<int>(0);
  std::weak_ptr<int> lifetime = capture;
  int second = 0;
  slot = [capture, &slot, &second](int choice) {
    slot = [&second](int next) { second = next; };
    *capture = choice;
    EXPECT_EQ(capture.use_count(), 1);
  };
  capture.reset();
  invokePopupChoice(slot, 7);
  EXPECT_TRUE(lifetime.expired());
  ASSERT_TRUE(slot);
  invokePopupChoice(slot, 11);
  EXPECT_EQ(second, 11);
  EXPECT_FALSE(slot);
  invokePopupChoice(slot, 12);
  EXPECT_EQ(second, 11);
}
