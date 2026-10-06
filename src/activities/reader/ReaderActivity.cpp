#include "ReaderActivity.h"

#include <FontCacheManager.h>
#include <FsHelpers.h>
#include <HalClock.h>
#include <HalStorage.h>
#include <Memory.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <ctime>

#include "CrossPointSettings.h"
#include "CrossPointState.h"
#include "EpubReaderActivity.h"
#include "HabitDayTime.h"
#include "HabitEventLog.h"
#include "HabitSheepStore.h"
#include "ReaderUtils.h"
#include "RecentBooksStore.h"
#include "SdCardFontSystem.h"
#include "TxtReaderActivity.h"
#include "XtcReaderActivity.h"

ReaderActivity::ReaderActivity(const char* name, GfxRenderer& renderer, MappedInputManager& mappedInput,
                               std::string bookPath, const bool allowFastInitialRefresh)
    : Activity(name, renderer, mappedInput), bookPath(std::move(bookPath)) {
  if (allowFastInitialRefresh) {
    const int refreshFrequency = SETTINGS.getRefreshFrequency();
    pagesUntilFullRefresh = refreshFrequency > 1 ? refreshFrequency : 2;
  }
}

std::unique_ptr<ReaderActivity> ReaderActivity::create(GfxRenderer& renderer, MappedInputManager& mappedInput,
                                                       std::string path, const bool allowFastInitialRefresh) {
  // ActivityManager requires heap ownership; each branch allocates exactly one screen-lifetime object.
  std::unique_ptr<ReaderActivity> activity;
  if (FsHelpers::hasXtcExtension(path)) {
    activity = makeUniqueNoThrow<XtcReaderActivity>(renderer, mappedInput, std::move(path), allowFastInitialRefresh);
  } else if (FsHelpers::hasTxtExtension(path) || FsHelpers::hasMarkdownExtension(path)) {
    activity = makeUniqueNoThrow<TxtReaderActivity>(renderer, mappedInput, std::move(path), allowFastInitialRefresh);
  } else {
    activity = makeUniqueNoThrow<EpubReaderActivity>(renderer, mappedInput, std::move(path), allowFastInitialRefresh);
  }

  if (!activity) {
    LOG_ERR("READER", "OOM: reader activity");
  }
  return activity;
}

void ReaderActivity::applyInitialOrientation() { ReaderUtils::applyOrientation(renderer, SETTINGS.orientation); }

void ReaderActivity::disableFastInitialRefresh() { pagesUntilFullRefresh = 0; }

