# CrossPoint Sheep V1 — current product and architecture

CrossPoint Sheep is a thin fork of CrossPoint Reader for quiet reading and habit formation. This document describes the implemented V1 public-test candidate, superseding earlier draft rules. The repository stays `petereading/habit-sheep`; the implementation branch is `feature/habit-sheep-v1` and PR #1 remains draft. Public testing does not authorize a release, merge or upstream submission.

## Habits and rewards

Save up to nine habits, with three active Home slots. Fresh installations and Reset Habits seed Reading (30 min/session), Pomodoro (25 min focus, four focuses before long break) and an empty third slot. Saved user definitions/history survive firmware upgrades. Reading is optional: it may be removed without disabling CrossPoint reading.

Every completed count, Pomodoro focus or whole accumulated timed/reading session earns **one grass**, including beyond a daily/weekly target. Targets describe progress and never gate rewards. Pomodoro is a tool without a daily target; four focuses controls the long-break cycle only. Breaks start manually. Skip on waiting/running short or long breaks starts the next focus immediately without a reward.

Reading integrates device reading, paper-book timing and manual minutes into one total. Timed habits award whole sessions silently; fractions remain in the current daily/weekly period. Daily fractions never carry into tomorrow. Weekly fractions stay within the configured week. The week boundary defaults to Monday and can be changed. Editing session length or upgrading does not retroactively revalue stock or recorded rewards. Count/focus completions show positive reward notices, including honest full-stock/save-failure messages.

Timers use timestamps with low-frequency e-ink refresh. Active duration timing prevents automatic sleep. Paper/device reading is split across local midnight; unfinished Pomodoro time from yesterday is cleared while completed-cycle progress is retained. Paused mode freezes habit recording/timers and never silently resumes a timer.

## Home, input and artwork

Home has three top habit icons, a date/minute clock/battery header, five mood hearts, seven right-aligned grass symbols, a central sheep, and six original dock entries: Continue reading, Browse files, Library, OPDS, Transfer, Settings. Each three-blade grass symbol represents three units. Selecting/tapping stock opens the fourteen-day grass tracker. Each habit's own History / statistics is available within its settings.

Short sheep press/tap interacts; long press opens `Play with [name]`, a 2 × 2 game popup. Active habit long press opens replacement selection. Button-only Home default is configurable as first habit, sheep or Continue reading; Back retains fast reading access. Touch uses direct targets. Habits orientation independently offers portrait, landscape clockwise, portrait inverted and landscape counterclockwise.

Ordinary focus has one outline; popup windows have two. Sheep scenes have no decorative grass or ground line; eating alone shows grass. Twenty companion poses with stable random mirroring preserve the approved white wool/black face and feet palette. Twelve awake poses change every ten minutes without repeat within two hours. Reading/focus screens have themed sheep changing every five minutes. Interactions use one/one/three-second stages (five seconds total). Artwork is packed one-bit flash data, scaled into the existing framebuffer; no extra framebuffer is allocated.

## Care, pause and sleep

Grass is the only resource. Stock cap is 21, initially nine. Local meals at **08:00, 13:00 and 19:00** consume one each and restore one mood heart up to five. Missed meals show rest; a completely empty day loses one heart. At zero hearts the sheep goes to find food, with no death, illness or food debt. New grass returns it and may use one remaining daily meal slot, never a fourth meal. Stock limits actual gain, while all completed habit events remain recorded; there is no deferred excess reward.

Interactions/game completion improve relationship at most once per local date, without grass rewards. Completely empty days gently reduce relationship while mood remains positive. Habit mode Off preserves records and freezes food/mood/relationship decline. Home shows the recent book. Normal pause temporarily chooses Cover sleep, respects explicit display changes, and resumes without catch-up meal charges.

Display → Sleep Screen → CrossPoint Sheep selects the additional scene; existing Cover and other modes remain available. Sheep sleep displays date, battery without percentage, hearts, grass and passive habit progress, with no clock/countdown. Home keeps its battery percentage. At local 07:00–21:59 a resting sheep has open eyes; 22:00–06:59 uses sleeping poses. X3 schedules half-hour RTC maintenance wakes, including meals; successful meals show eating for five minutes before rest. Missed meals rest and zero hearts forage. Other sleep choices do not schedule these sheep wakes. Physical overnight battery/refresh behavior remains to be validated.

## Games

The four entries are Pairs, Sheep Turn, Remember sheep and Sheep-doku. No reaction timing or dragging is required; all use Previous/Next/Confirm/Back and touch.

- Pairs uses six houses/three pairs, open doors and acknowledged mismatches.
- Sheep Turn toggles one sheep and orthogonal neighbours; face all sheep forward. Every generated state is solvable. Undo and Hint work from the current board. Hint highlights a sheep without turning it automatically.
- Remember sheep shows positions until the user is ready, then asks where a sheep was.
- Sheep-doku generates unique-solution 4 × 4 Sudoku boards with four 2 × 2 regions and front/rear/left/right sheep. Each row/column/region uses each symbol once. Fixed clues have a dot; editable cells offer a sheep chooser and Clear, with Undo/Hint/Restart. Hint waits for confirmation. Logical symbols are never randomly mirrored.

Maze remains in source/model/tests without a menu entry. Pushing grass and optional turn-based games against the sheep are V2 ideas, not V1 commitments.

## Reset and persistence

Settings → Habits → Reset Habits requires two independent confirmations, each initially Cancel. It deletes Habits history, custom definitions, sheep name, timers and relationship and restores the defaults above (mode On, Monday, portrait, first-habit Home focus, nine grass/five hearts). Reset is permanent after confirmation.

Only `/.crosspoint/habit_sheep.json`, `habit_sheep_state.json`, `habit_timers.json` and `habit_events` are replaced. CrossPoint settings, Display/Sleep Screen, Wi-Fi, OPDS, books and reading progress are preserved. Reset bypasses normal mode-toggle behavior so it does not change the sleep choice. Verified staging/journal markers allow interruption rollback before commit and cleanup after commit; a failed recovery blocks Habits writes until repaired/retried.

Events are append-style and totals are derived. Stock/history persist locally through the storage HAL. Bounded fixed-size puzzle state lives in its ActivityManager-owned activity; new screens use fallible allocation. No additional framebuffer or runtime bitmap buffer is introduced on ESP32-C3.

## Scope and validation

V1 works offline with no AI/backend/key. AI policy, multi-device sync and extra games remain future scope. See [public test guide](CROSSPOINT_SHEEP_PUBLIC_TEST.md) and [implementation/checklist](HABIT_SHEEP_PENDING.md). Unit/storage/model and host layout tests do not establish physical e-ink quality, battery life or resolution of the earlier freeze report.

CrossPoint remains a read-only upstream; preserve its reading capabilities and attribution. See [upstream policy](UPSTREAM_POLICY.md). No upstream submission is authorized.
