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
means screenshots show the end state of animations (the Display tab reads "Motion: Reduced"). The `motion-*` scripts switch Settings -> Display to Motion: Full first (the setting is what the animations read) and capture mid-animation frames; the frame counts in their capture names are frames after the triggering click.

## Script language (`scripts/*.ui`)

One command per line, `#` starts a comment: `wait <frames>`, `move <x> <y>`, `click <x> <y>`, `wheel <dy>`,
`key <NAME>`, `capture <name>` (writes `<name>.png`; names must be unique across scripts). Coordinates are real
screen pixels; each script documents the layout it derives them from. The game screen is driven by name instead:
`clicksq <timeline> <half-turn> <square>` clicks a square (`hoversq` with the same arguments only moves the pointer there) (e.g. `clicksq 0 2 g1`) wherever the camera has put it, so
scripts do not depend on the zoom. `clicksq` also targets the Guide's page. `mode <catalog id>`, `position <file.5dp>` and `record <file.5dr>` (relative to the script's directory,
e.g. `../positions/check.5dp`, `../records/branch.5dr`) open a game directly; follow them with `wait`. `slot <1-3> <file>` puts a file's text into a
save slot (saves live in memory under the harness, never in the config directory, and the date is fixed), so the Load screen can be shown with chosen contents. The harness runs with defaults, so every
game screenshot shows the Pixel pieces in the Atlas board view unless the script changes it in Settings. A `click` is hover frame, press frame,
release frame. After a click that changes the camera (new boards appear) wait ~40-90 frames before the next one.
Use `capture` after enough `wait` frames for scene cross-fades (about 10 frames under Reduce motion).

Camera commands (see the header of `tools/ui_script.cpp`): `dblclick x y`, `dblclicksq <l> <t> <sq>`, `drag x0 y0 x1 y1 [steps]`, `movesq` and `clickcard <l> <t>` (the label strip of a card);
`zoom?` prints the camera of the game screen (state, zoom, target, moving) to the log and takes no frame; `camlog on|off` prints it every frame;
`camstill on|off` fails the script when the camera moves on any frame in between ("the camera stays still mid-turn"); `camexpect state <Overview|Focus|Free>`, `camexpect zoom <min> <max>`,
`camexpect visible <l> <t> [fraction]`, `camexpect squareat <l> <t> <sq> <x> <y>|pointer` and `camexpect nextboard <0|1>` fail the script (exit 2) when the camera is not where the script says. Key names include `HOME`, `PLUS`, `MINUS` and `SHIFT_TAB`.

`ainodes <n>` sets how many nodes of the computer opponent's search run each frame (400 until changed; 0 freezes it where it is,
to capture the "thinking" HUD; `clock` is the shipped wall-clock budget, not reproducible, for `FDCHESS_PERF=1` measurements) and
`waitai` runs frames until the computer has nothing left to do (it has replied and the game's own legal-turn search finished; the On a puzzle it waits for the judging of a submitted turn, the proof of a mate in 2, the engine's reply and a scripted solution.
script fails after 5000 frames). A game with the computer is started through the real menus (Versus, Opponent: Computer, side, level,
mode, Play) or by a record with the `# vs-computer:` line (`record`, `slot`).

