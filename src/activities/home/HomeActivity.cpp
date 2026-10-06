#include "HomeActivity.h"

#include <Bitmap.h>
#include <Epub.h>
#include <FsHelpers.h>
#include <GfxRenderer.h>
#include <HalDisplay.h>
#include <HalStorage.h>
#include <I18n.h>
#include <LibraryBuilder.h>
#include <LibraryIndexFile.h>
#include <Memory.h>
#include <Utf8.h>
#include <Xtc.h>
#include <esp_system.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <vector>

#include "CrossPointSettings.h"
#include "CrossPointState.h"
#include "HabitEventLog.h"
#include "HabitSheepStore.h"
#include "HabitTimer.h"
#include "MappedInputManager.h"
#include "OpdsServerStore.h"
#include "RecentBooksStore.h"
#include "SheepScene.h"
#include "SheepStateStore.h"
#include "activities/habits/GrassHistoryActivity.h"
#include "activities/habits/HabitCountActivity.h"
#include "activities/habits/HabitDurationActivity.h"
#include "activities/habits/SheepMemoryActivity.h"
#include "activities/habits/SheepPuzzleActivity.h"
#include "activities/reader/ReaderUtils.h"
#include "components/HabitReward.h"
#include "components/UITheme.h"
#include "fontIds.h"

int HomeActivity::getMenuItemCount() const {
  int count = 4;  // File Browser, Library, File transfer, Settings
  if (!recentBooks.empty()) {
    count += recentBooks.size();
  }
  if (hasOpdsServers) {
    count++;
  }
  return count;
}

void HomeActivity::loadRecentBooks(int maxBooks) {
  recentBooks.clear();
  const auto& books = RECENT_BOOKS.getBooks();
  recentBooks.reserve(coverGridUi ? maxBooks : std::min(static_cast<int>(books.size()), maxBooks));

  for (const RecentBook& book : books) {
    // Limit to maximum number of recent books
    if (recentBooks.size() >= maxBooks) {
      break;
    }

    // Skip if file no longer exists
    if (RecentBooksStore::isMissing(book)) {
      continue;
    }

    recentBooks.push_back(book);
  }
}

void HomeActivity::fillCoverGridFromLibrary() {
  if (recentBooks.size() >= CoverGridHomeUi::MAX_BOOKS) return;
  // Keep the index and record together off the task stack; reuse for every row.
  struct LibraryReader {
    library::LibraryIndexFile index;
    library::ClixRecord record;
  };
  auto reader = makeUniqueNoThrow<LibraryReader>();
  if (!reader) {
    LOG_ERR("HOME", "OOM: library index");
    return;
  }
  auto& index = reader->index;
  auto& record = reader->record;
  if (!index.open(library::libraryIndexPath())) {
    index.close();
    GUI.drawPopup(renderer, tr(STR_LIBRARY_REBUILDING));
    library::BuildStats stats;
    if (!library::buildLibraryIndex("/", stats, SETTINGS.libraryUseMetadata != 0) ||
        !index.open(library::libraryIndexPath())) {
      LOG_ERR("HOME", "Cannot populate cover grid from library");
      return;
    }
  }
  for (uint16_t row = 0; row < index.bookCount() && recentBooks.size() < CoverGridHomeUi::MAX_BOOKS; ++row) {
    RecentBook book;
    if (!index.readRecord(index.ordinalForRow(library::SortOrder::RecentDesc, row), record) ||
        !index.readPath(record, book.path))
      continue;
    if (std::any_of(recentBooks.begin(), recentBooks.end(),
                    [&](const RecentBook& existing) { return existing.path == book.path; }) ||
        RecentBooksStore::isMissing(book))
      continue;
    if (!index.readTitle(record, book.title) && !index.readName(record, book.title)) continue;
    index.readAuthor(record, book.author);
    if (index.ioFailed()) break;
    recentBooks.push_back(std::move(book));
  }
}

