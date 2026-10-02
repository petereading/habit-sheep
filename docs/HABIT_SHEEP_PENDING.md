# Habit Sheep — implementation and device test list

The user approved items 1–7 and the uniform `min` abbreviation. These changes are implemented for the next X3 firmware test. PR #1 remains a draft targeting `develop`; neither merge nor upstream submission is authorized.

## Implemented: items 1–7

1. **Skip short Pomodoro break:** Start the next focus immediately, either before or during the short break. Skipping creates no event or grass award. Long breaks keep their existing flow.
2. **Awake clock:** Home and individual habit timer screens show the existing local device clock and time-format preference. Refresh when the minute changes, including while paused. Omit unavailable time. No minute wakeups or live clock on the static sheep sleep scene.
3. **Sleep selection:** Display → Sleep Screen includes Habit Sheep alongside Cover and the existing modes. Remove the separate sheep toggle from device and web settings. Migrate the effective legacy choice once, preserving Quick Resume. Its existing timeout behaviour remains unchanged.
4. **14-day grass history:** Settings → Habit Sheep → Grass history shows seven dates per page, with grass actually earned and eaten. Earlier dates without stored entries show a dash. This is an aggregate grass ledger, not individual habit statistics.
5. **Consumable grass:** Stock and feeding now follow the three-meal functional slice below. Offline elapsed days use only available stock. Unavailable clock pauses feeding; no debt or death.
6. **Foraging scene:** At zero mood, the pen shows a playful foraging sign. New grass returns the sheep. Formal animation remains deferred.
7. **Completion notice:** Completion counts, duration/reading targets and completed Pomodoro focus sessions show a positive notice with a grass symbol and the actual reward. Full stock still records completion and shows a positive full-stock notice. Duration timers keep running after the target checkpoint. Reader notices wait until Home so they do not interrupt a book.

All abbreviated timer units remain `min`, irrespective of the number.

## X3 device checks

- Finish one focus, then test Skip break both before and during the short break. Confirm the next focus starts at zero, and the completed focus earns grass only once.
- Leave Home and a paused habit open across a minute boundary. Check portrait and both landscape orientations for overlap.
- Select Cover, sleep and wake; select Habit Sheep and repeat. Restart to verify the choice persists. Check Quick Resume separately if used.
- Reach a duration target, continue timing, then stop. Confirm one grass award and correct accumulated minutes. Test automatic reading and its notice on return Home.
- Check both history pages and the current stock. At full stock, complete a habit: progress should record normally, with the full-stock notice.
- With empty stock, confirm the foraging sign; earn one grass and confirm the sheep returns.

## Implemented functional slice — 2026-10-02

The user authorized code changes for P-004 through P-010. Finish this functional X3 test before starting the formal artwork/UI phase. PR #1 stays draft.

- **P-004 — spacing:** stock and reward labels use `Grass 21 / 21`. Final artwork may use a grass icon; settings retain readable text.
- **P-005 / P-007 — food:** grass is the only resource. Three local meals at 08:00, 13:00 and 19:00 consume one grass each; cap 21. A fresh installation starts with nine grass. Existing consumable stock and history are preserved; upgrading establishes a new meal baseline without retroactive charges. Daily count completions, daily duration targets and Pomodoro focuses earn +3. A weekly count target below seven apportions 21 across its target completions (e.g. three completions earn +7 each); larger targets earn +3 each. Awards reflect only actual capacity remaining. No food debt or death.
- **P-006 — week boundary:** Settings → Habits → Week starts on offers all seven days, default Monday. Weekly progress follows the chosen local midnight boundary immediately. Changing it does not rewrite events or award old completions.
- **P-008 — mood and relationship:** five hearts show mood. Eating restores one heart; a whole local day without food costs one heart, excluding dates touched by pause. A missed meal shows rest; zero hearts shows the foraging sign. Replenishment returns the sheep and shares one of that day's three meal slots if available. Optional hello or a completed game improves bond at most once per local date. Empty days gently reduce bond while mood remains positive. Bond is expressed by position and responses, with no separate meter. Daytime wandering advances in coarse ten-minute poses; nighttime/empty-meal rest uses a static pose, not continuous animation.
- **P-009 — optional game:** Sheep pairs is an eight-card/four-pair memory game accessible through the sheep menu or Habits settings. Mismatches stay visible until Select, avoiding timed e-ink reveals. Back exits immediately; there are no food rewards or automatic habit completions.
- **P-010 — pause:** Habit mode Off pauses the running timer and preserves elapsed time. It stops new habit records/rewards and freezes feeding, mood and relationship decline. Resume rebases meal time with no catch-up charges and leaves timers waiting for manual Resume. Paused dates are marked in the existing two-page grass history. Home shows the latest available book's title and cover instead of habit controls. Disabling selects the real Display Cover sleep mode and remembers the prior mode; re-enabling restores it unless the user explicitly changes Display → Sleep Screen while paused.

## X3 checks for this slice

