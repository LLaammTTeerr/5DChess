# UI screenshot tests

A safety net for UI refactors: the real app (same `SceneManager`, scenes and menus as `src/main.cpp`, through
`App::frame`) is driven by scripts with injected input, screenshots are taken at 1400x800 and compared with the
committed baselines in `baseline/`.

## Run

```
tests/ui/run.sh                 # build ui_script if needed, run every scripts/*.ui under xvfb, compare
tests/ui/container.sh           # same, inside ubuntu:24.04 (identical to the CI `ui` job)
```

Needs `xvfb-run` and Python 3 with Pillow. Results go to `ui-out/run/*.png`; for every mismatch a red-overlay diff
goes to `ui-out/diff/`. Exit status is non-zero on any mismatch, missing or new image.

## Re-baseline (intentional UI change)

```
tests/ui/container.sh --update   # preferred: baselines are produced in the CI environment
git diff --stat tests/ui/baseline
```

Look at the changed PNGs before committing. `tests/ui/run.sh --update` does the same locally, but local Mesa may
differ slightly from CI, so baselines should come from the container. PNGs are optimised with `optipng` when
available, otherwise Pillow.

## How determinism is achieved

`TestMode` (include/TestMode.h, set only by `tools/ui_script.cpp`): fixed 1/60 s timestep, Reduce motion forced
on, audio device never opened, RNG seeded. All pointer, keyboard and clock reads in the game go through
`Input::` (include/Input.h), which reads raylib normally and a scripted state in test mode. The game has no
`rand()`-style random source; blinks and the menu field are pure functions of the (fixed) clock. Reduce motion
means screenshots show the end state of animations (the Display tab reads "Motion: Reduced").

## Script language (`scripts/*.ui`)

One command per line, `#` starts a comment: `wait <frames>`, `move <x> <y>`, `click <x> <y>`, `wheel <dy>`,
`key <NAME>`, `capture <name>` (writes `<name>.png`; names must be unique across scripts). Coordinates are real
screen pixels; each script documents the layout it derives them from. A `click` is hover frame, press frame,
release frame. After a click that changes the camera (new boards appear) wait ~40-90 frames before the next one.
Use `capture` after enough `wait` frames for scene cross-fades (about 10 frames under Reduce motion).

| script | captures |
| --- | --- |
| main-menu | menu at rest |
| settings | tabs; Piece Theme with Pixel selected; Music; Display |
| versus | mode list; Standard selected, Play visible |
| game-standard | e2 selected, e2-e4, submit, black e7-e5, submit, knight time-travel jump creating a timeline |
| game-battle | Time Line Battle: start, pawn selected, three moves, submitted, black move |
| endgame | Time Line Fragment: a two-turn checkmate; "Black wins!" / "Checkmate" card |
| stalemate | Time Line Fragment: a six-turn stalemate through time travel; "Draw" / "Stalemate" card |

## Tolerance

A pixel differs when any RGB channel differs by more than 8 (`--channel-threshold`); an image fails when more than
0.2 % of its pixels differ (`--max-diff-percent`). This absorbs sub-LSB rasteriser or MSAA differences between
Mesa versions while any moved, resized, recoloured or re-worded element (hundreds to thousands of pixels) fails.
Same-machine runs are byte-identical; the tolerance is only for cross-environment drift.