void HomeActivity::resolveGridCoverPaths() {
  for (auto& book : recentBooks) {
    if (!book.coverBmpPath.empty()) continue;
    // Constructors only derive cache paths; no metadata parsing or image generation.
    // Keep these large objects off the task stack and release each before the next book.
    if (FsHelpers::hasEpubExtension(book.path)) {
      auto epub = makeUniqueNoThrow<Epub>(book.path, "/.crosspoint");
      if (!epub) {
        LOG_ERR("HOME", "OOM: EPUB thumbnail path");
        continue;
      }
      book.coverBmpPath = epub->getThumbBmpPath();
    } else if (FsHelpers::hasXtcExtension(book.path)) {
      auto xtc = makeUniqueNoThrow<Xtc>(book.path, "/.crosspoint");
      if (!xtc) {
        LOG_ERR("HOME", "OOM: XTC thumbnail path");
        continue;
      }
      book.coverBmpPath = xtc->getThumbBmpPath();
    }
  }
}

void HomeActivity::loadGridCover(RecentBook& book, int height, bool& showingLoading, Rect& popupRect) {
  if (!book.coverBmpPath.empty() && Storage.exists(UITheme::getCoverThumbPath(book.coverBmpPath, height).c_str()))
    return;
  // Only one parser lives at a time; EPUB/XTC objects exceed the stack budget.
  if (FsHelpers::hasEpubExtension(book.path)) {
    auto epub = makeUniqueNoThrow<Epub>(book.path, "/.crosspoint");
    if (!epub) {
      LOG_ERR("HOME", "OOM: cover EPUB");
      return;
    }
    book.coverBmpPath = epub->getThumbBmpPath();
    if (Storage.exists(epub->getThumbBmpPath(height).c_str())) return;
    if (!showingLoading) {
      showingLoading = true;
      popupRect = GUI.drawPopup(renderer, tr(STR_LOADING_POPUP));
      GUI.fillPopupProgress(renderer, popupRect, 0);
    }
    if (epub->generateThumbBmpFromSource(height)) {
      return;
    }
  } else if (FsHelpers::hasXtcExtension(book.path)) {
    auto xtc = makeUniqueNoThrow<Xtc>(book.path, "/.crosspoint");
    if (!xtc) {
      LOG_ERR("HOME", "OOM: cover XTC");
      return;
    }
    book.coverBmpPath = xtc->getThumbBmpPath();
    if (Storage.exists(xtc->getThumbBmpPath(height).c_str())) return;
    if (!showingLoading) {
      showingLoading = true;
      popupRect = GUI.drawPopup(renderer, tr(STR_LOADING_POPUP));
      GUI.fillPopupProgress(renderer, popupRect, 0);
    }
    if (xtc->load() && xtc->generateThumbBmp(height)) {
      return;
    }
  }
  book.coverBmpPath.clear();
}

void HomeActivity::loadRecentCovers(int coverHeight) {
  recentsLoading = true;
  bool showingLoading = false;
  Rect popupRect;

  int progress = 0;
  for (RecentBook& book : recentBooks) {
    // The cover grid shares one slot size; generating at any other height
    // would rescale the dithered thumb at draw time and alias badly.
    const int thumbHeight = coverGridUi ? coverGridUi->thumbHeightFor() : coverHeight;
    if (coverGridUi) {
      loadGridCover(book, thumbHeight, showingLoading, popupRect);
      ++progress;
      if (showingLoading) GUI.fillPopupProgress(renderer, popupRect, progress * 100 / recentBooks.size());
      continue;
    }
    if (!book.coverBmpPath.empty()) {
      std::string coverPath = UITheme::getCoverThumbPath(book.coverBmpPath, thumbHeight);
      if (!Storage.exists(coverPath.c_str())) {
        // If epub, try to load the metadata for title/author and cover
        if (FsHelpers::hasEpubExtension(book.path)) {
          Epub epub(book.path, "/.crosspoint");
          // Skip loading css since we only need metadata here
          epub.load(false, true);

          // Try to generate thumbnail image for Continue Reading card
          if (!showingLoading) {
            showingLoading = true;
            popupRect = GUI.drawPopup(renderer, tr(STR_LOADING_POPUP));
          }
          GUI.fillPopupProgress(renderer, popupRect, 10 + progress * (90 / recentBooks.size()));
          bool success = epub.generateThumbBmp(thumbHeight);
          if (!success) {
            RECENT_BOOKS.updateBook(book.path, book.title, book.author, "");
            book.coverBmpPath = "";
          }
          coverRendered = false;
          requestUpdate();
        } else if (FsHelpers::hasXtcExtension(book.path)) {
          // Handle XTC file
          Xtc xtc(book.path, "/.crosspoint");
          if (xtc.load()) {
            // Try to generate thumbnail image for Continue Reading card
            if (!showingLoading) {
              showingLoading = true;
              popupRect = GUI.drawPopup(renderer, tr(STR_LOADING_POPUP));
            }
            GUI.fillPopupProgress(renderer, popupRect, 10 + progress * (90 / recentBooks.size()));
            bool success = xtc.generateThumbBmp(thumbHeight);
            if (!success) {
              RECENT_BOOKS.updateBook(book.path, book.title, book.author, "");
              book.coverBmpPath = "";
            }
            coverRendered = false;
            requestUpdate();
          }
        }
      }
    }
    progress++;
  }

  recentsLoaded = true;
  recentsLoading = false;
}

