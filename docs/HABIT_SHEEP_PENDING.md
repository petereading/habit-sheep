# Habit Sheep — implemented screens and X3 verification

Updated 2026-10-06. The second artwork/game/history batch is implemented for X3 testing. PR #1 stays draft on `feature/habit-sheep-v1`; merging into develop and submitting upstream remain unauthorized.

## Current implementation

| Item | Result |
| --- | --- |
| Home | Date, minute clock, battery, three enlarged top habit icons without permanent frames, five mood hearts without a Mood label, seven right-aligned grass symbols, centered sheep and original six-entry dock. |
| Dock, left to right | Continue reading, Browse files, Library, OPDS, Transfer, Settings. |
| Immediate tracker access | Select or tap grass stock on Home to open 14-day grass history. Settings → Habits retains the same entry. |
| Empty habit slots | Rounded dashed placeholders; focus/tap opens Choose habit. Nine saved habits, three active. |
| Sheep interactions | Short Confirm/tap on the sheep starts a random three-stage interaction, with two-second keyframes and a final smile/heart held to ten seconds. Long Confirm/long touch opens the named 2×2 game popup. Long presses suppress release/tap. Food accounting takes priority over interaction artwork. |
| Icons | 24 original monochrome icons, including Reading, Focus, Family, Relationship, Money, Phone and five general choices (Star, Flag, Target, Check, Sun). Device and web settings preserve choices. Older habits derive defaults. |
| Reading and timed habits | One cumulative total combines device reading, paper-book timing and manual minutes. Large whole-session number, smaller `/ target sessions`, accumulated minutes and session length. Daily/weekly targets are independent of rewards. Session length 1–1440 min and target 1–99 can be configured on device and web. |
| Manual minutes | 5/10/15/20/30/45/60/Custom grid, cumulative total preview and confirmation. Custom uses the existing interval chooser. |
| Daily/weekly counts | Dedicated progress screen, week date range and confirmed Log one action. Week starts on any chosen weekday, default Monday. |
| Pomodoro | Tool without a daily target. Home shows today's completed sessions; timer shows focuses until long break and phase progress. Default four focuses controls long breaks, not a reward goal. Manual breaks; Skip short break immediately starts focus. Unified `min`. |
| Completion | Count/focus completions show double-line positive notices and actual gain or full-stock/save-failure notice. Reading/timed sessions award silently. |
| Games | Pairs has six houses/three pairs: selecting opens a door, a miss stays visible until acknowledged, and matched houses remain open with a check. Find different, Remember sheep and Sheep order fill the other three 2×2 menu entries. Previous/Next/Confirm/Back and direct touch work throughout. Remember waits for user confirmation to hide; order swaps two selected sheep. No timed reflexes, dragging or food rewards. |
| Grass presentation | `Grass 21 / 21` in text settings; seven right-aligned three-blade symbols on Home and sleep (one blade per unit). Exact numerical stock in history. No decorative grass or ground line in sheep scenes. |
| Awake poses | Twelve poses, ten-minute intervals, no repeat within two hours. Missed meals override with rest; only zero mood shows the foraging sign. |
| Sleep selection | Display → Sleep Screen → Habit Sheep. No separate Sleep sheep scene setting. Date, battery without percentage, hearts below date, sheep, three passive habit progress icons. |
| X3 sleeping updates | RTC timer maintenance wake at half-hour boundaries, including 08:00/13:00/19:00 meals. Successful meals show eating for five minutes, then rest; missed meals rest and zero mood forages. Other sleep modes do not schedule these wakes. |
| Pause | Habit mode Off preserves history/timer time and freezes food/mood/bond decline. Home shows recent book; Cover becomes the default sleep mode with explicit user changes respected. Resume has no catch-up charges and leaves timer paused. |
| Input compatibility | Existing CrossPoint hardware hints/navigation; direct touch targets for habits, sheep, grass, actions, minute presets, icons and history paging. |
| Habits orientation | Settings → Habits → Habits Orientation: Portrait, Landscape CW, Portrait 180°, Landscape CCW, independent of Reading Orientation. Applied to Home, habit screens and Habit Sheep sleep; reader exit restores the Habits choice. |
| Rotated controls | Timer, count, icon picker, memory game and history use CrossPoint's oriented safe area, leaving physical button hints clear on left/right/top edges. Render and touch bounds share the same geometry; landscape history uses the smaller font for seven readable rows. |

