# Habit Sheep artwork

Original project artwork: 24 habit icons, 12 awake sheep poses, 4 rest poses,
2 eating poses, 4 face/leg variants for Sheep pairs, and care/interaction icons.
No third-party icon or character artwork is included. Sheep scenes contain no grass.
These assets follow the repository license.

SVGs are editable sources. Committed one-bit P4 PBMs are build inputs.
`python3 scripts/make_habit_art.py` regenerates PBMs using Inkscape and Pillow.
PlatformIO runs `scripts/build_habit_art.py` (Python standard library only)
to produce the ignored flash-array header. Inkscape/Pillow are not build dependencies.