void HomeActivity::onEnter() {
  ReaderUtils::applyOrientation(renderer, HABIT_SHEEP.getOrientation());
  Activity::onEnter();

  hasOpdsServers = OPDS_STORE.hasServers();

  const auto& metrics = UITheme::getInstance().getMetrics();

  // Habit Sheep is the default home for this fork. Keep it as a screen-lifetime
  // component owned by HomeActivity so ActivityManager and upstream Home
  // semantics stay unchanged. Fall back to the upstream home on allocation failure.
  habitSheepUi = makeUniqueNoThrow<HabitSheepHomeUi>(renderer);
  if (habitSheepUi) {
    habitReplacementPopup.setHabitStyle();
    loadRecentBooks(1);
    hasContinueReading = !recentBooks.empty();
    if (!HABIT_SHEEP.isEnabled())
      loadRecentCovers(std::min(260, static_cast<int>(renderer.getScreenHeight()) - 74 - 52 - 110));
    HABIT_EVENTS.refreshToday();
    SHEEP_STATE.settleDay();
    lastHabitProgressStamp = UINT32_MAX;
    lastHabitModeRevision = HABIT_SHEEP.getModeRevision();
    selectorIndex = mappedInput.hasTouch() ? 0 : HABIT_SHEEP.homeSelection(hasContinueReading);
    requestUpdate();
    return;
  }
  LOG_ERR("HABIT", "OOM: Habit Sheep home; using upstream home");

  if (UITheme::getInstance().hasCoverGridHome()) {
    // Screen-lifetime interaction tables and component properties exceed the stack budget.
    coverGridUi = makeUniqueNoThrow<CoverGridHomeUi>(renderer);
    if (!coverGridUi) LOG_ERR("HOME", "OOM: cover grid UI; using standard home");
  }
  loadRecentBooks(coverGridUi ? CoverGridHomeUi::MAX_BOOKS : metrics.homeRecentBooksCount);
  hasContinueReading = !recentBooks.empty();
  if (coverGridUi) {
    fillCoverGridFromLibrary();
    resolveGridCoverPaths();
    coverGridUi->begin(recentBooks, hasOpdsServers, hasContinueReading);
  }

  const auto base = static_cast<int>(recentBooks.size());
  selectorIndex = initialMenuItem == HomeMenuItem::NONE ? 0 : base + menuItemToIndex(initialMenuItem, hasOpdsServers);

  // Trigger first update
  requestUpdate();
}

void HomeActivity::onExit() {
  Activity::onExit();

  habitSheepUi.reset();
  coverGridUi.reset();

  // Free the stored cover buffer if any
  freeCoverBuffer();
}

bool HomeActivity::storeCoverBuffer() {
  // render() must have already set the cover rect; without it we'd be back to
  // cloning the whole framebuffer.
  if (coverRectW <= 0 || coverRectH <= 0) return false;
  freeCoverBuffer();
  const size_t needed = renderer.getRegionByteSize(coverRectX, coverRectY, coverRectW, coverRectH);
  if (needed == 0) return false;
  coverBuffer = static_cast<uint8_t*>(malloc(needed));
  if (!coverBuffer) {
    LOG_ERR("HOME", "OOM: cover buffer (%u bytes)", (unsigned)needed);
    return false;
  }
  coverBufferSize = needed;
  if (!renderer.copyRegionToBuffer(coverRectX, coverRectY, coverRectW, coverRectH, coverBuffer, coverBufferSize)) {
    free(coverBuffer);
    coverBuffer = nullptr;
    coverBufferSize = 0;
    return false;
  }
  return true;
}