| script | captures |
| --- | --- |
| main-menu | menu at rest |
| settings | tabs; Piece Theme with each of the six themes selected in turn (Pixel, Medieval, Bauhaus, Neon, Origami, Ink); Music; Display |
| versus | mode list (Two players, with the Opponent row); Standard selected, Play visible |
| vs-setup | Opponent: Computer in the Setup panel with the side and level rows and the level's description; Random / Hard; Black / Easy; Standard selected; back to Two players (the panel shrinks to Opponent; the mode list stays) |
| vs-play | vs Computer, Easy, as White: start, e2-e4 and Submit, the "Computer is thinking" HUD frozen mid-search, a click on the computer's pawn ignored, the computer's reply, the player in control again |
| vs-undo | Undo against the computer: disabled at the start; after a round; an unsubmitted move first; then the whole round (player's turn and the reply); while the computer thinks (search cancelled) |
| vs-save | the Load screen with two vs-Computer slots; loading a record as Black (the computer, White, has opened); a reply; Continue resumes the vs-Computer game from the autosave; Undo there |
| game-standard | e2 selected, e2-e4, submit, black e7-e5, submit, knight time-travel jump creating a timeline (a branch connector and a jump arc) |
| game-battle | Time Line Battle: start, pawn selected, three moves, submitted, black move |
| endgame | Time Line Fragment: a two-turn checkmate; "Black wins!" / "Checkmate" card |
| nav-toggle | H hides and restores the nav buttons (Esc no longer does: it cancels a selection); none left hovered |
| stalemate | Time Line Fragment: a six-turn stalemate through time travel; "Draw" / "Stalemate" card |
| board-styles | Settings -> Display -> Board view cycled through Atlas (default), Blueprint and Deep space; the same Standard mid-game (knight picked up) in each |
| medieval, bauhaus, neon, origami, ink | Piece Theme -> that theme chosen in Settings; the same Standard mid-game as board-styles with its pieces in Atlas, Blueprint (greyscale) and Deep space |
| guide | main menu -> Guide, every page (Right arrow) and the "Try it" goal played on most of them: selected piece with its dots, the check mark, the mate card, the promotion picker; Left arrow goes back; the last page's Standard-game button |
| save-continue | no Continue on a fresh menu; after a submitted turn Continue appears, resumes the game, and an unsubmitted move is not restored |
| save-slots | the game's Save panel with the unsubmitted-moves warning, the saved slot, the Load screen, and the loaded game |
| load-records | the Load screen with a branch record and a corrupted one ("This save can't be loaded"), Delete with confirmation, loading the branch game |
| puzzles-list | main menu -> Puzzles: the list, three tiers, no check marks |
| puzzles-open | the first puzzle opened from the list; Hint (text), then Show piece (the rook ringed) |
| puzzles-wrong | a wrong move (Re1-e2, submitted): "Not quite - try again" with Try again; the position is still there 3 s later (no automatic reset), Try again resets it |
| puzzles-solve | the solution Re1-e8: the success card, then the list with a check mark |
| puzzles-refuted | a first turn of a mate in 2 that does not win by force: Black's refuting defence is played, then "Not quite - Black has a defence" |
| puzzles-mate2 | a mate in 2: the engine's reply played on the board and the second turn awaited; Show solution replayed from the start |
| layout-embedded | the Guide and a puzzle: the pill, action row and controls bar centred on the board view beside the side panel; the Submit tooltip listing the pending move; the controls bar stays on a fresh game however long it is idle (it fades after 3 idle seconds only once a turn has been handed over) |
| layout-banner | Motion: Full: the turn change after Submit 3, 9, 24, 48 and 90 frames in (the pill's hint segment cross-fades to "Black to move"; nothing covers the ruler) |
| layout-tooltips | tooltips: none before the 400 ms dwell, then Submit (the moves), Undo, the L0 lane pill, a ruler tick |
| layout-long-labels | the longest slot row ("Simplify - Knight vs Bishop vs Computer" with its turns and date) in the Load screen and the Save panel |
| motion-shake | Motion: Full: a refused click (Black's pawn on White's turn) mid-shake and settled, a click on a history board |
| motion-travel | Motion: Full: the present marker sliding after a move, the boards lifting on Submit, the hover preview of the knight, the live arc to a time-travel target, the halo on the board the knight left |
| motion-lane | Motion: Full: Black's jump creating L-1, its lane unfolding from L0 |
| motion-check | Motion: Full: Ra8+ submitted, the checked king pulsing and the attack line drawing on |
| motion-screens | Motion: Full: a screen sliding in (back and forward) and a click during the cross-fade reaching the new screen |
| camera-click-focus | a click on a square below zoom 0.8 brings the board up and picks the piece; Esc steps back (selection, then the framing); a click on a card's label strip and on a board that is history focus it |
| camera-dblclick | double-click on a board: Focus, again: Overview; on empty canvas: Overview |
| camera-wheel-drag | the wheel zooms around the pointer (the square under it stays), limits 0.4 and 2.5; a drag pans at once and Home returns |
| camera-space-cycle | Time Line Battle: Space / Tab / Shift+Tab cycle through the three boards that need a move; Home |
| camera-keyboard | the board cursor with the arrow keys, Enter focuses, + / - zoom, Esc deselects, U undoes, Enter submits |
| camera-timetravel | a knight's time-travel jump keeps its source and the new timeline's board in view; Submit focuses Black's one board |
| camera-submit-multi | Submit with three mandatory boards frames the present column (Overview) |
| camera-still-midturn | `camstill`: from Black's board being focused to the Submit the camera does not move (selecting, switching pieces, moving, Undo) |
| camera-vs-computer | vs Computer as Black: the computer opens, the camera frames the player's board; the reply is framed too |
| rules-visuals | tests/ui/positions: check (attack line from the rook to the king), an inactive timeline (dimmed, tagged), the promotion picker and a chosen promotion |

## Layout audit

Every text drawn through a button, the HUD (pill, controls bar), a side panel, the lane / card / ruler labels or a centred title reports
its measured size (`MeasureTextEx` with the real fonts) and the box it must fit in (`include/ui/Audit.h`). A text that does not fit prints
`UI-OVERFLOW ...` to the script's log and `run.sh` then exits non-zero (even when every screenshot matches); a label that had to be drawn
smaller than its nominal size prints a `UI-SHRUNK` note. The log's last line, `UI-AUDIT <n> texts checked`, shows the audit ran. The
game's canvas is 1400x800 on every platform (the web page scales it with CSS), so this is also the web build's layout.

## Overlap audit

HUD and chrome elements register their screen-space rectangle as they draw (`ui::audit::rect`, in `include/ui/Audit.h`): every button (a scrolling list registers its viewport instead of its rows), the status pill, controls bar, Present badge, ruler, minimap, side panels (mode select, Guide, puzzles), the Save panel, the HUD zone (`[0, UI::Layout::hudBottom)`) and the top edges of the present column and the lane bands. At the end of each frame every pair that intersects, or comes closer than the clear space of either (1 px, 8 px around the Present badge), prints `UI-OVERLAP <a> vs <b> (<rects>)` unless an explicit rule allows it (a button inside its panel, the badge on the ruler, the Save panel dropping over the scene). Lane bands and the present column are only checked against the HUD zone and the ruler: that is where the scene must not enter. `run.sh` fails on any such line and lists them with their script. Add a rule to `kAllowed` in `src/ui/Audit.cpp` only for a deliberate overlap; call `ui::audit::rect` after `ui::audit::enabled()` so a shipped build builds no names. The log ends with `UI-AUDIT-OVERLAP <n> rects over <m> frames`.

## Tolerance

A pixel differs when any RGB channel differs by more than 8 (`--channel-threshold`); an image fails when more than
0.2 % of its pixels differ (`--max-diff-percent`). This absorbs sub-LSB rasteriser or MSAA differences between
Mesa versions while any moved, resized, recoloured or re-worded element (hundreds to thousands of pixels) fails.
Same-machine runs are byte-identical; the tolerance is only for cross-environment drift.