## Existing care rules preserved

Grass is the only resource. Cap 21 (seven days of food); fresh installations start with nine. Local meals at 08:00, 13:00 and 19:00 consume one each. Each new completed focus, accumulated timed session or logged count earns one grass unit, including beyond the target. No cycle/goal bonus. Stock capacity limits actual gain; completed events remain recorded, with no deferred reward. Upgrades/session-length edits never revalue stock/history or retroactively reward logged time.

Daily sessions use floor(today's seconds / session seconds), with no next-day remainder carry. Optional weekly sessions retain remainder within the selected week only. Live paper timers split at midnight; paused timers retain dated time without counting sleep/pause time.

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

## Implemented second X3 feedback batch — 2026-10-04

Status: P-028–P-031 implemented; orientation, margins and sleep await X3 verification.

| ID | Pending request | Acceptance notes |
| --- | --- | --- |
| P-028 | Add orientation selection in Settings → Habits, with the same four choices as reading. | Implemented: Portrait, Landscape CW, Portrait 180°, Landscape CCW. Reading orientation stays independent; reader exit restores Habits orientation. Check transitions and sleep on X3. |
| P-029 | Make the Home sheep slightly smaller and keep its selection frame clear of the dock separator. | Reduce the sheep artwork size and separately inset/resize its focus rectangle so its bottom edge does not touch or overlap the horizontal divider above the six dock icons. Do not rely on shrinking the artwork alone, because the current frame encloses the entire sheep scene. Retain single-outline focus. |
| P-030 | Remove the ground beneath the sheep entirely. | Supersedes the curved-ground treatment in P-024. No ground line or contact shadow on Home or Habit Sheep sleep scenes; retain no decorative grass. Keep the foraging notice legible. |
| P-031 | Show grass stock on the Habit Sheep sleep screen, using the same representation as Home. | Seven right-aligned grass symbols, each containing three independently filled/empty blades. One blade represents one grass unit; cap remains 21. Keep hearts and date/battery clear, including after scheduled meals. This is a passive sleep display, not an interactive history shortcut. Existing meal accounting and sleep refresh rules stay unchanged. |

## Implemented uniform rewards and timed sessions — 2026-10-04

Status: P-032–P-033 implemented. All rewards are one grass per newly completed whole session/count; goals never gate rewards.

| ID | Agreed change | Acceptance notes |
| --- | --- | --- |
| P-032 | One completed session or logged count earns exactly one grass unit for every habit type. | Pomodoro: +1 for each completed focus, never for a break or Skip break. Reading and other timed habits: +1 per complete accumulated session. Daily/weekly count habits: +1 per confirmed logged completion; remove the weekly-target apportionment formula. Goals track progress rather than gate rewards: extra completed sessions/counts can still earn grass, subject to cap 21. One unit fills one blade of the seven-symbol meter; three units fill one whole symbol. Meals remain one unit at each of three daily meals. Keep existing stock/history unchanged, with no retroactive clawback or revaluation. History records actual new gains; full-stock completions do not become deferred food debt. |
| P-033 | Reading and other timed habits use accumulated session counts instead of a single daily duration-target reward. | Users choose their target session count and minutes/hours per session. Daily completed sessions = floor(today's accumulated seconds / seconds per session). Device reading, paper-book timing and manual additions remain one combined reading total. Every newly completed whole session earns +1, including sessions beyond the goal; residual time does not count as another session and does not carry to the next date in daily mode. Accumulation must survive pauses/restarts without duplicate credit. Reading does not show disruptive per-session completion popups; Pomodoro retains its intentional focus/break workflow. Migrating existing durations, changing session length and old events must not create duplicate or retroactive rewards. |

Optional weekly timed/reading mode follows the approved daily/weekly target design, using the configurable week boundary (default Monday). Existing saved periods are preserved; daily remains the default.

Pomodoro has no daily target. Its saved cycle counter survives midnight and restart independently of today's completed-session count. Four focuses before long break is the default timing cycle, not a goal or bonus; completed focuses earn one each. Breaks require manual start and short-break Skip starts focus immediately.

Balancing intent: three grass earned covers three daily meals. Three 30-minute sessions take 90 minutes; users may lower reading session duration if needed. This reward decision alone does not change the current default Pomodoro focus duration of 25 minutes or silently make it 30 minutes.

## New X3 feedback — 2026-10-05, implemented

- Twenty native SVG poses follow the approved proportions/color scheme: white wool/forehead/tail, black long face/ears/legs, standalone smile without a linked nose. Twelve awake, four distinct rest/sleep, two eating, two interaction poses. Stable date/pose-slot pseudorandom mirroring supplies both directions without duplicating bitmap arrays; Z and heart overlays remain upright. SVGs are re-rasterized to 384×288 for sheep and 192×192 for icons. No new framebuffer or runtime bitmap allocation. Smaller icons retain monochrome pixel edges, but are no longer enlarged from 48×48 sources.
- Eating includes visible grass at the lowered mouth or protruding while chewing. Only feeding has grass. Home no longer lets automatic sleep interrupt the five-minute meal window; manual power sleep remains available. Meal and minute-change checks both execute rather than short-circuiting. The user's flash cause is unproven and requires hardware verification; these changes address visibility and interruptions without changing meal charges.
- Pairs and three other games are accessed by long-press on the sheep. Short press interacts; original Pet/Call panel is replaced. Hardware hints identify the long-press game entry. Black/white face/leg combinations are reserved for game sheep; the companion keeps the agreed black face/legs.
- Each habit edit menu has History / statistics: two seven-day pages, date totals and a 14-day summary. Counts use actual logged completions, Pomodoro uses actual focus records, duration shows accumulated minutes and sessions derived using the current interval. Weekly remainder is attributed on the day it completes, reading up to six preceding days to seed the first displayed week. Changing interval changes these session estimates, never food/history events. The aggregate grass ledger remains separate. Missing dates show zero activity; read failures show a notice.
- Game/history activities use fallible screen-lifetime ownership. Puzzle state is fixed arrays; history holds fourteen rows/counts and one 512-byte parser scratch buffer in its activity, avoiding a new render stack buffer or new global cache. Scenes and loops share the existing RenderLock.

## Artwork fidelity and history controls — 2026-10-05 correction

The e7b4d83 sheep was manually reconstructed geometry and departed from the approved illustration proportions. Replace its twenty scene sources with monochrome pixel-contour SVGs taken from the approved awake/eating/interaction sheet and four-rest sheet. Original shape, head/body proportions, feet and expression are retained; detached labels/hearts/Z are removed, with upright heart/Z overlays placed on the correct head side. The icon-generation script preserves these SVG sources instead of redrawing them. PBM size stays 384×288, flash-only, without another framebuffer. Pixel contours preserve the approved illustration rather than promising infinite-resolution vector curves.

Both grass and individual habit history show their two on-screen paging buttons only when MappedInputManager reports touch capability. The hardware-only UI keeps CrossPoint's mapped bottom hints; touch themes already suppress those hints. Habit history uses the same single-outline touch buttons and tap rectangles as grass history. X3 checks: only one Newer/Older control set; touch check: visible buttons still page correctly.

## Pending cross-day bug checks — X3 report 2026-10-06

Status: recorded only; investigation, fixes and regression tests are pending. No code change is included with this report.

| ID | Report / pending check | Acceptance notes |
| --- | --- | --- |
| P-034 | An unfinished Pomodoro focus from yesterday carried five minutes into today. | Reproduce and identify whether the carry is in the timer display, today's totals or reward accounting. Check unfinished running/paused focus across local midnight, sleep and restart. Previous-date time must not be incorrectly credited to today's daily progress or rewarded twice. Preserve the separately agreed saved Pomodoro cycle counter; do not confuse it with daily progress. |
| P-035 | Check Reading and other habits for the same cross-day problem. | Check combined device reading, paper timing and manual minutes, plus custom timed and count habits. Verify date attribution, daily reset and no prior-day daily remainder or duplicate grass credit, including pause/resume and restart. Weekly timed remainder may carry within the configured week only; verify the selected week boundary too. These checks have not yet been performed for this report. |

## Pending timer improvements — 2026-10-06

Status: suggestions recorded for later implementation; no code or artwork change in this update.

| ID | Requested improvement | Acceptance notes |
| --- | --- | --- |
| P-036 | Show the sheep reading in Reading timed mode, changing pose every five minutes. | Add reading-themed poses to the Reading timer screen. This is a visual change; preserve cumulative time and grass accounting. |
| P-037 | Show the sheep thinking in Pomodoro mode, changing pose every five minutes. | Use thinking-themed poses, for example a light bulb above its head or a thought bubble. Preserve focus/break timing and reward rules. |
| P-038 | Add Skip to long breaks, matching the existing short-break button. | Make Skip available for long breaks through hardware buttons and touch. As with short-break Skip, immediately start the next focus session; skipping a break earns no grass. |

## X3 verification checklist

1. Check unframed habit icons, single-outline focus and double-outline popups. Check battery 100%, grass 0/1/2/3/17/18/20/21 and folded/jump poses. Navigate all slots, grass, sheep and dock. Set each Habits orientation, return from a differently oriented reader and check habit/settings navigation and sleep. Confirm smaller sheep, no ground and frame margin above the dock.
2. Open grass history directly from Home. Check both seven-day pages, earned/eaten amounts, paused markers and the current stock. Preserve existing records after firmware update.
3. Create/edit a custom habit and select icons on both pages. Restart; confirm choices survive. Test Family/Relationship/Money/Phone and generic icons.
4. Combine device reading, paper timing and manual additions. Verify one total, Cancel has no effect and each whole session earns one, even beyond target, without per-session popups. Test partial sessions, restart, length edits and daily/weekly boundaries: no retroactive or duplicate rewards.
5. Complete daily and weekly count habits several times. Verify date range and Monday/default or a changed boundary. Check positive/full-stock notices.
6. Finish a Pomodoro, leave the break waiting, then Skip break. Repeat during a running short break. Focus starts immediately; +1 per focus, no break/cycle bonus. Home has no daily denominator; cycle setting means focuses before long break.
7. Use short-press interaction and long-press Games repeatedly, create/rename/delete habits and open chained confirmations. Play several rounds, return Home and read for at least thirty minutes. If it freezes again, record the last screen/action and capture Serial panic/backtrace, free heap and largest block if available.
8. Leave Home awake for two hours: twelve ten-minute poses, hearts without Mood, no drawn grass. An interaction response returns to the current pose after ten seconds.
9. Select Habit Sheep sleep mode before a meal. Check exactly one grass deducted, five-minute eating scene, then rest. Leave asleep across half-hour boundaries and wake manually. Compare overnight battery use with Cover sleep mode; scheduled wakes are X3-specific and require device validation.
10. With no stock, verify missed meals show rest while hearts remain positive; zero hearts shows the foraging sign. Earn grass to return the sheep. Pause/resume and confirm there are no catch-up meal charges or automatic timer resume.

11. Play Pairs (six houses), Find different, Remember sheep and Sheep order using buttons. Confirm a wrong answer stays visible until acknowledged. Repeat by touch where available. Confirm long-press does not also trigger short-press interaction.
12. Open each habit’s History / statistics, check fourteen dates against known reading/count/focus records and test a weekly remainder crossing the first displayed date.

## Engineering and validation

Artwork uses committed SVG/PBM sources and a stdlib-only PlatformIO generator. Packed one-bit artwork stays in flash and scales as horizontal runs into the existing framebuffer; no new framebuffer or render-time bitmap allocation. The popup name uses the existing UTF-8-safe truncation helper, whose temporary string is limited by the saved name (96 bytes) and only exists while the popup is rendered. No extra framebuffer or bitmap allocation is introduced. New activities use fallible ActivityManager-owned screen-lifetime allocation. Icon selection retains one copied habit until its result returns. Popup row storage belongs to the popup rather than the render-task stack.

Automated tests compile the real stores, event log and timer against deterministic HAL/storage, and the real popup callback and scene scheduling helpers. Final CI/build status and firmware provenance are attached to PR #1. Actual battery behavior, e-ink refresh quality, button/touch usability and the reported freeze require the user's X3 test.

AI, sync and additional games beyond the four approved here remain future scope. The older three-row interaction concept was superseded by three top habits and the sheep popup, so no redundant top interaction row is added.

Weekly timed progress reuses the existing nine-entry reserved week cache, adding seconds/validity fields rather than a second allocation. New interval pickers are ActivityManager-owned, nothrow-allocated and OOM-checked; copied habits are screen-lifetime only. Timer date tracking adds a fixed 16-byte day per existing session, with no additional timer allocation. Source evidence: `HabitEventLog.cpp:359–427` replaces goal-gated rewards; `HabitTimer.cpp:39–68` dates live timer fragments; `HabitSheepHomeUi.cpp:159–182` removes ground and insets sheep focus. Final CI provenance belongs to PR #1.