bool HomeActivity::restoreCoverBuffer() {
  if (!coverBuffer || coverRectW <= 0 || coverRectH <= 0) return false;
  return renderer.copyBufferToRegion(coverRectX, coverRectY, coverRectW, coverRectH, coverBuffer, coverBufferSize);
}

void HomeActivity::freeCoverBuffer() {
  if (coverBuffer) {
    free(coverBuffer);
    coverBuffer = nullptr;
  }
  coverBufferSize = 0;
  coverBufferStored = false;
}

void HomeActivity::showHabitReplacementPicker(const int slot) {
  if (slot < 0 || slot >= static_cast<int>(HabitSheepStore::MAX_ACTIVE_HABITS)) return;

  std::vector<std::string> labels;
  habitReplacementIds.clear();
  labels.emplace_back("Empty slot");
  habitReplacementIds.emplace_back("");

  const auto& active = HABIT_SHEEP.getActiveHabitIds();
  for (const auto& habit : HABIT_SHEEP.getHabits()) {
    const bool usedElsewhere = std::any_of(active.begin(), active.end(),
                                           [&](const std::string& id) { return id == habit.id && id != active[slot]; });
    if (usedElsewhere) continue;
    labels.push_back(habit.name);
    habitReplacementIds.push_back(habit.id);
  }

  const auto currentIt = std::find(habitReplacementIds.begin() + 1, habitReplacementIds.end(), active[slot]);
  const int current = currentIt == habitReplacementIds.end()
                          ? 0
                          : static_cast<int>(std::distance(habitReplacementIds.begin(), currentIt));

  std::vector<const char*> options(labels.size());
  std::transform(labels.begin(), labels.end(), options.begin(), [](const std::string& label) { return label.c_str(); });
  habitReplacementPopup.show("Active habit", options.data(), static_cast<int>(options.size()), current,
                             [this, slot](const int selected) {
                               if (selected < 0 || selected >= static_cast<int>(habitReplacementIds.size())) return;
                               HABIT_SHEEP.setActiveHabit(slot, habitReplacementIds[selected]);
                               if (habitReplacementIds[selected].empty()) selectorIndex = 0;
                               requestUpdate();
                             });
  requestUpdate();
}

void HomeActivity::activateHabitSheepSelection() {
  if (!habitSheepUi) return;

  const auto action = habitSheepUi->actionForSelection(selectorIndex);
  switch (action) {
    case HabitSheepHomeUi::Action::Sheep:
      if (!HABIT_SHEEP.isEnabled()) {
        if (hasContinueReading && !recentBooks.empty())
          onSelectBook(recentBooks[0].path);
        else
          onFileBrowserOpen();
      } else if (!SHEEP_STATE.isForaging()) {
        SHEEP_STATE.recordInteraction();
        habitSheepUi->nudgeSheep(static_cast<uint8_t>(esp_random() % 4));
        requestUpdate();
      }
      break;
    case HabitSheepHomeUi::Action::Habit1:
    case HabitSheepHomeUi::Action::Habit2:
    case HabitSheepHomeUi::Action::Habit3: {
      const int slot = static_cast<int>(action) - static_cast<int>(HabitSheepHomeUi::Action::Habit1);
      const auto& active = HABIT_SHEEP.getActiveHabitIds();
      if (slot < 0 || slot >= static_cast<int>(active.size())) break;
      if (active[slot].empty()) {
        showHabitReplacementPicker(slot);
        break;
      }
      const HabitDefinition* habit = HABIT_SHEEP.findHabit(active[slot]);
      if (!habit) break;

      if (habit->type == HabitType::Completion) {
        auto detail = makeUniqueNoThrow<HabitCountActivity>(renderer, mappedInput, habit->id);
        if (detail) activityManager.pushActivity(std::move(detail));
      } else {
        auto detail = makeUniqueNoThrow<HabitDurationActivity>(renderer, mappedInput, habit->id);
        if (detail) activityManager.pushActivity(std::move(detail));
      }
      break;
    }
    case HabitSheepHomeUi::Action::ContinueReading:
      if (hasContinueReading && !recentBooks.empty()) {
        onSelectBook(recentBooks[0].path);
      } else {
        onFileBrowserOpen();
      }
      break;
    case HabitSheepHomeUi::Action::BrowseFiles:
      onFileBrowserOpen();
      break;
    case HabitSheepHomeUi::Action::Library:
      onLibraryOpen();
      break;
    case HabitSheepHomeUi::Action::Opds:
      onOpdsBrowserOpen();
      break;
    case HabitSheepHomeUi::Action::Transfer:
      onFileTransferOpen();
      break;
    case HabitSheepHomeUi::Action::Settings: {
      activityManager.goToSettings(4);
      break;
    }
    case HabitSheepHomeUi::Action::GrassHistory: {
      auto history = makeUniqueNoThrow<GrassHistoryActivity>(renderer, mappedInput);
      if (history) activityManager.pushActivity(std::move(history));
      break;
    }
    case HabitSheepHomeUi::Action::None:
      break;
  }
}

