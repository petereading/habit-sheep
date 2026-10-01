#include <HalClock.h>
#include <HalStorage.h>
#include <gtest/gtest.h>

#include <cstdlib>

#include "HabitEventLog.h"
#include "HabitSheepStore.h"
#include "HabitTimer.h"
#include "SheepStateStore.h"
#include "components/HabitClock.h"

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
    doc["schema"] = 3;
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

TEST_F(HabitSheepTest, DurationTargetAwardsWhileRunningOnlyOnce) {
  ASSERT_TRUE(HABIT_TIMER.start("duration"));
  advance(60);
  HABIT_TIMER.tick();
  EXPECT_EQ(HABIT_EVENTS.progressForToday("duration").durationSeconds, 60);
  EXPECT_EQ(SHEEP_STATE.getGrassStock(), 4);
  EXPECT_TRUE(HABIT_TIMER.isRunning());
  HabitEventLog::RewardNotice notice;
  ASSERT_TRUE(HABIT_EVENTS.takeReward(notice));
  EXPECT_EQ(notice.grass, 1);
  advance(60);
  HABIT_TIMER.tick();
  HABIT_TIMER.stopAndLog("duration");
  EXPECT_EQ(HABIT_EVENTS.progressForToday("duration").durationSeconds, 120);
  EXPECT_EQ(SHEEP_STATE.getGrassStock(), 4);
  EXPECT_FALSE(HABIT_EVENTS.takeReward(notice));
}

TEST_F(HabitSheepTest, PastReadingDayCreditsHistoryAndDoesNotRewardAgain) {
  date(2);
  ASSERT_TRUE(HABIT_EVENTS.appendDurationSecondsOnDay("duration", 30, "2026-10-01"));
  EXPECT_EQ(SHEEP_STATE.getGrassStock(), 3);
  ASSERT_TRUE(HABIT_EVENTS.appendDurationSecondsOnDay("duration", 30, "2026-10-01"));
  EXPECT_EQ(SHEEP_STATE.grassForDay(20261001).earned, 1);
  EXPECT_EQ(HABIT_EVENTS.progressForToday("duration").durationSeconds, 0);
  HabitEventLog::RewardNotice notice;
  ASSERT_TRUE(HABIT_EVENTS.takeReward(notice));
  ASSERT_TRUE(HABIT_EVENTS.appendDurationSecondsOnDay("duration", 60, "2026-10-01"));
  EXPECT_FALSE(HABIT_EVENTS.takeReward(notice));
}

TEST_F(HabitSheepTest, FullStockStillLogsCompletionAndZeroGainNotice) {
  stock(14);
  ASSERT_TRUE(HABIT_EVENTS.appendCompletion("completion"));
  EXPECT_EQ(HABIT_EVENTS.progressForToday("completion").completionCount, 1);
  EXPECT_EQ(SHEEP_STATE.getGrassStock(), 14);
  HabitEventLog::RewardNotice notice;
  ASSERT_TRUE(HABIT_EVENTS.takeReward(notice));
  EXPECT_EQ(notice.grass, 0);
  EXPECT_FALSE(HABIT_EVENTS.appendCompletion("completion"));
}

TEST_F(HabitSheepTest, WeeklyThreeCompletionsReplenishSevenGrass) {
  auto habit = *HABIT_SHEEP.findHabit("completion");
  habit.period = HabitPeriod::Weekly;
  habit.targetCount = 3;
  ASSERT_TRUE(HABIT_SHEEP.upsertHabit(habit));
  for (int i = 0; i < 3; ++i) ASSERT_TRUE(HABIT_EVENTS.appendCompletion("completion"));
  EXPECT_EQ(SHEEP_STATE.getGrassStock(), 10);
  EXPECT_EQ(SHEEP_STATE.grassForDay(20261001).earned, 7);
  EXPECT_FALSE(HABIT_EVENTS.appendCompletion("completion"));
}

TEST_F(HabitSheepTest, OfflineFeedingHasNoDebtAndReplenishmentReturnsSheep) {
  stock(3);
  date(20);
  ASSERT_TRUE(SHEEP_STATE.settleDay());
  EXPECT_EQ(SHEEP_STATE.getGrassStock(), 0);
  EXPECT_EQ(SHEEP_STATE.grassForDay(20261002).eaten, 1);
  EXPECT_EQ(SHEEP_STATE.grassForDay(20261004).eaten, 1);
  EXPECT_EQ(SHEEP_STATE.grassForDay(20261005).eaten, 0);
  EXPECT_FALSE(SHEEP_STATE.settleDay());
  EXPECT_EQ(SHEEP_STATE.addGrass(1), 1);
  EXPECT_EQ(SHEEP_STATE.getGrassStock(), 1);
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
  EXPECT_EQ(SHEEP_STATE.getGrassStock(), 14);
  EXPECT_EQ(SHEEP_STATE.getBondPoints(), 42);
  EXPECT_EQ(SHEEP_STATE.grassForDay(20261001).day, 0);
  ASSERT_TRUE(SHEEP_STATE.settleDay());
  EXPECT_EQ(SHEEP_STATE.getGrassStock(), 14);
}

TEST_F(HabitSheepTest, LedgerKeepsNewestFourteenDaysAndSurvivesJsonRoundTrip) {
  for (int i = 1; i <= 16; ++i) {
    date(i);
    SHEEP_STATE.addGrass(1);
  }
  EXPECT_EQ(SHEEP_STATE.grassForDay(20261001).day, 0);
  EXPECT_EQ(SHEEP_STATE.grassForDay(20261002).day, 0);
  EXPECT_EQ(SHEEP_STATE.grassForDay(20261003).earned, 1);
  EXPECT_EQ(SHEEP_STATE.grassForDay(20261016).eaten, 1);
  JsonDocument saved;
  SHEEP_STATE.toJson(saved);
  ASSERT_TRUE(SHEEP_STATE.fromJson(saved));
  EXPECT_EQ(SHEEP_STATE.grassForDay(20261016).earned, 1);
  SHEEP_STATE.addGrass(1, "2026-10-01");
  EXPECT_EQ(SHEEP_STATE.grassForDay(20261003).earned, 1);
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
