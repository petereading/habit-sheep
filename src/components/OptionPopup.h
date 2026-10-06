#pragma once
#include <I18n.h>

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <functional>
#include <string>
#include <vector>

#include "GfxRenderer.h"
#include "MappedInputManager.h"
#include "SheepStateStore.h"
#include "components/HabitUi.h"
#include "components/PopupCallback.h"
#include "components/UITheme.h"
#include "components/UiAppHelpers.h"
#include "fontIds.h"

// Modal option picker drawn over the current screen (no clear) via
// fui::optionDialog. Touch hit-testing is the SDK's InteractionBuffer: each
// render registers the option buttons (plus a chrome guard rect) on the render
// task, and handleInput routes touch snapshots against that table on the loop
// task, gated by the uiReady handshake (same pattern as UiListActivity).
// render() builds into InteractionBuffer's non-published generation
// (beginPublishCycle()) and publishes it only once every hit() call for the
// frame is done (publish()), so handleInput()'s routePublished()/
// publishedData() reads on the loop task always see a complete table, never
// one render is mid-rebuilding. uiReady closes when show() replaces the
// popup's data, then stays open across ordinary repaints after the first
// publication so a release cannot be dropped during a highlight repaint.
class OptionPopup {
 public:
  void setHabitStyle(bool value = true) { habitStyle = value; }
  void showGames(const char* sheepName, std::function<void(int)> callback) {
    const char* options[] = {tr(STR_SHEEP_PAIRS), tr(STR_SHEEP_TURN), tr(STR_SHEEP_REMEMBER), tr(STR_SHEEP_MAZE)};
    show(tr(STR_SHEEP_PLAY_WITH), options, 4, 0, std::move(callback));
    title += " ";
    title += sheepName && *sheepName ? sheepName : tr(STR_SHEEP_DEFAULT_NAME);
    gameMenu = true;
  }
  void showInteractions(const char* sheepName, std::function<void(int)> callback) {
    const char* options[] = {tr(STR_SHEEP_PET), tr(STR_SHEEP_CALL), tr(STR_SHEEP_PAIRS)};
    show(sheepName && *sheepName ? sheepName : tr(STR_SHEEP_INTERACT), options, 3, 0, std::move(callback));
    iconMenu = true;
  }
  void showMinuteChoices(std::function<void(int)> callback) {
    const char* options[] = {tr(STR_HABIT_PRESET_5),  tr(STR_HABIT_PRESET_10), tr(STR_HABIT_PRESET_15),
                             tr(STR_HABIT_PRESET_20), tr(STR_HABIT_PRESET_30), tr(STR_HABIT_PRESET_45),
                             tr(STR_HABIT_PRESET_60), tr(STR_HABIT_CUSTOM)};
    show(tr(STR_HABIT_ADD_MINUTES), options, 8, 2, std::move(callback));
    minuteMenu = true;
  }
  void show(StrId titleId, const StrId* optionIds, int optionCount, int currentIndex,
            std::function<void(int)> onSelect) {
    title = I18N.get(titleId);
    headline.clear();
    ownedStrings.resize(optionCount);
    for (int i = 0; i < optionCount; i++) {
      ownedStrings[i] = I18N.get(optionIds[i]);
    }
    activate(currentIndex, std::move(onSelect));
  }

  void show(const char* titleStr, const char* const* options, int optionCount, int currentIndex,
            std::function<void(int)> onSelect) {
    title = titleStr;
    headline.clear();
    ownedStrings.resize(optionCount);
    for (int i = 0; i < optionCount; i++) {
      ownedStrings[i] = options[i];
    }
    activate(currentIndex, std::move(onSelect));
  }

  // As above, plus a subject line inside the dialog (a book or event title).
  // It wraps to several lines under the caption; the dialog grows to fit.
  void show(const char* titleStr, const char* headlineStr, const char* const* options, int optionCount,
            int currentIndex, std::function<void(int)> onSelect) {
    show(titleStr, options, optionCount, currentIndex, std::move(onSelect));
    headline = headlineStr ? headlineStr : "";
  }

  void show(StrId titleId, const std::vector<std::string>& options, int currentIndex,
            std::function<void(int)> onSelect) {
    title = I18N.get(titleId);
    headline.clear();
    ownedStrings = options;
    activate(currentIndex, std::move(onSelect));
  }