bool HomeActivity::preventAutoSleep() {
  if (!habitSheepUi || !HABIT_SHEEP.isEnabled()) return false;
  tm local{};
  return habitSheepUi->isInteracting() || (halClock.localTime(local) && SHEEP_STATE.ateCurrentMeal(local));
}

void HomeActivity::showSheepGames() {
  habitReplacementPopup.showGames(HABIT_SHEEP.getSheepName().c_str(), [this](int selected) {
    if (selected == 0) {
      auto game = makeUniqueNoThrow<SheepMemoryActivity>(renderer, mappedInput);
      if (game)
        activityManager.pushActivity(std::move(game));
      else
        LOG_ERR("HABIT", "OOM: Pairs");
    } else {
      auto game =
          makeUniqueNoThrow<SheepPuzzleActivity>(renderer, mappedInput, static_cast<SheepPuzzle::Mode>(selected - 1));
      if (game)
        activityManager.pushActivity(std::move(game));
      else
        LOG_ERR("HABIT", "OOM: sheep puzzle");
    }
  });
  requestUpdate();
}

void HomeActivity::loopHabitSheepHome() {
  if (!habitSheepUi) return;
  RenderLock lock;
  if (habitSheepUi->expireNudge()) requestUpdate();
  if (lastHabitModeRevision != HABIT_SHEEP.getModeRevision()) {
    lastHabitModeRevision = HABIT_SHEEP.getModeRevision();
    if (!HABIT_SHEEP.isEnabled()) {
      loadRecentBooks(1);
      hasContinueReading = !recentBooks.empty();
      loadRecentCovers(std::min(260, static_cast<int>(renderer.getScreenHeight()) - 74 - 52 - 110));
    }
    selectorIndex = mappedInput.hasTouch() ? 0 : HABIT_SHEEP.homeSelection(hasContinueReading);
    requestUpdate();
  }
  const bool mealChanged = SHEEP_STATE.settleDay();
  const bool clockChanged = habitClock.changed();
  if (mealChanged || clockChanged) requestUpdate();

  uint32_t progressStamp = 0;
  for (const auto& id : HABIT_SHEEP.getActiveHabitIds()) {
    if (id.empty()) continue;
    progressStamp = progressStamp * 31 + HABIT_TIMER.elapsedSecondsFor(id) / 60;
    progressStamp = progressStamp * 31 + static_cast<uint8_t>(HABIT_TIMER.phaseFor(id));
  }
  if (progressStamp != lastHabitProgressStamp) {
    lastHabitProgressStamp = progressStamp;
    requestUpdate();
  }

  if (habitReplacementPopup.isActive()) {
    habitReplacementPopup.handleInput(mappedInput, [this] { requestUpdate(); });
    return;
  }

  if (HABIT_SHEEP.isEnabled() && showHabitReward(habitReplacementPopup)) {
    requestUpdate();
    return;
  }

  const int longPressedSlot = habitSheepUi->longPressedHabit(mappedInput);
  if (longPressedSlot == 3) {
    selectorIndex = 0;
    showSheepGames();
    return;
  }
  if (longPressedSlot >= 0) {
    showHabitReplacementPicker(longPressedSlot);
    return;
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Up) ||
      mappedInput.wasReleased(MappedInputManager::Button::Left)) {
    selectorIndex = habitSheepUi->previousSelection(selectorIndex);
    requestUpdate();
    return;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Down) ||
      mappedInput.wasReleased(MappedInputManager::Button::Right)) {
    selectorIndex = habitSheepUi->nextSelection(selectorIndex);
    requestUpdate();
    return;
  }

  // Preserve CrossPoint's Home shortcut: Back resumes the most recent book.
  if (mappedInput.wasReleased(MappedInputManager::Button::Back) && hasContinueReading && !recentBooks.empty()) {
    onSelectBook(recentBooks[0].path);
    return;
  }

  const int touched = habitSheepUi->selectedAction(mappedInput);
  if (touched >= 0) {
    selectorIndex = touched;
    activateHabitSheepSelection();
    return;
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    activateHabitSheepSelection();
  }
}

