# Habit Sheep V1 Product & Architecture Spec

Habit Sheep is an e-ink habit companion and virtual sheep built as a thin fork of CrossPoint Reader. It preserves CrossPoint's reading experience while making three user-selected habits and a persistent sheep companion immediately accessible from Home.

## Product principles

- CrossPoint remains a reader. Habit Sheep must not make reading feel compulsory.
- The sheep and habit system must be fully useful offline and without AI.
- Positive reinforcement only: no starvation, illness, death, punishment, guilt, or loss of pasture for missed habits.
- Keep the low-end ESP32-C3 devices as the baseline. Touch and higher-memory devices may add richer interactions without changing core functionality.
- Minimise friction: most daily actions should be possible directly from Home.
- Preserve upstream CrossPoint navigation and reader capabilities wherever practical.

## Habits

- Users may save up to 9 habits in a Habit Library.
- Up to 3 habits are Active at one time and shown on Home.
- Active habits can be swapped without deleting their history.
- Long-pressing an Active habit opens a replacement picker.
- Active habits can also be managed in Settings / Web Settings.
- No automatic weekday/weekend scheduling in V1. Users can manually swap habits for weekends, holidays, travel, etc.
- V1 habit types:
  - Completion: complete once for the configured period/day.
  - Duration: accumulate minutes toward a target.
- Duration habits support Start, Pause, Stop, and manual +minutes logging.
- While a Duration timer is actively running, automatic sleep is prevented.
- E-ink does not need a per-second redraw. Time is calculated from timestamps; the screen may refresh at a low cadence and immediately after input.

## Reading is optional

Reading is not a required or default obligation. The CrossPoint dock always provides quick access to reading, independent of the user's three Active habits.

A Duration habit may optionally enable CrossPoint Reading integration. When enabled, selecting that habit provides:
- live manual timer logging for paper books or other reading;
- manual +minutes logging;
- Continue Reading;
- Browse Files / books;
- automatic CrossPoint active-reading events when that capability is implemented.

If no habit has Reading integration, CrossPoint reading remains normal and does not create a daily reading obligation.

## Habit event model

Store append-style events rather than only mutable daily totals so later multi-device sync is possible.

Each event should have, at minimum:
- stable event ID;
- stable habit ID;
- timestamp;
- amount;
- unit / event type;
- source.

Candidate sources include manual completion, manual timer, manual adjustment, and CrossPoint reader. Daily/weekly totals are derived views.

## Habit Sheep Home

Habit Sheep Home is a dedicated Home mode, not merely a global UI theme. Users should still be able to use the normal CrossPoint styling on Library, Settings, Reader, etc.

Home hierarchy:
1. Sheep + persistent pasture: the largest visual area.
2. Up to 3 Active habits.
3. Bottom icon dock containing CrossPoint's existing major functions.

The dock reuses CrossPoint's existing monochrome icons but removes text labels. Typical entries include:
- open book: Continue Reading;
- folder: Browse Files;
- books: OPDS Browser;
- other major CrossPoint functions such as transfer/settings as appropriate.

Button-only navigation:
- Up / Down cycles through interactive Home targets, including sheep, Active habits, and dock icons.
- Confirm activates the selected target.
- Existing quick reading access should remain low-friction.

Touch devices:
- Direct tap on sheep, habits, and dock icons.
- Long-press Active habit to replace it.

## Sheep identity and relationship

- Users name their sheep during onboarding and may rename it later in Settings.
- Sheep appearance is customisation, not tied to habit category.
- Ordinary sheep interaction does not count as habit completion.
- Habit completion contributes to care/growth/pasture.
- Sheep interaction contributes to relationship/bond.
- Bond does not need a visible numeric score. It is expressed through behaviour, e.g. looking at the user, approaching, resting nearby, or running over when called.
- No penalty for not interacting.

## Sheep life and pasture

The sheep has lightweight autonomous ambient behaviour such as sleeping, grazing, sitting, wandering, playing, approaching the user, and finding a clover.

Do not simulate continuously while the device is powered off. Persist the last state/time and advance coarse state from elapsed time on wake.

The pasture is persistent and grows through positive habit activity. Decorations may be unlocked over time (flowers, tree, rocks, pond, butterflies, sheep house, etc.). Missing a habit does not remove progress.

Short optional e-ink-friendly interactions/minigames may be added without coins, XP, daily quests, leaderboards, or endless loops.

## Sleep screen

Habit Sheep provides a dedicated static sleep scene using the e-ink panel's image retention.

The sleep scene should prioritise:
- sheep + persistent pasture;
- Active habit progress;
- date;
- coarse battery icon.

Do not show information that falsely appears live while the device is off, such as a running clock or countdown.

Timed wake may later update daily rollover / coarse sheep state / sleep scene and return to sleep without Wi-Fi.

## Battery display

Habit Sheep never displays a numeric battery percentage.

Use approximately 10 coarse battery icon states, changing at roughly 10% boundaries. This avoids implying precision that the battery estimate cannot justify. Charging may add a charging indicator.

Use the same rule on Home and sleep screens.

## Optional AI Brain

AI is optional enhancement only. No API key means a complete product.

Architecture:
- local RulePolicy always available;
- optional remote behaviour policy;
- provider adapters for OpenCode Zen and OpenRouter;
- BYOK through Web Settings;
- no Habit Sheep-owned backend and no user-deployed Worker.

API keys:
- stored locally;
- never returned in full by the Web UI/API after saving;
- replace/remove controls;
- HTTPS only;
- never log keys;
- recommend a dedicated low-limit key.

Remote AI should receive only minimal behavioural state where possible, not habit names or unnecessary personal text. It chooses from a bounded action vocabulary; invalid/failed responses immediately fall back to local rules. AI must never block Home, habit logging, sleep, or reading.

## Upstream relationship

CrossPoint is a read-only upstream. Habit Sheep only pulls/merges upstream changes into this repository; Habit Sheep-specific commits are never pushed back upstream. See `docs/UPSTREAM_POLICY.md`.

## Initial engineering direction

Keep the fork close to upstream CrossPoint:
- dedicated Habit Sheep modules for habit state/events, sheep state/pasture, behaviour policy, and UI;
- a Habit Sheep Home UI component owned by the existing HomeActivity, keeping ActivityManager/Home semantics upstream-compatible while avoiding unrelated screen changes;
- reuse CrossPoint input abstraction, icons, reader navigation, web settings, storage/HAL, and sleep infrastructure;
- keep device-specific code behind existing HAL/capability boundaries;
- minimise RAM use and avoid full-screen duplicate buffers on ESP32-C3.

V1 implementation should proceed in small independently testable slices. Do not open a pull request or trigger expensive CI repeatedly during early development.
