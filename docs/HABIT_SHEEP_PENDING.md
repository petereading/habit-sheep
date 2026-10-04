# Habit Sheep — implemented screens and X3 verification

Updated 2026-10-04. The user authorized the initial X3 visual-feedback batch P-019–P-027, including the proposed short curved ground mark and using the sheep name as the interaction-window title. These changes are implemented for the next hardware test. PR #1 stays draft on `feature/habit-sheep-v1`; merging into develop and submitting upstream remain unauthorized.

## Current implementation

| Item | Result |
| --- | --- |
| Home | Date, minute clock, battery, three enlarged top habit icons without permanent frames, five mood hearts without a Mood label, seven right-aligned grass symbols, centered sheep and original six-entry dock. |
| Dock, left to right | Continue reading, Browse files, Library, OPDS, Transfer, Settings. |
| Immediate tracker access | Select or tap grass stock on Home to open 14-day grass history. Settings → Habits retains the same entry. |
| Empty habit slots | Rounded dashed placeholders; focus/tap opens Choose habit. Nine saved habits, three active. |
| Sheep interactions | Selecting the sheep opens a double-line panel titled with its name, with Pet, Call and Play. Responses expire after ten seconds; foraging cannot be bypassed by calling. |
| Icons | 24 original monochrome icons, including Reading, Focus, Family, Relationship, Money, Phone and five general choices (Star, Flag, Target, Check, Sun). Device and web settings preserve choices. Older habits derive defaults. |
| Reading and timed habits | One cumulative total combines device reading, paper-book timing and manual minutes. Large current number, smaller `/ target min`, rounded progress and action rows. |
| Manual minutes | 5/10/15/20/30/45/60/Custom grid, cumulative total preview and confirmation. Custom uses the existing interval chooser. |
| Daily/weekly counts | Dedicated progress screen, week date range and confirmed Log one action. Week starts on any chosen weekday, default Monday. |
| Pomodoro | Manual break start; Skip short break immediately starts the next focus. Unified `min`. Stop/log confirmation. |
| Completion | Double-line positive notice, sheep, actual grass gain or honest full-stock/save-failure notice. Reading notices wait until Home. |
| Sheep pairs | Four visual sheep pairs distinguished by black/white faces and legs. No numbers. Access through Play; removed from Habits settings. |
| Grass presentation | `Grass 21 / 21` in text settings; seven right-aligned three-blade symbols on Home (one blade per unit); exact numerical stock remains in settings/history. No decorative grass in sheep scenes, including empty-stock scenes. |
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

## Initial X3 visual feedback — implemented 2026-10-04

Status: implemented 2026-10-04 for the next X3 firmware test; hardware verification remains open. This batch changes presentation only; existing care rules and grass accounting remain unchanged. Focus uses one outline (two pixels for emphasis), while popup windows retain two outlines.

| ID | Implemented change | Implementation / hardware checks |
| --- | --- | --- |
| P-019 | Remove permanent frames around the three configured top habit icons; enlarge the icons. | Plain icons at rest. Hardware-selected icons still need the single-line focus indicator in P-020. Empty slots show a single solid focus outline when selected and the agreed dashed placeholder when not selected, never two stacked frames. |
| P-020 | All hardware-navigation selection indicators use a single-line frame. | Apply consistently to selectable items, including habits, grass/history, sheep, dock and other screens. Keep adequate padding around content. |
| P-021 | Reserve double-line frames for popup messages and windows. | Preserve the agreed popup design; separate popup borders from ordinary focus indicators. |
| P-022 | Replace Home's numeric grass stock with seven right-aligned grass symbols, analogous to the five hearts. Each meal removes one third of one symbol. | Cap 21: each complete symbol represents three grass units; three meals consume one symbol per day. Preserve direct selection/tap access to the 14-day grass history. Implemented rendering: each symbol has three distinct blades, allowing exact one-third/two-thirds states without clipping an indistinct shape. 18 units = six full symbols and one empty slot; 17 = five full, one two-thirds and one empty. Settings/history retain exact numeric stock. Check spacing against habit captions and focus frames in all orientations. |
| P-023 | Keep the entire Home battery display within the right margin. | Header now measures the complete icon/spacing/percentage group and right-aligns it inside both the layout margin and oriented bezel margin. Sleep retains battery without percentage. |
| P-024 | Replace the unnatural long straight ground line beneath the sheep. | A short, slightly curved ground mark replaces the full-width horizontal line on Home and sleep screens, with no decorative grass. Rest poses use a higher contact line beneath their folded legs. |
| P-025 | Review excessive blank space above the sheep and explain its purpose. | Artwork now uses up to 420 pixels of available width and is vertically centered in the scene, rather than bottom anchored. All poses and the foraging sign remain within the scene. |
| P-026 | Make sheep outlines slightly thinner, but keep them heavier than the other icons. | Original sheep/pair outline strokes reduced from four to three source units; icon artwork retains 2.5 source units. PBMs regenerated; review actual e-ink legibility. |
| P-027 | Review the tiny sheep name, which currently has little purpose. | Keep user naming; remove the permanent small Home label and use the name as the interaction-window title. Long names are truncated to the popup width; an empty name falls back to the localized interaction title. No new Pet/Call response text is added. |

Source checks: `HabitSheepHomeUi.cpp` explicitly selects fixed font IDs for Home text; `UIScale.h` also defines one fixed UI tier. Home does not follow the reader's selected font size. The reported overlap should be addressed as layout/padding, rather than attributed to the user's font preference. Habit-caption height now determines its band spacing, leaving twelve pixels before the separate status/focus row. Hardware validation is still required.