  bool handleInput(MappedInputManager& input, const std::function<void()>& requestUpdate) {
    if (!active) return false;

    // Match the render cap: only the first MAX_OPTIONS rows exist on screen,
    // so button wrap-around must not select an invisible option.
    const int total = static_cast<int>(ownedStrings.size());
    const int count = total > MAX_OPTIONS ? MAX_OPTIONS : total;
    const freeink::ui::InputSnapshot snap = touchSnapshotFrom(input);
    if (snap.touchPressed || snap.touchReleased || snap.touchHeld) {
      // Interactions are registered on the render task; only route once the
      // first render after show() has populated the table (uiReady handshake).
      if (uiReady) {
        const freeink::ui::ActionEvent event = interactions.routePublished(snap);
        if (event && event.action == ACTION_PAGE) {
          selectedIndex = event.value;
          requestUpdate();
          return true;
        }
        if (event && event.action == ACTION_OPTION) {
          // Tap released on an option: select it, fire, dismiss.
          selectedIndex = event.value;
          active = false;
          invokePopupChoice(onSelectCallback, selectedIndex);
          requestUpdate();
          return true;
        }
        if (event && event.action == ACTION_CHROME) {
          // Taps on the dialog chrome (title, padding) keep the popup open.
          return true;
        }
        if (snap.touchReleased && snap.touchX >= 0) {
          // Tap released outside the dialog: dismiss without firing. Swipe-end
          // releases arrive with -1,-1 coords and fall through (no dismiss).
          active = false;
          requestUpdate();
          return true;
        }
        if (snap.touchPressed) {
          // Touch-down on an option moves the highlight (route() latched the
          // hit as the active interaction; read it back, no re-hit-testing).
          const int16_t idx = interactions.activeIndex();
          if (idx >= 0) {
            const freeink::ui::Interaction& hit = interactions.publishedData()[idx];
            if (hit.action == ACTION_OPTION && selectedIndex != hit.value) {
              selectedIndex = hit.value;
              requestUpdate();
            }
          }
        }
      }
      return true;
    }

    if (input.wasPressed(MappedInputManager::Button::NavPrevious)) {
      selectedIndex = (selectedIndex - 1 + count) % count;
      requestUpdate();
      return true;
    } else if (input.wasPressed(MappedInputManager::Button::NavNext)) {
      selectedIndex = (selectedIndex + 1) % count;
      requestUpdate();
      return true;
    } else if (input.wasReleased(MappedInputManager::Button::Confirm)) {
      active = false;
      invokePopupChoice(onSelectCallback, selectedIndex);
      requestUpdate();
      return true;
    } else if (input.wasReleased(MappedInputManager::Button::Back)) {
      active = false;
      requestUpdate();
      return true;
    }
    return true;
  }

