# Upstream policy

Habit Sheep is an independent downstream fork of CrossPoint Reader.

## Direction of change

The relationship is intentionally one-way:

```text
crosspoint-reader/crosspoint-reader (upstream)
                 |
                 | pull / merge updates
                 v
petereading/habit-sheep (downstream)
```

Habit Sheep may periodically import the latest compatible CrossPoint `develop` changes. Habit Sheep-specific commits are never pushed to the upstream CrossPoint repository.

## Rules

1. Do not push branches, tags, commits, or releases to `crosspoint-reader/crosspoint-reader`.
2. Do not open upstream CrossPoint pull requests for Habit Sheep features.
3. Treat upstream CrossPoint as read-only.
4. Keep Habit Sheep development in `petereading/habit-sheep`.
5. Sync upstream changes into the fork's `develop` branch, then merge/rebase Habit Sheep feature work inside this repository only.
6. Resolve upstream conflicts downstream. Do not require CrossPoint to carry Habit Sheep compatibility code.
7. Keep Habit Sheep additions modular and minimise edits to CrossPoint core files so future upstream merges stay small.
8. CrossPoint bug fixes discovered while developing Habit Sheep remain downstream unless the repository owner separately and explicitly decides to contribute a generic fix upstream.

This policy is a project invariant, not a temporary V1 workflow.