1. At 12:59 → 13:00 and 18:59 → 19:00, return Home and inspect history: exactly one grass per meal, unchanged after reopening or reboot. No scheduled wake is required.
2. Complete a daily habit (+3), a weekly three-count habit (+7 each), a reading target and a Pomodoro focus. Test near cap 21 and verify the positive notice reports the actual gain. Break skipping still produces no reward.
3. With no food, verify rest before the hearts reach zero; after five completely empty days verify the foraging sign. New grass returns the sheep. Test by advancing device date/time only on a disposable test SD backup; backward clock changes intentionally do not replay meals.
4. Turn Habit mode Off with a partially elapsed timer. Read while paused, cross dates, reboot, then enable. Confirm no food/heart catch-up, no paused reading credit, and manual timer resume. Check the book cover/title and both history pages.
5. Test Cover / Habit Sheep / Quick Resume before disabling. Check pause selects Cover, resume restores the prior choice, and an explicit sleep-screen change during pause remains the user's choice.
6. Switch Monday ↔ Sunday around a week boundary. Counts should follow the selected week; old completions must not produce reward notices.
7. Play Sheep pairs using buttons and touch where supported. Confirm a mismatch waits for Select, Back works at every stage, four pairs finish, and grass never increases from playing.
8. Inspect five hearts, sheep status, selection borders, date/clock, paused book tile, game and history in portrait and both landscape orientations.

## Pending X3 feedback — 2026-10-02 (firmware 5fde8ed)

Record only: the user has not authorized implementation of this new feedback yet. No runtime changes or firmware CI run for this update. The Home freeze remains unresolved and takes priority before the formal artwork/UI phase; passing automated tests does not establish X3 runtime stability.

### P-011: Remove the Mood label

- Keep the five hearts and remove the word `Mood` beside them. The hearts already communicate mood.
- This is a presentation change; retain the existing mood calculation.

### P-012: Home freezes after some use — priority blocker

- X3 report: after playing/using the firmware for a while, Home freezes and buttons stop responding.
- The exact sequence, elapsed time, and whether this involves entering or leaving Sheep pairs have not been established. Cause unknown; do not claim a game, refresh, memory or locking issue without evidence.
- Next authorized debugging pass should reproduce the transition sequence and distinguish a frozen display from a stalled input/main loop, including clock refresh, popup/game transitions and sleep/wake where relevant. Capture serial diagnostics if available.
- Acceptance: repeated interactions/game sessions and returns to Home remain responsive, with working navigation, clock updates and sleep/wake. Resolve this before declaring the functional build stable and proceeding to formal screens.

### P-013: Keep Sheep pairs out of Habits settings

- Remove the Sheep pairs entry from Settings → Habits. It is an activity, not a setting.
- Retain access through the sheep interaction menu.

### P-014: Distinguish pairs through sheep appearance

- Replace the numerical identifiers on Sheep pairs cards with small visual differences between sheep, for example black/white heads and different leg colours.
- Pair cards by matching sheep appearance. Use clearly distinct monochrome markings suitable for X3 e-ink; keep variants recognisable at card size.
- Preserve the existing game rules, immediate exit and absence of grass/habit rewards.

### P-015: Future interaction menu as an icon grid

The later P-016 proposal is now the preferred Home layout direction. Keep this grid idea as a future option; it is not an instruction to implement a popup grid now.

- When more sheep interactions exist, consider replacing the summoned text menu with a 4×4 or 3×2 icon grid.
- Icons should make games and other interactions recognisable at a glance, with visible focus and button/touch access.
- Grid size and icon artwork are future design choices, not selected for implementation yet.

### P-016: Tamagotchi-inspired Home layout — 2026-10-03 Europe/London

The user's latest design proposal arranges Home vertically:

1. A horizontal row of sheep interaction icons at the top.
2. A small caption directly below that row showing the interaction title.
3. The sheep itself in the central scene, as the main character.
4. A small caption directly above the habit row showing the habit title.
5. A horizontal row of at most three active habit icons.
6. Six reading/settings icons along the bottom, retaining the existing dock (user clarification on 2026-10-03).

Hardware buttons move focus to the icons; Confirm opens the focused interaction or habit. Proposed navigation to settle during screen design: Left/Right within a row, Up/Down between rows, a clear focus frame, and the caption showing the currently focused item. Touch-capable devices should retain direct selection. These control details are recommendations, not additional user-approved implementation rules.

Preserve the previously agreed five mood hearts without the word Mood, grass stock, clock/battery information, habit progress/confirmation and active-habit replacement. Their compact placement is still to be designed. A possible selected-habit caption can combine the name with progress rather than retaining full-width habit text rows.

The user confirmed retaining all six existing dock entries: Continue reading, Browse files, Library, OPDS, Transfer and Settings. No merging, removal or relocation is requested. This corrects the initial recollection of five icons. The interaction icons and the way user-defined habits obtain recognisable icons remain design choices.

This supersedes a summoned icon grid as the current primary Home layout direction. Record the proposal only; no runtime changes, artwork implementation or firmware CI run are authorized in this update. P-012 (Home freeze) remains the functional blocker before implementing formal screens.

## Still deferred until after this device test

- Formal sheep/grass artwork, polished Home/habit/game layouts and animation frames.
- Richer relationship presentation if the behavioural cues are unclear on X3.
- Scheduled wake, AI, sync and a coarse battery redesign.

## Engineering notes

The reward queue uses nine fixed entries and the grass ledger uses fourteen fixed entries. Timer and Home reuse their existing modal popup rather than allocating another interaction buffer. Popup reward text is a fixed buffer. The history activity uses the existing `makeUniqueNoThrow` activity ownership pattern because the activity stack must retain it until navigation completes; allocation failure is logged.

Native regression tests compile the real habit store, event log, timer and sheep state modules against deterministic HAL/storage stubs. Device layout, SD persistence and actual sleep behaviour still require the user's X3 test.
