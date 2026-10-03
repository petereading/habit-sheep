#include <HalClock.h>
#include <HalStorage.h>
#include <gtest/gtest.h>

#include <cstdlib>
#include <memory>

#include "HabitEventLog.h"
#include "HabitSheepStore.h"
#include "HabitTimer.h"
#include "SheepMemoryGame.h"
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
  EXPECT_EQ(SHEEP_STATE.getGrassStock(), 6);
  HabitEventLog::RewardNotice notice;
  ASSERT_TRUE(HABIT_EVENTS.takeReward(notice));
  EXPECT_EQ(notice.grass, 3);
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
  EXPECT_EQ(SHEEP_STATE.getGrassStock(), 6);
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

TEST_F(HabitSheepTest, DurationTargetAwardsWhileRunningOnlyOnce) {
  ASSERT_TRUE(HABIT_TIMER.start("duration"));
  advance(60);
  HABIT_TIMER.tick();
  EXPECT_EQ(HABIT_EVENTS.progressForToday("duration").durationSeconds, 60);
  EXPECT_EQ(SHEEP_STATE.getGrassStock(), 6);
  EXPECT_TRUE(HABIT_TIMER.isRunning());
  HabitEventLog::RewardNotice notice;
  ASSERT_TRUE(HABIT_EVENTS.takeReward(notice));
  EXPECT_EQ(notice.grass, 3);
  advance(60);
  HABIT_TIMER.tick();
  HABIT_TIMER.stopAndLog("duration");
  EXPECT_EQ(HABIT_EVENTS.progressForToday("duration").durationSeconds, 120);
  EXPECT_EQ(SHEEP_STATE.getGrassStock(), 6);
  EXPECT_FALSE(HABIT_EVENTS.takeReward(notice));
}

TEST_F(HabitSheepTest, PastReadingDayCreditsHistoryAndDoesNotRewardAgain) {
  date(2);
  ASSERT_TRUE(HABIT_EVENTS.appendDurationSecondsOnDay("duration", 30, "2026-10-01"));
  EXPECT_EQ(SHEEP_STATE.getGrassStock(), 3);
  ASSERT_TRUE(HABIT_EVENTS.appendDurationSecondsOnDay("duration", 30, "2026-10-01"));
  EXPECT_EQ(SHEEP_STATE.grassForDay(20261001).earned, 3);
  EXPECT_EQ(HABIT_EVENTS.progressForToday("duration").durationSeconds, 0);
  HabitEventLog::RewardNotice notice;
  ASSERT_TRUE(HABIT_EVENTS.takeReward(notice));
  ASSERT_TRUE(HABIT_EVENTS.appendDurationSecondsOnDay("duration", 60, "2026-10-01"));
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
  EXPECT_FALSE(HABIT_EVENTS.appendCompletion("completion"));
}

TEST_F(HabitSheepTest, WeeklyThreeCompletionsReplenishTwentyOneGrass) {
  auto habit = *HABIT_SHEEP.findHabit("completion");
  habit.period = HabitPeriod::Weekly;
  habit.targetCount = 3;
  ASSERT_TRUE(HABIT_SHEEP.upsertHabit(habit));
  for (int i = 0; i < 3; ++i) ASSERT_TRUE(HABIT_EVENTS.appendCompletion("completion"));
  EXPECT_EQ(SHEEP_STATE.getGrassStock(), 21);
  EXPECT_EQ(SHEEP_STATE.grassForDay(20261001).earned, 18);
  EXPECT_FALSE(HABIT_EVENTS.appendCompletion("completion"));
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
  EXPECT_EQ(HABIT_EVENTS.progressForToday("duration").durationSeconds, 60);
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

TEST_F(HabitSheepTest, WeeklyFourCompletionsApportionExactlyTwentyOneGrass) {
  stock(0);
  auto habit = *HABIT_SHEEP.findHabit("completion");
  habit.period = HabitPeriod::Weekly;
  habit.targetCount = 4;
  ASSERT_TRUE(HABIT_SHEEP.upsertHabit(habit));
  for (int i = 0; i < 4; ++i) ASSERT_TRUE(HABIT_EVENTS.appendCompletion("completion"));
  EXPECT_EQ(SHEEP_STATE.getGrassStock(), 21);
  EXPECT_EQ(SHEEP_STATE.grassForDay(20261001).earned, 21);
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