void ReaderActivity::onEnter() {
  Activity::onEnter();

  // Heap ledger for field crash reports: free vs largest block distinguishes a
  // leak (free falls) from fragmentation (free stable, largest collapses).
  LOG_INF("MEM", "reader enter: free=%u max_block=%u", (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getMaxAllocHeap());

  if (!Storage.exists(bookPath.c_str())) {
    LOG_ERR("READER", "File does not exist: %s", bookPath.c_str());
    finish();
    return;
  }

  // Clear remembered book after opening it
  if (!APP_STATE.openEpubPath.empty()) {
    APP_STATE.openEpubPath.clear();
    APP_STATE.saveToFile();
  }

  sdFontSystem.ensureLoaded(renderer);
  applyInitialOrientation();

  if (!loadBook()) {
    finish();
    return;
  }

  readingTimeTracked = std::any_of(
      HABIT_SHEEP.getHabits().begin(), HABIT_SHEEP.getHabits().end(),
      [](const HabitDefinition& habit) { return habit.type == HabitType::Duration && habit.readingIntegration; });
  readingModeRevision = HABIT_SHEEP.getModeRevision();
  if (readingTimeTracked) {
    readingLastRecordedMs = millis();
    readingLastCheckMs = readingLastRecordedMs;
    struct tm local{};
    if (halClock.isAvailable() && halClock.localTime(local))
      strftime(readingDay, sizeof(readingDay), "%Y-%m-%d", &local);
  }

  requestUpdate();
}

void ReaderActivity::rememberBookOnceRendered() {
  if (bookRemembered || !pageRendered.load(std::memory_order_acquire)) return;
  bookRemembered = true;
  APP_STATE.openEpubPath = bookPath;
  APP_STATE.saveToFile();
  RECENT_BOOKS.addBook(bookPath, getBookTitle(), getBookAuthor(), getBookThumbBmpPath());
}

void ReaderActivity::onExit() {
  recordReadingTime(true);
  Activity::onExit();

  // Keep rebuildable font buffers from pinning the heap between reading sessions.
  if (auto* fontCache = renderer.getFontCacheManager()) {
    fontCache->releaseSdFontCaches();
  }

  LOG_INF("MEM", "reader exit: free=%u max_block=%u", (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getMaxAllocHeap());

  ReaderUtils::applyOrientation(renderer, HABIT_SHEEP.getOrientation());
  APP_STATE.readerActivityLoadCount = 0;
  APP_STATE.saveToFile();

  endOfBookOptions.reset();
  endOfBookOptionsReady.store(false, std::memory_order_release);
}

void ReaderActivity::recordReadingTime(const bool force) {
  if (!readingTimeTracked) return;
  if (!HABIT_SHEEP.isEnabled() || readingModeRevision != HABIT_SHEEP.getModeRevision()) {
    readingModeRevision = HABIT_SHEEP.getModeRevision();
    readingLastRecordedMs = millis();
    readingLastCheckMs = readingLastRecordedMs;
    readingDay[0] = '\0';
    return;
  }
  const unsigned long now = millis();
  if (!force && now - readingLastCheckMs < 1000) return;
  readingLastCheckMs = now;

  struct tm local{};
  if (halClock.isAvailable() && halClock.localTime(local)) {
    char today[sizeof(readingDay)];
    strftime(today, sizeof(today), "%Y-%m-%d", &local);
    if (*readingDay && strcmp(today, readingDay) != 0) {
      const uint32_t elapsed = now - readingLastRecordedMs;
      const uint32_t newMs = habitMillisAfterMidnight(elapsed, local);
      const uint32_t seconds = (elapsed - newMs) / 1000;
      if (seconds > 0) {
        const auto& habits = HABIT_SHEEP.getHabits();
        if (std::any_of(habits.begin(), habits.end(), [&](const auto& habit) {
              return habit.type == HabitType::Duration && habit.readingIntegration &&
                     !HABIT_EVENTS.appendDurationSecondsOnDay(habit.id, seconds, readingDay);
            }))
          return;
      }
      readingLastRecordedMs = now - newMs;
      snprintf(readingDay, sizeof(readingDay), "%s", today);
    } else if (!*readingDay) {
      snprintf(readingDay, sizeof(readingDay), "%s", today);
    }
  }

  const uint32_t seconds = (now - readingLastRecordedMs) / 1000;
  if (seconds < 60 && !force) return;
  if (seconds == 0) return;
  const auto& habits = HABIT_SHEEP.getHabits();
  if (std::any_of(habits.begin(), habits.end(), [&](const auto& habit) {
        return habit.type == HabitType::Duration && habit.readingIntegration &&
               !HABIT_EVENTS.appendDurationSeconds(habit.id, seconds, HabitEventSource::Reader);
      }))
    return;
  readingLastRecordedMs += seconds * 1000;
}

bool ReaderActivity::handleBackNavigation() {
  return ReaderUtils::handleBackNavigation(mappedInput, activityManager, bookPath.c_str(),
                                           {this, [](void* ctx) { static_cast<ReaderActivity*>(ctx)->onGoHome(); }});
}

void ReaderActivity::clearEndOfBookOptionsIfNeeded() {
  if (isAtEndOfBook() || !endOfBookOptionsReady.load(std::memory_order_acquire)) return;

  RenderLock lock(*this);
  endOfBookOptionsReady.store(false, std::memory_order_release);
  endOfBookOptions.reset();
}

bool ReaderActivity::endOfBookMenuActive() const {
  return isAtEndOfBook() && endOfBookOptionsReady.load(std::memory_order_acquire) && endOfBookOptions->menuActive();
}

bool ReaderActivity::handleEndOfBookMenu(const bool suppressConfirmRelease) {
  if (suppressConfirmRelease || !endOfBookMenuActive()) {
    return false;
  }

  std::string openPath;
  switch (endOfBookOptions->handleMenuInput(mappedInput, &openPath)) {
    case EndOfBookOptions::Action::OpenBook:
      activityManager.goToReader(openPath);
      return true;
    case EndOfBookOptions::Action::GoHome:
      onGoHome();
      return true;
    case EndOfBookOptions::Action::LastPage:
      onReturnFromEndOfBook();
      requestUpdate();
      return true;
    case EndOfBookOptions::Action::Redraw:
      requestUpdate();
      return true;
    case EndOfBookOptions::Action::None:
      return false;
  }

  return false;
}

bool ReaderActivity::handleEndOfBookPageTurn(const bool prevTriggered, const bool nextTriggered) {
  if (!isAtEndOfBook()) return false;

  if (endOfBookOptionsReady.load(std::memory_order_acquire) && endOfBookOptions->menuActive()) {
    return true;
  }
  if (nextTriggered) {
    onGoHome();
  } else if (prevTriggered) {
    onReturnFromEndOfBook();
    requestUpdate();
  }
  return true;
}

void ReaderActivity::loop() {
  recordReadingTime();
  rememberBookOnceRendered();
  clearEndOfBookOptionsIfNeeded();
  if (handleEndOfBookMenu()) return;
  if (handleFormatInput()) return;
  if (handleBackNavigation()) return;

  const auto touch = ReaderUtils::detectTouchPageTurn(renderer, mappedInput);
  auto [prevTriggered, nextTriggered, fromTilt] = ReaderUtils::detectPageTurn(mappedInput);
  prevTriggered = prevTriggered || touch.prev;
  nextTriggered = nextTriggered || touch.next;
  if (!prevTriggered && !nextTriggered) return;
  if (handleEndOfBookPageTurn(prevTriggered, nextTriggered)) return;

  const unsigned long heldMs = (touch.prev || touch.next) ? touch.heldMs : mappedInput.getHeldTime();
  const bool skip =
      !fromTilt && SETTINGS.longPressButtonBehavior == SETTINGS.CHAPTER_SKIP && heldMs >= ReaderUtils::SKIP_HOLD_MS;

  if (prevTriggered) {
    if (skip) {
      skipPages(-10);
    } else {
      pageTurn(false);
    }
  } else {
    if (skip) {
      skipPages(10);
    } else {
      pageTurn(true);
    }
  }
  requestUpdate();
}

void ReaderActivity::render(RenderLock&&) {
  if (isAtEndOfBook()) {
    if (!endOfBookOptions) {
      endOfBookOptions = makeUniqueNoThrow<EndOfBookOptions>(renderer);
      if (!endOfBookOptions) LOG_ERR("READER", "OOM: EndOfBookOptions");
    }
    renderer.clearScreen();
    if (endOfBookOptions) {
      endOfBookOptions->loadOnce(bookPath);
      // Release-publish AFTER loadOnce() so the main task's acquire load can't
      // observe an object whose names/selector are still being populated.
      endOfBookOptionsReady.store(true, std::memory_order_release);
      endOfBookOptions->render(renderer, mappedInput);
    }
    renderer.displayBuffer();
    onEndOfBookRendered();
    markPageRendered();
    return;
  }

  renderBook();
}

bool ReaderActivity::handleForcedRefresh() {
  {
    RenderLock lock(*this);
    pagesUntilFullRefresh = 1;
    forcedRefreshPending = true;
  }
  requestUpdate();
  return true;
}