## Pending second X3 feedback batch — 2026-10-04

Status: record only. P-028–P-031 are not implemented in firmware `d3030aa`; no code change, build or new firmware is requested for this update.

| ID | Pending request | Acceptance notes |
| --- | --- | --- |
| P-028 | Add orientation selection in Settings → Habits, with the same four choices as reading. | Portrait, Landscape CW, Portrait 180°, Landscape CCW. Provide an actual on-device orientation entry for Habit Sheep Home/habit screens; keep the reading orientation setting independent. Check navigation transitions, return from reading and Habit Sheep sleep rendering. Current `Reading Orientation` only applies to readers; `ReaderActivity::onExit()` restores Portrait. Host-rendered landscape checks do not mean Home currently has a usable landscape switch. |
| P-029 | Make the Home sheep slightly smaller and keep its selection frame clear of the dock separator. | Reduce the sheep artwork size and separately inset/resize its focus rectangle so its bottom edge does not touch or overlap the horizontal divider above the six dock icons. Do not rely on shrinking the artwork alone, because the current frame encloses the entire sheep scene. Retain single-outline focus. |
| P-030 | Remove the ground beneath the sheep entirely. | Supersedes the curved-ground treatment in P-024. No ground line or contact shadow on Home or Habit Sheep sleep scenes; retain no decorative grass. Keep the foraging notice legible. |
| P-031 | Show grass stock on the Habit Sheep sleep screen, using the same representation as Home. | Seven right-aligned grass symbols, each containing three independently filled/empty blades. One blade represents one grass unit; cap remains 21. Keep hearts and date/battery clear, including after scheduled meals. This is a passive sleep display, not an interactive history shortcut. Existing meal accounting and sleep refresh rules stay unchanged. |

## Pending uniform rewards and timed sessions — 2026-10-04

Status: agreed product rules, recorded only; not implemented in firmware `d3030aa`. These rules supersede the reward amounts under Existing care rules once implemented. The current firmware still awards three grass per Pomodoro focus and applies the old duration/weekly-count reward rules.

| ID | Agreed change | Acceptance notes |
| --- | --- | --- |
| P-032 | One completed session or logged count earns exactly one grass unit for every habit type. | Pomodoro: +1 for each completed focus, never for a break or Skip break. Reading and other timed habits: +1 per complete accumulated session. Daily/weekly count habits: +1 per confirmed logged completion; remove the weekly-target apportionment formula. Goals track progress rather than gate rewards: extra completed sessions/counts can still earn grass, subject to cap 21. One unit fills one blade of the seven-symbol meter; three units fill one whole symbol. Meals remain one unit at each of three daily meals. Keep existing stock/history unchanged, with no retroactive clawback or revaluation. History records actual new gains; full-stock completions do not become deferred food debt. |
| P-033 | Reading and other timed habits use accumulated session counts instead of a single daily duration-target reward. | Users choose their target session count and minutes/hours per session. Daily completed sessions = floor(today's accumulated seconds / seconds per session). Device reading, paper-book timing and manual additions remain one combined reading total. Every newly completed whole session earns +1, including sessions beyond the goal; residual time does not count as another session and does not carry to the next date in daily mode. Accumulation must survive pauses/restarts without duplicate credit. Reading does not show disruptive per-session completion popups; Pomodoro retains its intentional focus/break workflow. Migrating existing durations, changing session length and old events must not create duplicate or retroactive rewards. |

Weekly timed/reading mode is still a design choice, not an approved implementation item: the recommendation discussed is optional weekly goals, with whole-week accumulated time and within-week carry of partial sessions, using the existing configurable week boundary (default Monday). Its exact behavior remains to be confirmed; weekly count habits are already covered by P-032.

Balancing intent: three grass earned covers three daily meals. Three 30-minute sessions take 90 minutes; users may lower reading session duration if needed. This reward decision alone does not change the current default Pomodoro focus duration of 25 minutes or silently make it 30 minutes.

## X3 verification checklist

1. Check larger unframed habit icons, single-outline focus and double-outline popups. Check long names/labels, battery 100%, grass 0/1/2/3/17/18/20/21 and folded/jump poses; confirm no overlap or clipping. Navigate all three top slots, grass, sheep and six dock entries; test an empty slot and long-press replacement. Home/habit orientation hardware tests beyond Portrait await P-028; the current reading-only orientation setting does not rotate Home.
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

Artwork uses committed SVG/PBM sources and a stdlib-only PlatformIO generator. Packed one-bit artwork stays in flash and scales as horizontal runs into the existing framebuffer; no new framebuffer or render-time bitmap allocation. The popup name uses the existing UTF-8-safe truncation helper, whose temporary string is limited by the saved name (96 bytes) and only exists while the popup is rendered. No extra framebuffer or bitmap allocation is introduced. New activities use fallible ActivityManager-owned screen-lifetime allocation. Icon selection retains one copied habit until its result returns. Popup row storage belongs to the popup rather than the render-task stack.

Automated tests compile the real stores, event log and timer against deterministic HAL/storage, and the real popup callback and scene scheduling helpers. Final CI/build status and firmware provenance are attached to PR #1. Actual battery behavior, e-ink refresh quality, button/touch usability and the reported freeze require the user's X3 test.

AI, sync, further games/interactions and per-habit analytics have no approved specification and remain future scope. The older three-row interaction concept was superseded by three top habits and the sheep popup, so no redundant top interaction row is added.