  bool processRender(GfxRenderer& renderer, const MappedInputManager& input) const {
    if (!active) return false;
    const auto popupLabels = input.mapLabels(tr(STR_BACK), tr(STR_SELECT), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
    GUI.drawButtonHints(renderer, popupLabels.btn1, popupLabels.btn2, popupLabels.btn3, popupLabels.btn4);
    render(renderer);
    renderer.displayBuffer();
    return true;
  }

  void render(const GfxRenderer& renderer) const {
    if (!active) return;
    if (iconMenu || minuteMenu || gameMenu) {
      const Rect safe = UITheme::getInstance().getScreenSafeArea(renderer, true, false);
      const int w = safe.width - 24;
      const int h = std::min(minuteMenu ? 390 : gameMenu ? 280 : 220, safe.height - 24);
      const int x = safe.x + 12, y = safe.y + (safe.height - h) / 2;
      renderer.fillRoundedRect(x, y, w, h, 12, Color::White);
      habitUi::popupFrame(renderer, x, y, w, h);
      const auto shownTitle = renderer.truncatedText(NOTOSANS_14_FONT_ID, title.c_str(), w - 32);
      renderer.drawText(NOTOSANS_14_FONT_ID,
                        x + (w - renderer.getTextWidth(NOTOSANS_14_FONT_ID, shownTitle.c_str())) / 2, y + 14,
                        shownTitle.c_str());
      interactions.beginPublishCycle();
      auto target = makeUiTarget(renderer);
      const auto device = target.deviceContext();
      const freeink::ui::InputSnapshot noInput{};
      freeink::ui::Frame<INTERACTION_CAPACITY> frame(target, device, noInput, interactions);
      frame.hit(freeink::ui::Rect{static_cast<int16_t>(x), static_cast<int16_t>(y), static_cast<int16_t>(w),
                                  static_cast<int16_t>(h)},
                ACTION_CHROME, 0, freeink::ui::InputTouch);
      const int columns = minuteMenu || gameMenu ? 2 : 3,
                rows = minuteMenu ? 4
                       : gameMenu ? 2
                                  : 1,
                step = (w - 32) / columns;
      const int menuTop = 14 + renderer.getLineHeight(NOTOSANS_14_FONT_ID) + 12;
      const int tile = minuteMenu || gameMenu ? step - 10 : std::min(80, (w - 40) / 3);
      const int rowH = minuteMenu || gameMenu ? (h - menuTop - 24) / rows : 80;
      for (int i = 0; i < columns * rows; ++i) {
        const int px = x + 16 + (i % columns) * step + (step - tile) / 2, py = y + menuTop + (i / columns) * rowH;
        const int tileH = minuteMenu || gameMenu ? rowH - 8 : tile;
        habitUi::frame(renderer, px, py, tile, tileH, i == selectedIndex);
        if (minuteMenu)
          renderer.drawText(NOTOSANS_14_FONT_ID,
                            px + (tile - renderer.getTextWidth(NOTOSANS_14_FONT_ID, ownedStrings[i].c_str())) / 2,
                            py + (tileH - renderer.getLineHeight(NOTOSANS_14_FONT_ID)) / 2, ownedStrings[i].c_str());
        else if (gameMenu) {
          const int iconSize = std::clamp(std::min(tile / 3, tileH - 20), 40, 72);
          const int textX = px + 12 + iconSize + 12, textW = px + tile - 12 - textX;
          habitUi::gameIcon(renderer, i, px + 12, py + (tileH - iconSize) / 2, iconSize);
          const int textH = renderer.getLineHeight(SMALL_FONT_ID) * 2;
          UITheme::drawCenteredWrappedText(renderer, Rect{textX, py + (tileH - textH) / 2, textW, textH}, SMALL_FONT_ID,
                                           ownedStrings[i].c_str(), 2);
        } else
          habitUi::interaction(renderer, i, px + (tile - 48) / 2, py + (tile - 48) / 2, 48);
        frame.hit(freeink::ui::Rect{static_cast<int16_t>(px), static_cast<int16_t>(py), static_cast<int16_t>(tile),
                                    static_cast<int16_t>(tileH)},
                  ACTION_OPTION, i, freeink::ui::InputTouch);
      }
      if (iconMenu) renderer.drawCenteredText(SMALL_FONT_ID, y + h - 42, ownedStrings[selectedIndex].c_str());
      interactions.publish();
      uiReady = true;
      return;
    }
    namespace fui = freeink::ui;

    // Per-render target: a GfxRendererTarget is a renderer reference plus
    // three font ids, so rebuilding it here is trivially cheap and always
    // tracks the live orientation and uiScale fonts; a target held across
    // show() would stale-bind both after a rotation or scale change.
    fui::GfxRendererTarget target = makeUiTarget(renderer);
    const fui::ThemeTokens& theme = refreshSharedUiThemeTokens(target);
    // Frame stores a const DeviceContext&; keep it in a local that outlives
    // the frame (a deviceContext() temporary would dangle).
    const fui::DeviceContext device = target.deviceContext();
    // Routing happens on the loop task against the member buffer; the frame
    // itself never dispatches, so it gets an empty snapshot.
    const fui::InputSnapshot noInput{};

    // Builds into the generation handleInput()'s routePublished()/
    // publishedData() aren't currently reading, so the loop task never sees
    // this table mid-rebuild — see publish() below and
    // InteractionBuffer::beginPublishCycle().
    interactions.beginPublishCycle();
    fui::Frame<INTERACTION_CAPACITY> frame(target, device, noInput, interactions);

    const auto& metrics = UITheme::getInstance().getMetrics();
    const int totalOptions = static_cast<int>(ownedStrings.size());
    uint8_t count = static_cast<uint8_t>(totalOptions > MAX_OPTIONS ? MAX_OPTIONS : totalOptions);

    fui::OptionDialogProps props;
    props.title = title.c_str();
    props.headline = headline.empty() ? nullptr : headline.c_str();
    props.message = grassBadge ? rewardText : nullptr;
    props.contentHeight = grassBadge ? 100 : 0;
    props.options = optionRows;
    props.optionCount = count;
    props.verticalOptions = true;
    // Touch only: physical buttons stay on the legacy wrap/confirm path above,
    // so the buffer never competes with it for focus/confirm dispatch.
    props.inputMask = fui::InputTouch;
    props.titleText.font = fui::GfxRendererTarget::FONT_BODY;
    props.titleText.bold = true;
    props.titleText.align = fui::TextAlign::Center;
    // Captions like "Remove from Recent Books?" overflow the narrow portrait
    // dialog in one line; let them wrap and the panel grow.
    props.titleText.maxLines = 2;
    props.headlineText.font = fui::GfxRendererTarget::FONT_BODY;
    props.headlineText.align = fui::TextAlign::Center;
    props.headlineText.maxLines = 3;
    props.messageText = props.headlineText;
    props.buttonText.font = fui::GfxRendererTarget::FONT_BODY;
    const int16_t innerPadding = static_cast<int16_t>(metrics.optionPopupInnerPadding);
    props.padding = fui::Insets{innerPadding, innerPadding, innerPadding, innerPadding};
    props.gap = static_cast<int16_t>(metrics.optionPopupItemSpacing);
    // Rounded invert-fill themes use a black pill, not the default gray focus cursor.
    if (theme.listSelectionStyle == fui::SelectionStyle::InvertFill && theme.listRowRadius > 0) {
      props.buttonStyles = fui::defaultButtonStyles();
      props.buttonStyles.focused = props.buttonStyles.selected;
      fui::setStyleRadius(props.buttonStyles, theme.listRowRadius);
    }
    // defaultPopupStyles() has no border, so opt in using the per-theme frame metrics.
    props.styles = fui::defaultPopupStyles();
    props.styles.normal.border = fui::Paint::solid(fui::Color::Black);
    props.styles.normal.borderWidth = static_cast<uint8_t>(metrics.popupFrameThickness);
    props.styles.normal.radius = static_cast<uint8_t>(metrics.popupCornerRadius);
    props.styles.selected = props.styles.normal;
    props.styles.focused = props.styles.normal;
    props.styles.active = props.styles.normal;
    props.styles.disabled = props.styles.normal;
    props.buttonHeight =
        fui::clampI16(target.lineHeight(fui::GfxRendererTarget::FONT_BODY) + metrics.optionPopupSelectionVPadding * 2);

    // Fixed fraction of the screen, clamped by the theme's side margins; the
    // old max-text-width sizing is gone, long labels wrap inside the buttons.
    const fui::Rect screen = device.screen();
    const int16_t width =
        fui::clampI16(std::min<int>(screen.width * 3 / 4, screen.width - metrics.optionPopupDialogSideMargin * 2));
    // Keep every selectable row on screen, including nine-habit pickers in landscape.
    const auto populate = [&] {
      const int first = (selectedIndex / count) * count;
      props.optionCount = std::min<int>(count, std::min(totalOptions, MAX_OPTIONS) - first);
      for (int i = 0; i < props.optionCount; ++i) {
        const int index = first + i;
        optionRows[i].label = ownedStrings[index].c_str();
        optionRows[i].action = ACTION_OPTION;
        optionRows[i].value = static_cast<int16_t>(index);
        optionRows[i].state = index == selectedIndex ? fui::StateFocused : fui::StateNormal;
      }
    };
    populate();
    while (count > 1 &&
           fui::optionDialogHeight(target, props, width) > screen.height - metrics.buttonHintsHeight - 24) {
      --count;
      if (!grassBadge) props.contentHeight = 44;
      populate();
    }
    const int16_t height = fui::clampI16(fui::optionDialogHeight(target, props, width), 0, screen.height);
    const fui::Rect dialogRect = fui::centeredRect(screen, fui::Size{width, height});

    // Chrome guard first, options after: route() scans newest-first, so the
    // option buttons win inside the dialog and the guard absorbs the rest.
    frame.hit(dialogRect, ACTION_CHROME, 0, fui::InputTouch);
    const fui::Rect content = fui::optionDialog(frame, dialogRect, props);
    if (habitStyle)
      renderer.drawRoundedRect(dialogRect.x + 5, dialogRect.y + 5, dialogRect.width - 10, dialogRect.height - 10, 1, 8,
                               true);
    if (grassBadge) {
      habitUi::sheep(renderer, content.x + (content.width - 80) / 2, content.y, 80, 60, 2);
      char amount[20];
      if (grassGain)
        snprintf(amount, sizeof(amount), "+%u", static_cast<unsigned>(grassGain));
      else
        snprintf(amount, sizeof(amount), "%u / %u", static_cast<unsigned>(grassStock),
                 static_cast<unsigned>(SheepStateStore::GRASS_CAP));
      const int amountW = renderer.getTextWidth(NOTOSANS_14_FONT_ID, amount);
      const int x = content.x + (content.width - amountW - 38) / 2;
      const int y = content.y + 62;
      renderer.drawLine(x + 14, y + 32, x + 14, y + 4, 2, true);
      renderer.drawLine(x + 14, y + 23, x + 3, y + 13, 2, true);
      renderer.drawLine(x + 14, y + 16, x + 25, y + 7, 2, true);
      renderer.drawText(NOTOSANS_14_FONT_ID, x + 38, y + 4, amount);
    } else if (count < std::min(totalOptions, MAX_OPTIONS)) {
      const int first = (selectedIndex / count) * count;
      const int step = content.width / 2;
      for (int i = 0; i < 2; ++i) {
        const int px = content.x + i * step;
        renderer.drawText(SMALL_FONT_ID, px + 8, content.y + 8, i ? tr(STR_HABIT_NEXT) : tr(STR_HABIT_PREVIOUS));
        frame.hit(fui::Rect{static_cast<int16_t>(px), content.y, static_cast<int16_t>(step), 44}, ACTION_PAGE,
                  i ? std::min(std::min(totalOptions, MAX_OPTIONS) - 1, first + count) : std::max(0, first - count),
                  fui::InputTouch);
      }
    }
    // Atomically make this generation the one handleInput() reads, now that
    // every hit() call for this frame is done.
    interactions.publish();
    uiReady = true;
  }

  bool isActive() const { return active; }

  void showGrassReward(const char* habitName, const char* message, const uint16_t grass, const uint8_t stock,
                       const char* title = nullptr) {
    const char* options[] = {tr(STR_DONE)};
    show(title ? title : tr(STR_HABIT_REWARD_TITLE), habitName, options, 1, 0, [](int) {});
    snprintf(rewardText, sizeof(rewardText), "%s", message);
    grassBadge = true;
    grassGain = grass;
    grassStock = stock;
  }

  // Close without firing the callback (the surface under the popup is going
  // away, e.g. its host screen closes from outside the popup's own input).
  void dismiss() {
    active = false;
    onSelectCallback = nullptr;
  }

 private:
  // Bounded screen-lifetime row storage plus chrome and two touch paging targets.
  static constexpr int MAX_OPTIONS = 16;
  static constexpr size_t INTERACTION_CAPACITY = MAX_OPTIONS + 3;
  static constexpr freeink::ui::ActionId ACTION_OPTION = 1;
  static constexpr freeink::ui::ActionId ACTION_CHROME = 2;
  static constexpr freeink::ui::ActionId ACTION_PAGE = 3;

  void activate(int currentIndex, std::function<void(int)> onSelect) {
    grassBadge = false;
    iconMenu = false;
    minuteMenu = false;
    gameMenu = false;
    const int count = std::min<int>(ownedStrings.size(), MAX_OPTIONS);
    selectedIndex = currentIndex >= 0 && currentIndex < count ? currentIndex : 0;
    onSelectCallback = std::move(onSelect);
    uiReady = false;
    active = count > 0;
  }

  bool active = false;
  bool habitStyle = false;
  bool iconMenu = false;
  bool minuteMenu = false;
  bool gameMenu = false;
  bool grassBadge = false;
  uint16_t grassGain = 0;
  uint8_t grassStock = 0;
  char rewardText[80]{};
  std::string title;
  std::string headline;
  std::vector<std::string> ownedStrings;
  int selectedIndex = 0;
  std::function<void(int)> onSelectCallback;
  // Written by the render task (frame registration), routed by the loop task;
  // uiReady closes the rebuild window exactly like UiListActivity::uiReady.
  mutable freeink::ui::InteractionBuffer<INTERACTION_CAPACITY> interactions;
  // Screen-lifetime rows avoid a large array on the embedded render-task stack.
  mutable freeink::ui::DialogOption optionRows[MAX_OPTIONS]{};
  mutable std::atomic<bool> uiReady{false};
};