void HomeActivity::loop() {
  if (habitSheepUi) {
    loopHabitSheepHome();
    return;
  }
  const int menuCount = getMenuItemCount();
  const auto& metrics = UITheme::getInstance().getMetrics();

  auto activateSelection = [this] {
    if (selectorIndex < recentBooks.size()) {
      onSelectBook(recentBooks[selectorIndex].path);
      return;
    }
    const int menuIndex = selectorIndex - static_cast<int>(recentBooks.size());
    switch (indexToMenuItem(menuIndex, hasOpdsServers)) {
      case HomeMenuItem::FILE_BROWSER:
        onFileBrowserOpen();
        break;
      case HomeMenuItem::LIBRARY:
        onLibraryOpen();
        break;
      case HomeMenuItem::OPDS_BROWSER:
        onOpdsBrowserOpen();
        break;
      case HomeMenuItem::FILE_TRANSFER:
        onFileTransferOpen();
        break;
      case HomeMenuItem::SETTINGS_MENU:
        onSettingsOpen();
        break;
      default:
        break;
    }
  };

  // Cover grid home splits navigation by button group (see below); the flat
  // next/previous cycle is for the classic list home only.
  if (!coverGridUi) {
    buttonNavigator.onNext([this, menuCount] {
      selectorIndex = ButtonNavigator::nextIndex(selectorIndex, menuCount);
      requestUpdate();
    });

    buttonNavigator.onPrevious([this, menuCount] {
      selectorIndex = ButtonNavigator::previousIndex(selectorIndex, menuCount);
      requestUpdate();
    });
  }

  const auto swipe = mappedInput.wasSwipe();
  if (swipe == MappedInputManager::SwipeDir::Up) {
    selectorIndex = ButtonNavigator::nextIndex(selectorIndex, menuCount);
    requestUpdate();
    return;
  }
  if (swipe == MappedInputManager::SwipeDir::Down) {
    selectorIndex = ButtonNavigator::previousIndex(selectorIndex, menuCount);
    requestUpdate();
    return;
  }

  // Back is otherwise unused on the home menu: open the most recently read
  // book directly (recentBooks is most-recent-first and already pruned of
  // files missing from the SD card).
  if (mappedInput.wasReleased(MappedInputManager::Button::Back) && hasContinueReading && !recentBooks.empty()) {
    onSelectBook(recentBooks[0].path);
    return;
  }

  if (coverGridUi) {
    const int touched = coverGridUi->selectedAction(mappedInput);
    if (touched >= 0 && touched < menuCount) {
      selectorIndex = touched;
      activateSelection();
      return;
    }
    if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
      activateSelection();
      return;
    }
    // Side page buttons walk the covers, front Left/Right walk the tabs
    // (selectorIndex is flat: books first, then the tab items). A press while
    // selection sits in the other band jumps into this band first.
    const int bookCount = static_cast<int>(recentBooks.size());
    const auto cycleBand = [this](const int base, const int count, const int dir) {
      if (count <= 0) return;
      int idx = selectorIndex - base;
      if (idx < 0 || idx >= count) {
        idx = dir > 0 ? 0 : count - 1;
      } else {
        idx = (idx + count + dir) % count;
      }
      selectorIndex = base + idx;
      requestUpdate();
    };
    buttonNavigator.onPressAndContinuous({MappedInputManager::Button::Up},
                                         [&cycleBand, bookCount] { cycleBand(0, bookCount, -1); });
    buttonNavigator.onPressAndContinuous({MappedInputManager::Button::Down},
                                         [&cycleBand, bookCount] { cycleBand(0, bookCount, +1); });
    buttonNavigator.onPressAndContinuous({MappedInputManager::Button::Left}, [&cycleBand, bookCount, menuCount] {
      cycleBand(bookCount, menuCount - bookCount, -1);
    });
    buttonNavigator.onPressAndContinuous({MappedInputManager::Button::Right}, [&cycleBand, bookCount, menuCount] {
      cycleBand(bookCount, menuCount - bookCount, +1);
    });
    return;
  }

  const int coverColumnCount = std::max(1, metrics.homeRecentBooksCount);
  const int recentCount = std::min(static_cast<int>(recentBooks.size()), coverColumnCount);
  const int coverColumnWidth = (renderer.getScreenWidth() - 2 * metrics.contentSidePadding) / coverColumnCount;
  int touchedBook = -1;
  const auto coverTouch = mappedInput.colTouch(touchedBook, metrics.contentSidePadding, coverColumnWidth, recentCount,
                                               metrics.homeTopPadding,
                                               metrics.homeTopPadding + metrics.homeCoverTileHeight, coverColumnWidth);
  if (coverTouch != MappedInputManager::RowTouch::None) {
    if (coverTouch == MappedInputManager::RowTouch::Down) {
      if (selectorIndex != touchedBook) {
        selectorIndex = touchedBook;
        requestUpdate();
      }
    } else {
      selectorIndex = touchedBook;
      activateSelection();
    }
    return;
  }

  const int menuTop = metrics.homeTopPadding + metrics.homeCoverTileHeight + metrics.homeMenuTopOffset;
  const int renderedMenuCount =
      menuCount - (metrics.homeContinueReadingInMenu ? 0 : static_cast<int>(recentBooks.size()));
  int menuRow = -1;
  // Row height from the theme, not the metrics table: RoundedRaff draws
  // font-derived rows and the touch grid must match the visuals exactly.
  const int menuRowHeight = GUI.getMenuRowHeight(renderer);
  const auto menuTouch = mappedInput.rowTouch(menuRow, menuTop, menuRowHeight + metrics.menuSpacing, renderedMenuCount,
                                              0, INT32_MAX, menuRowHeight);
  if (menuTouch != MappedInputManager::RowTouch::None) {
    const int touchedIndex =
        metrics.homeContinueReadingInMenu ? menuRow : menuRow + static_cast<int>(recentBooks.size());
    if (menuTouch == MappedInputManager::RowTouch::Down) {
      if (selectorIndex != touchedIndex) {
        selectorIndex = touchedIndex;
        requestUpdate();
      }
    } else {
      selectorIndex = touchedIndex;
      activateSelection();
    }
    return;
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    activateSelection();
  }
}

