# UI screenshot tests

A safety net for UI refactors: the real app (same `ScreenStack` and screens as `src/main.cpp`, through
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
on, audio device never opened, RNG seeded, the computer opponent's search stepped a fixed number of nodes per frame
(`aiNodesPerFrame`, so the frame at which it finishes does not depend on the machine's speed). All pointer, keyboard and clock
reads in the game go through `Input::` (include/Input.h), which reads raylib normally and a scripted state in test mode. The only
random source is the seed of a new game against the computer (raylib's generator, which the harness seeds); blinks and the menu
field are pure functions of the (fixed) clock. Reduce motion
means screenshots show the end state of animations (the Display tab reads "Motion: Reduced").

## Script language (`scripts/*.ui`)

One command per line, `#` starts a comment: `wait <frames>`, `move <x> <y>`, `click <x> <y>`, `wheel <dy>`,
`key <NAME>`, `capture <name>` (writes `<name>.png`; names must be unique across scripts). Coordinates are real
screen pixels; each script documents the layout it derives them from. The game screen is driven by name instead:
`clicksq <timeline> <half-turn> <square>` clicks a square (e.g. `clicksq 0 2 g1`) wherever the camera has put it, so
scripts do not depend on the zoom. `clicksq` also targets the Guide's page. `mode <catalog id>`, `position <file.5dp>` and `record <file.5dr>` (relative to the script's directory,
e.g. `../positions/check.5dp`, `../records/branch.5dr`) open a game directly; follow them with `wait`. `slot <1-3> <file>` puts a file's text into a
save slot (saves live in memory under the harness, never in the config directory, and the date is fixed), so the Load screen can be shown with chosen contents. The harness runs with defaults, so every
game screenshot shows the Pixel pieces in the Atlas board view unless the script changes it in Settings. A `click` is hover frame, press frame,
release frame. After a click that changes the camera (new boards appear) wait ~40-90 frames before the next one.
Use `capture` after enough `wait` frames for scene cross-fades (about 10 frames under Reduce motion).

`ainodes <n>` sets how many nodes of the computer opponent's search run each frame (400 until changed; 0 freezes it where it is,
to capture the "thinking" HUD; `clock` is the shipped wall-clock budget, not reproducible, for `FDCHESS_PERF=1` measurements) and
`waitai` runs frames until the computer has nothing left to do (it has replied and the game's own legal-turn search finished; the
script fails after 5000 frames). A game with the computer is started through the real menus (Versus, Opponent: Computer, side, level,
mode, Play) or by a record with the `# vs-computer:` line (`record`, `slot`).

| script | captures |
| --- | --- |
| main-menu | menu at rest |
| settings | tabs; Piece Theme with each of the six themes selected in turn (Pixel, Medieval, Bauhaus, Neon, Origami, Ink); Music; Display |
| versus | mode list (Two players, with the Opponent row); Standard selected, Play visible |
| vs-setup | Opponent: Computer with the side and level rows and the level's description; Random / Hard; Black / Easy; Standard selected; back to Two players (the rows go, the list moves up) |
| vs-play | vs Computer, Easy, as White: start, e2-e4 and Submit, the "Computer is thinking" HUD frozen mid-search, a click on the computer's pawn ignored, the computer's reply, the player in control again |
| vs-undo | Undo against the computer: disabled at the start; after a round; an unsubmitted move first; then the whole round (player's turn and the reply); while the computer thinks (search cancelled) |
| vs-save | the Load screen with two vs-Computer slots; loading a record as Black (the computer, White, has opened); a reply; Continue resumes the vs-Computer game from the autosave; Undo there |
| game-standard | e2 selected, e2-e4, submit, black e7-e5, submit, knight time-travel jump creating a timeline (a branch connector and a jump arc) |
| game-battle | Time Line Battle: start, pawn selected, three moves, submitted, black move |
| endgame | Time Line Fragment: a two-turn checkmate; "Black wins!" / "Checkmate" card |
| nav-toggle | ESC hides and restores the nav buttons; none left hovered |
| stalemate | Time Line Fragment: a six-turn stalemate through time travel; "Draw" / "Stalemate" card |
| board-styles | Settings -> Display -> Board view cycled through Atlas (default), Blueprint and Deep space; the same Standard mid-game (knight picked up) in each |
| medieval, bauhaus, neon, origami, ink | Piece Theme -> that theme chosen in Settings; the same Standard mid-game as board-styles with its pieces in Atlas, Blueprint (greyscale) and Deep space |
| guide | main menu -> Guide, every page (Right arrow) and the "Try it" goal played on most of them: selected piece with its dots, the check mark, the mate card, the promotion picker; Left arrow goes back; the last page's Standard-game button |
| save-continue | no Continue on a fresh menu; after a submitted turn Continue appears, resumes the game, and an unsubmitted move is not restored |
| save-slots | the game's Save panel with the unsubmitted-moves warning, the saved slot, the Load screen, and the loaded game |
| load-records | the Load screen with a branch record and a corrupted one ("This save can't be loaded"), Delete with confirmation, loading the branch game |
| puzzles-list | main menu -> Puzzles: the list, three tiers, no check marks |
| puzzles-open | the first puzzle opened from the list; Hint (text), then Show piece (the rook ringed) |
| puzzles-wrong | a wrong move (Re1-e2, submitted): "Not quite - try again" with Try again, then the automatic reset |
| puzzles-solve | the solution Re1-e8: the success card, then the list with a check mark |
| puzzles-mate2 | a mate in 2: the engine's reply played on the board and the second turn awaited; Show solution replayed from the start |
| rules-visuals | tests/ui/positions: check (attack line from the rook to the king), an inactive timeline (dimmed, tagged), the promotion picker and a chosen promotion |

## Tolerance

A pixel differs when any RGB channel differs by more than 8 (`--channel-threshold`); an image fails when more than
0.2 % of its pixels differ (`--max-diff-percent`). This absorbs sub-LSB rasteriser or MSAA differences between
Mesa versions while any moved, resized, recoloured or re-worded element (hundreds to thousands of pixels) fails.
Same-machine runs are byte-identical; the tolerance is only for cross-environment drift.
