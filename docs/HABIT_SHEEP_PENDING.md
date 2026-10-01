# Habit Sheep — implementation and device test list

The user approved items 1–7 and the uniform `min` abbreviation. These changes are implemented for the next X3 firmware test. PR #1 remains a draft targeting `develop`; neither merge nor upstream submission is authorized.

## Implemented: items 1–7

1. **Skip short Pomodoro break:** Start the next focus immediately, either before or during the short break. Skipping creates no event or grass award. Long breaks keep their existing flow.
2. **Awake clock:** Home and individual habit timer screens show the existing local device clock and time-format preference. Refresh when the minute changes, including while paused. Omit unavailable time. No minute wakeups or live clock on the static sheep sleep scene.
3. **Sleep selection:** Display → Sleep Screen includes Habit Sheep alongside Cover and the existing modes. Remove the separate sheep toggle from device and web settings. Migrate the effective legacy choice once, preserving Quick Resume. Its existing timeout behaviour remains unchanged.
4. **14-day grass history:** Settings → Habit Sheep → Grass history shows seven dates per page, with grass actually earned and eaten. Earlier dates without stored entries show a dash. This is an aggregate grass ledger, not individual habit statistics.
5. **Consumable grass:** Stock begins at three, holds at most 14, and decreases by one per local calendar day when the device next settles its state. Offline elapsed days use only available stock. Unavailable clock pauses feeding; no debt or death.
6. **Foraging scene:** An empty pen shows a playful foraging sign. New grass immediately returns the sheep. Autonomous movement, eating animation and relationship decline remain deferred.
7. **Completion notice:** Completion counts, duration/reading targets and completed Pomodoro focus sessions show a positive notice with a grass symbol and the actual reward. Full stock still records completion and shows a positive full-stock notice. Duration timers keep running after the target checkpoint. Reader notices wait until Home so they do not interrupt a book.

All abbreviated timer units remain `min`, irrespective of the number.

## X3 device checks

- Finish one focus, then test Skip break both before and during the short break. Confirm the next focus starts at zero, and the completed focus earns grass only once.
- Leave Home and a paused habit open across a minute boundary. Check portrait and both landscape orientations for overlap.
- Select Cover, sleep and wake; select Habit Sheep and repeat. Restart to verify the choice persists. Check Quick Resume separately if used.
- Reach a duration target, continue timing, then stop. Confirm one grass award and correct accumulated minutes. Test automatic reading and its notice on return Home.
- Check both history pages and the current stock. At full stock, complete a habit: progress should record normally, with the full-stock notice.
- With empty stock, confirm the foraging sign; earn one grass and confirm the sheep returns.

## Still deferred

- Autonomous sheep behaviour and animations: eating, walking, sleeping and returning.
- Relationship behaviour and gentle decline during prolonged absence.
- Optional interactions/minigames, scheduled wake, AI and sync.
- Coarse battery display and any other items outside the approved 1–7.

## Engineering notes

The reward queue uses nine fixed entries and the grass ledger uses fourteen fixed entries. Timer and Home reuse their existing modal popup rather than allocating another interaction buffer. Popup reward text is a fixed buffer. The history activity uses the existing `makeUniqueNoThrow` activity ownership pattern because the activity stack must retain it until navigation completes; allocation failure is logged.

Native regression tests compile the real habit store, event log, timer and sheep state modules against deterministic HAL/storage stubs. Device layout, SD persistence and actual sleep behaviour still require the user's X3 test.