void HomeActivity::render(RenderLock&&) {
  if (habitSheepUi) {
    renderer.clearScreen();
    habitSheepUi->setSelection(selectorIndex);
    habitSheepUi->renderUi(HABIT_SHEEP, !habitReplacementPopup.isActive(),
                           recentBooks.empty() ? nullptr : &recentBooks[0]);
    if (habitReplacementPopup.processRender(renderer, mappedInput)) return;
    renderer.displayBuffer(cleanInitialRefresh && !firstRenderDone ? HalDisplay::HALF_REFRESH
                                                                   : HalDisplay::FAST_REFRESH);
    firstRenderDone = true;
    return;
  }
  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();

  renderer.clearScreen();
  if (coverGridUi) {
    coverGridUi->setSelection(selectorIndex);
    UITheme::getInstance().drawCoverGridHome(*coverGridUi);
    // Front Left/Right walk the tabs, so their hints read Left/Right; the
    // side page buttons (unhinted) walk the covers.
    const auto labels = mappedInput.mapLabels(hasContinueReading ? tr(STR_RESUME) : "", tr(STR_SELECT),
                                              tr(STR_DIR_LEFT), tr(STR_DIR_RIGHT));
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
    renderer.displayBuffer(cleanInitialRefresh && !firstRenderDone ? HalDisplay::HALF_REFRESH
                                                                   : HalDisplay::FAST_REFRESH);
    // Slot heights are recorded during the draw above; a change (first layout
    // pass, orientation switch) means the paths must point at those sizes and
    // any missing thumbs must be generated. Refreshing the paths right away
    // lets the next pass draw already-cached thumbs before generation runs.
    const bool coverSpecChanged = coverGridUi->takeThumbHeightChanged();
    if (coverSpecChanged) {
      coverGridUi->refreshCoverPaths();
      recentsLoaded = false;
    }
    if (!firstRenderDone) {
      firstRenderDone = true;
      requestUpdate();
    } else if (!recentsLoaded && !recentsLoading) {
      loadRecentCovers(CoverGridHomeUi::THUMB_HEIGHT);
      coverGridUi->refreshCoverPaths();
      requestUpdate();
    }
    return;
  }
  bool bufferRestored = coverBufferStored && restoreCoverBuffer();

  // Band spans topPadding..homeTopPadding: the cover tile starts at the fixed
  // homeTopPadding, so the height must shrink by topPadding or the band (and a
  // centered title, e.g. RoundedRaff's book title) sinks into the tile.
  // Home is the stack root: no back button in its header.
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.homeTopPadding - metrics.topPadding},
                 metrics.homeContinueReadingInMenu && !recentBooks.empty() ? recentBooks[0].title.c_str() : nullptr,
                 nullptr, false);

  // Record the tile rect so storeCoverBuffer (called from the theme) knows
  // which sub-region of the framebuffer to snapshot. ~16 KB in Portrait
  // instead of the 48 KB full framebuffer the previous bind captured.
  coverRectX = 0;
  coverRectY = metrics.homeTopPadding;
  coverRectW = pageWidth;
  coverRectH = metrics.homeCoverTileHeight;

  GUI.drawRecentBookCover(renderer, Rect{0, metrics.homeTopPadding, pageWidth, metrics.homeCoverTileHeight},
                          recentBooks, selectorIndex, coverRendered, coverBufferStored, bufferRestored,
                          std::bind(&HomeActivity::storeCoverBuffer, this));

  // Build menu items dynamically
  std::vector<const char*> menuItems = {tr(STR_BROWSE_FILES), tr(STR_LIBRARY), tr(STR_FILE_TRANSFER),
                                        tr(STR_SETTINGS_TITLE)};
  std::vector<UIIcon> menuIcons = {Folder, Library, Transfer, Settings};

  if (hasOpdsServers) {
    menuItems.insert(menuItems.begin() + 2, tr(STR_OPDS_BROWSER));
    menuIcons.insert(menuIcons.begin() + 2, Blocks);
  }

  if (metrics.homeContinueReadingInMenu && !recentBooks.empty()) {
    // Insert Continue Reading at the top if enabled in theme
    menuItems.insert(menuItems.begin(), tr(STR_CONTINUE_READING));
    menuIcons.insert(menuIcons.begin(), Book);
  }

  GUI.drawButtonMenu(
      renderer,
      Rect{0, metrics.homeTopPadding + metrics.homeCoverTileHeight + metrics.homeMenuTopOffset, pageWidth,
           pageHeight - (metrics.headerHeight + metrics.homeTopPadding + metrics.verticalSpacing +
                         metrics.homeMenuTopOffset + metrics.buttonHintsHeight)},
      static_cast<int>(menuItems.size()),
      metrics.homeContinueReadingInMenu ? selectorIndex : selectorIndex - recentBooks.size(),
      [&menuItems](int index) { return std::string(menuItems[index]); },
      [&menuIcons](int index) { return menuIcons[index]; });

  const auto labels = mappedInput.mapLabels(recentBooks.empty() ? "" : tr(STR_RESUME), tr(STR_SELECT), tr(STR_DIR_UP),
                                            tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  renderer.displayBuffer(cleanInitialRefresh && !firstRenderDone ? HalDisplay::HALF_REFRESH : HalDisplay::FAST_REFRESH);

  if (!firstRenderDone) {
    firstRenderDone = true;
    requestUpdate();
  } else if (!recentsLoaded && !recentsLoading) {
    recentsLoading = true;
    const int themeThumbHeight = GUI.homeCoverThumbHeight(renderer);
    loadRecentCovers(themeThumbHeight > 0 ? themeThumbHeight : metrics.homeCoverHeight);
  }
}

void HomeActivity::onSelectBook(const std::string& path) { activityManager.goToReader(path); }

void HomeActivity::onFileBrowserOpen() { activityManager.goToFileBrowser(); }

void HomeActivity::onLibraryOpen() { activityManager.goToLibrary(); }

void HomeActivity::onSettingsOpen() { activityManager.goToSettings(); }

void HomeActivity::onFileTransferOpen() { activityManager.goToFileTransfer(); }

void HomeActivity::onOpdsBrowserOpen() { activityManager.goToBrowser(); }
