# Habit Sheep — implemented screens and X3 verification

Updated 2026-10-03. The user authorized implementation of every agreed screen and pending functional item. PR #1 stays draft on `feature/habit-sheep-v1`; merging into develop and submitting upstream remain unauthorized.

## Current implementation

| Item | Result |
| --- | --- |
| Home | Date, minute clock, battery, three top habit icons, five mood hearts without a Mood label, grass stock, central sheep and original six-entry dock. |
| Dock, left to right | Continue reading, Browse files, Library, OPDS, Transfer, Settings. |
| Immediate tracker access | Select or tap grass stock on Home to open 14-day grass history. Settings → Habits retains the same entry. |
| Empty habit slots | Rounded dashed placeholders; focus/tap opens Choose habit. Nine saved habits, three active. |
| Sheep interactions | Selecting the sheep opens a double-line panel with Pet, Call and Play. Responses expire after ten seconds; foraging cannot be bypassed by calling. |
| Icons | 24 original monochrome icons, including Reading, Focus, Family, Relationship, Money, Phone and five general choices (Star, Flag, Target, Check, Sun). Device and web settings preserve choices. Older habits derive defaults. |
| Reading and timed habits | One cumulative total combines device reading, paper-book timing and manual minutes. Large current number, smaller `/ target min`, rounded progress and action rows. |
| Manual minutes | 5/10/15/20/30/45/60/Custom grid, cumulative total preview and confirmation. Custom uses the existing interval chooser. |
| Daily/weekly counts | Dedicated progress screen, week date range and confirmed Log one action. Week starts on any chosen weekday, default Monday. |
| Pomodoro | Manual break start; Skip short break immediately starts the next focus. Unified `min`. Stop/log confirmation. |
| Completion | Double-line positive notice, sheep, actual grass gain or honest full-stock/save-failure notice. Reading notices wait until Home. |
| Sheep pairs | Four visual sheep pairs distinguished by black/white faces and legs. No numbers. Access through Play; removed from Habits settings. |
| Grass presentation | `Grass 21 / 21` in text settings; icon plus spaced stock on Home. No decorative grass in sheep scenes, including empty-stock scenes. |
| Awake poses | Twelve poses, ten-minute intervals, no repeat within two hours. Missed meals override with rest; only zero mood shows the foraging sign. |
| Sleep selection | Display → Sleep Screen → Habit Sheep. No separate Sleep sheep scene setting. Date, battery without percentage, hearts below date, sheep, three passive habit progress icons. |
| X3 sleeping updates | RTC timer maintenance wake at half-hour boundaries, including 08:00/13:00/19:00 meals. Successful meals show eating for five minutes, then rest; missed meals rest and zero mood forages. Other sleep modes do not schedule these wakes. |
| Pause | Habit mode Off preserves history/timer time and freezes food/mood/bond decline. Home shows recent book; Cover becomes the default sleep mode with explicit user changes respected. Resume has no catch-up charges and leaves timer paused. |
| Input compatibility | Existing CrossPoint hardware hints/navigation; direct touch targets for habits, sheep, grass, actions, minute presets, icons and history paging. |

## Existing care rules preserved

Grass is the only resource. Cap 21 (seven days of food); fresh installations start with nine. Local meals at 08:00, 13:00 and 19:00 consume one each. Daily completions, duration targets and Pomodoro focuses earn three; weekly count targets below seven apportion 21 across their target completions (three/week gives seven each). Stock capacity limits actual gain, while completed habit events remain recorded.

Eating restores one of five mood hearts. A full day without food costs one heart, excluding paused days. Missing a meal causes rest; only zero hearts causes foraging. New grass returns the sheep and may use one remaining meal slot, never an extra fourth meal. There is no death or food debt. Interactions/game completion improve bond at most once per local date, provide behavioral feedback and never create grass.

History records actual earned/eaten grass, paused dates and dashes for dates without entries. It is an aggregate habit reinforcement ledger, not per-habit analytics. Existing stock and records survive upgrades; no invented historical rewards.

## P-012: freeze investigation — hardware verification still open

The earlier X3 freeze was not accompanied by reproduction steps or Serial logs, so its cause is not proven. The previous OptionPopup invoked its own stored std::function. Habit creation and confirmation callbacks can replace that same function while its closure is still executing. `invokePopupChoice` now moves the callback into a local owner before invoking it, preserving its captures and any newly installed callback. The real helper has a regression test for this replacement sequence. Evidence: `src/components/OptionPopup.h:107` and `:148` route the old touch/Confirm call sites through `src/components/PopupCallback.h`; creation callbacks in `src/activities/habits/HabitLibraryActivity.cpp` replace the popup during execution.

Habit Home, timer, count, icon, history, game and library loop mutations now share RenderLock with rendering. Activity-result handlers use the same lock. Popup rows are held in the existing popup object and paginate to fit landscape instead of selecting off-screen entries. No hardware freeze resolution is claimed until the X3 test below passes.

## X3 verification checklist

1. Navigate all three top slots, grass, sheep and six dock entries; test an empty slot and long-press replacement. Check portrait, inverted portrait and both landscapes.
2. Open grass history directly from Home. Check both seven-day pages, earned/eaten amounts, paused markers and the current stock. Preserve existing records after firmware update.
3. Create/edit a custom habit and select icons on both pages. Restart; confirm choices survive. Test Family/Relationship/Money/Phone and generic icons.
4. Combine device reading, paper-book timer and confirmed manual additions. Verify one total, Cancel has no effect, target reward is only once, and timing may continue beyond target.
5. Complete daily and weekly count habits several times. Verify date range and Monday/default or a changed boundary. Check positive/full-stock notices.
6. Finish a Pomodoro, leave the break waiting, then Skip break. Repeat during a running short break. The next focus starts immediately at zero, with no break reward.
7. Use Pet/Call/Play repeatedly, create/rename/delete habits and open chained confirmations. Play several rounds, return Home and read for at least thirty minutes. If it freezes again, record the last screen/action and capture Serial panic/backtrace, free heap and largest block if available.
8. Leave Home awake for two hours: twelve ten-minute poses, hearts without Mood, no drawn grass. A Pet/Call response returns to the current pose after ten seconds.
9. Select Habit Sheep sleep mode before a meal. Check exactly one grass deducted, five-minute eating scene, then rest. Leave asleep across half-hour boundaries and wake manually. Compare overnight battery use with Cover sleep mode; scheduled wakes are X3-specific and require device validation.
10. With no stock, verify missed meals show rest while hearts remain positive; zero hearts shows the foraging sign. Earn grass to return the sheep. Pause/resume and confirm there are no catch-up meal charges or automatic timer resume.

## Engineering and validation

Artwork uses committed SVG/PBM sources and a stdlib-only PlatformIO generator. Packed one-bit artwork stays in flash and scales as horizontal runs into the existing framebuffer; no new framebuffer or render-time bitmap allocation. New activities use fallible ActivityManager-owned screen-lifetime allocation. Icon selection retains one copied habit until its result returns. Popup row storage belongs to the popup rather than the render-task stack.

Automated tests compile the real stores, event log and timer against deterministic HAL/storage, and the real popup callback and scene scheduling helpers. Final CI/build status and firmware provenance are attached to PR #1. Actual battery behavior, e-ink refresh quality, button/touch usability and the reported freeze require the user's X3 test.

AI, sync, further games/interactions and per-habit analytics have no approved specification and remain future scope. The older three-row interaction concept was superseded by three top habits and the sheep popup, so no redundant top interaction row is added.
