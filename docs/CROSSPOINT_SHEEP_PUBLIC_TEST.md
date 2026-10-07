# CrossPoint Sheep V1 public-test candidate

This is a test build for the independent CrossPoint fork, not a published stable release. PR #1 remains draft. Use the X3 firmware artifact from the candidate's CI run; firmware for other boards is not interchangeable. Firmware update alone preserves existing Habits data. **Reset Habits is deliberately destructive to Habits records.** Back up the SD card first if keeping that history matters.

## New V1 checks

1. In Settings → Habits → Reset Habits, read the first warning and Cancel. Repeat, Continue, then Cancel at the second prompt. Existing habits, history, grass/hearts, sheep name and timers should be unchanged.
2. Note a book's reading position, fonts/reading orientation, Display/Sleep Screen, Wi-Fi and OPDS settings. Confirm both reset prompts. Expect Reading/Pomodoro/empty third slot, nine grass, five hearts, an unnamed sheep, mode On, Monday, portrait and first-habit focus. Habits records and timers should be empty. Verify all noted CrossPoint settings and book position remain intact, including after restarting.
3. Long-press the sheep: the menu should read `Play with [name]` and contain Pairs, Sheep Turn, Remember sheep and Sheep-doku. Icons sit left of names in every button.
4. In Sheep-doku, identify front/rear/left/right at actual X3 size. Dotted clues must not change. Select an empty cell, choose each symbol, Clear and Undo. Hint should suggest a sheep and wait for Select; Back cancels the chooser. Solve/restart several random boards. Repeat landscape and touch on supported hardware.
5. In Sheep Turn, try moves, Undo, then Hint. Only the marked sheep should be suggested; Select applies the move. Repeatedly following hints should reach all-facing-forward. Restart and repeat.
6. Verify boot/About use CrossPoint Sheep. Recheck Home default focus stays on the first habit after an idle refresh; long and short Skip start focus immediately. Complete a focus/timed/count session and check one grass each.
7. Leave an unfinished focus across midnight and restart/sleep; yesterday's unfinished time should not appear today. Check reading/timed daily versus configured weekly accumulation and no duplicate rewards.
8. At a meal, verify one grass charge, visible grass in eating artwork for five minutes, and then rest. Check day/night device sleep poses and overnight RTC/battery behavior against Cover mode.

Read [the complete X3 checklist](HABIT_SHEEP_PENDING.md#x3-verification-checklist) for the existing interfaces, histories and pause/foraging rules. Freeze, e-ink ghosting and touch usability remain physical checks. If a problem occurs, report firmware build, device/orientation, last screen/action, expected/actual result and a photo or Serial log where possible.

## Later ideas

V2 may add pushing grass and optional turn-based games against the sheep. More daytime-rest drawings, AI/sync and a foraging-scene test aid remain separate proposals. They are not part of this candidate.
