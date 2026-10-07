# Changelog

All notable changes to this project are documented in this file.
The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Fixed
- **Game screen: nothing enters the HUD zone.** The present column and the lane bands no longer run up behind the Undo / Deselect / Submit row, and the "Present" badge no longer sits directly under it: the HUD zone (Back / Save / Copy, the status pill, the action row, Overview / Next board) ends at `UI::Layout::hudBottom`, the turn ruler starts 8 px below it, and the scene (and the camera's safe rect) below the ruler. The present column now hangs from the middle of its badge. Disabled buttons of the Deep space view are opaque, so nothing reads through them.
- Also found by the new overlap audit: the status pill could run under the Back button of the Guide and puzzle screens (it now stops short of it); the Present badge could lie over the minimap at the right end of the ruler (it stops before it); the minimap's frame could poke out of the ruler on many timelines; the Save panel reached 17 px under the Undo button (narrower now); the embedded Guide and puzzle boards drew their first frame without the side panel's inset.

### Changed
- **Mode select** has two columns: a Setup panel at the left (Opponent, You play and Level, each label above its row of choices, the level's description inside the panel; with Two players it holds only Opponent) and the list of game modes at the right in its own panel, whole rows only, clear of the Back / Play row and the footer. The mode list no longer moves when the opponent changes.

### Added
- **Overlap audit.** Under the UI test harness the buttons, status pill, controls bar, Present badge, ruler, minimap, side panels, popups, list viewports, the top edge of the present column and of the lane bands, and the HUD zone register their screen-space rect as they draw (`ui::audit::rect`, guarded by `ui::audit::enabled()`, so a shipped build pays one branch). A pair that intersects, or comes closer than its clear space (1 px; 8 px around the Present badge), without an explicit allowed-containment rule (a button inside its panel, the badge on the ruler) prints `UI-OVERLAP a vs b`, and `tests/ui/run.sh` fails on any such line.

## [1.1.0] - 2026-10-07

A pass over the game from the point of view of the person playing it: nothing overlaps or spills, the camera stays calm and readable, and motion explains each move.

- **Calmer camera**: Overview, Focus and your own framing; it never moves by itself mid-turn. Click a small board to focus it, double-click for the overview, Space for the next board that needs a move, Home for everything, arrow keys for a board cursor. The wheel zooms around the pointer.
- **Layout**: twelve overlap and overflow issues addressed (turn banner, Present badge, side-panel screens, jump labels, lists, save slots), tooltips, clearer Undo and Submit, and an end-of-game card with Rematch and Review. Every screenshot test now fails on overflowing text.
- **Motion and sound**: illegal clicks shake with a reason, hover previews legal moves, time travel draws a live arc, new timelines unfold, check pulses; Esc steps back, H hides the menu.

### Added
- **Layout audit.** Under the UI test harness every text drawn through a button, the HUD, a panel, the lane / card / ruler labels or a centred title reports its measured size and the box it must fit in (`include/ui/Audit.h`); anything that overflows prints a `UI-OVERFLOW` line and `tests/ui/run.sh` fails (a label drawn smaller than nominal prints a `UI-SHRUNK` note). `ui::ellipsized` (`include/ui/TextFit.h`) is the one cut-with-dots routine, unit tested.
- **Tooltips**: `ui::tooltip` (400 ms dwell, then a short fade; none under Reduce motion) on Submit (the moves it hands in, or why it is disabled), Undo, Deselect, the lane pills ("L+1: created by White at T3, branching from L0", plus why a timeline is inactive), the ruler ticks ("Turn 3, White"), jump badges (the text that no longer fits beside them) and the minimap. `ui::Button::tip` makes any button show one.
- **Cursor kinds** for the board code: `UI::Cursor::request(Hand | Grab | NotAllowed)`.
- **Minimap** at the right end of the ruler while part of the multiverse is off screen or tiny (one cell per board, the present column tinted, the visible part outlined); `BoardScene::minimap` / `minimapBoardAt` give the camera code its geometry.
- **End card buttons**: Rematch (same mode, side and level; fresh seed against the computer), Review board (dismisses the card, the pill keeps the result) and Back to menu; the scrim is lighter so the final position stays readable.
- **Undo label and badge**: "Undo move" (a move of this turn) or "Undo turn" (against the computer), with a count badge when the turn has several moves.
- HUD data for the camera: `HudData::cameraLabel`, `nextBoardLabel`, `rightInset`; `ActionRow` returns `Overview` / `NextBoard` (the buttons and the new controls line show as soon as the screen reports a camera label); `BoardScene::presentColumnX(t)` (world x of a fractional half-turn column) and `setPresentAt(t)` for the present-marker slide.
- **Feedback motion** on the board view (Reduce motion keeps the static parts and drops the animation):
  - a click that cannot do anything (an opponent's piece, a board that is history, a click on the boards while the computer thinks) shakes its card, flashes the square red and says why in red under the turn ruler (`Selection` returns a new `Rejected` intent with a reason);
  - hovering a piece you can pick up shows its targets as hollow dots after 120 ms, with dashed arcs to the other boards they lie on;
  - with a piece picked up, hovering a time-travel target draws its arc live, lights the target board and its ruler column and lane label;
  - a move that creates a timeline unfolds the new lane from its parent, starts the branch connector with it and lets the piece leave 120 ms later; the board a time-travel move left keeps an accent halo for 1.5 s;
  - a newly attacked king pulses twice and the attack line draws on from the attacker;
  - Submit slides the present column to its new place and lifts the boards of the side to move one after the other; a hovered card frame lifts 2 px.
- **Sound cues** (Kenney, CC0; they follow the sound-effects setting): Submit, a move that creates a timeline, a check, a refused click.
- Screens slide in 12 px in the direction of travel (from the right going forward, from the left going back) while the old screen fades; a pressed button scales to 0.98.
- Tests: `Selection` reasons and the feedback curves (`tests/selection_test.cpp`, `tests/feedback_test.cpp`); Motion: Full UI scripts with mid-animation captures (`motion-shake`, `motion-travel`, `motion-check`, `motion-screens`); `ui_script` gains `hoversq`.

### Changed
- The "White / Black to move" banner no longer covers the turn ruler: after a turn change the pill's hint segment cross-fades to it for 1.2 s (and it is skipped while the computer is to move).
- The status pill, the action row and the controls bar centre on the board view, not the window, beside the Puzzle and Guide panels; the pill cuts its hint with "..." rather than run under the panel. After the first handed-over turn the controls bar fades out after three idle seconds (leaving a "Controls" chip) and returns when the pointer moves or nears the bottom.
- The "Present" marker on the ruler now names its turn ("Present . T3w") and no longer hides the turn label; the lane bands stop at the boards' area instead of running under the ruler.
- Jump arcs run through the gaps between boards (down into the lane gap, along it, up or down the column gap) instead of across the boards below; the jump badge slides along the arc to a spot no board lies under and its text label shows only where it covers nothing (otherwise it is the badge's tooltip). Lane sub-lines, tags and card labels are at least 12-14 px; card labels never leave their card and, too small to fit, appear on the hovered board only.
- The mode list holds a whole number of rows (no row cut through its text) and fades where it continues; the load and save slot rows put the title at the left and "7 turns . date" muted at the right and cut only the title (labels no longer shrink before they are cut).
- The promotion picker opens above or below the board's card, on the side the promotion square is nearest, with a thin line to the square, instead of covering the neighbouring ranks.
- Settings: every tab has the same two columns (options at the left, what they do at the right).
- A wrong puzzle answer stays on the board until **Try again** is pressed (it used to reset itself after 2.4 s).
- **Esc** now cancels what is open (the promotion choice, then the picked-up piece) instead of hiding the navigation buttons; those move to **H**.
- The first click after a screen change is no longer swallowed by the cross-fade: it reaches the new screen.
- The legal-target dots of a long-range piece all appear within 150 ms (the stagger is capped; it was up to 300 ms).
- Click a card's frame or label, or a board that is history, to focus it; double-click a board to toggle Focus and Overview (double-click on empty canvas: Overview).
- Keyboard navigation: Home (Overview), Space / Tab / Shift+Tab (the next board that needs a move, Mandatory first), arrow keys (a board cursor with a focus ring), Enter (focus the cursor's board, then Submit), U (Undo), + / - (zoom).
- Ghost tags at the screen edge for the boards a picked-up piece can reach when they are off-screen; clicking one frames both boards.
- `BoardCamera` is graphics-free (`play_core`) and unit tested; new `BoardLayout::boardAt / find / neighbour / columnBounds / cardBounds`. `ui_script`: `dblclick`, `dblclicksq`, `drag`, `movesq`, `clickcard`, `zoom?`, `camlog`, `camstill` and `camexpect`; nine `camera-*` UI scripts.

- **Camera and navigation** (replaces the follow / fit / free camera): three framings, Overview, Focus and Free, plus a lock while a piece is picked up. The camera no longer moves on its own except where the player's next action needs a board that is not readable: Submit focuses the one board that needs a move (or frames the present column when several do), a time-travel move keeps the source and the new board in view, and a piece picked up on a board smaller than zoom 0.8 brings it up in the same motion as the click. It never refits in the middle of a turn, never flies back after 10 seconds and never pans to a selected piece. The jump arcs no longer count in the fit. Every move is one tween of the position and the zoom (260 ms focus, 360 ms overview, 200 ms hops; 80 ms linear under Reduce motion), and any drag, wheel or key interrupts it. Automatic zooms snap to 0.6 / 0.8 / 1.0 / 1.25; the wheel zooms around the pointer, 12 % a notch, between 0.4 and 2.5, with no accumulator; a drag pans at once (no 100 ms dead time) and never selects. The computer's moves follow the same rules 150 ms after each move.
- Esc also steps back the framing (promotion, then selection, then back to the Overview). The Z (auto-zoom) and X (fit) keys are gone; the minimap strip focuses the board you click.
- Release notes on GitHub now start with the version's CHANGELOG section (`scripts/changelog_section.sh`), followed by the list of merged PRs; the release workflow fails early if the section is missing.

## [1.0.0] - 2026-10-06

The first complete release: a computer opponent, puzzles, an interactive guide, save and load, and six original piece themes on top of the official-rules engine and the three board views of 0.5.0.

- **Play vs Computer** at three levels (Easy, Normal, Hard), as White, Black or Random, on every mode. The AI searches whole turns across timelines in small per-frame slices, so the game stays responsive on desktop and in the browser.
- **Puzzles**: fourteen original mates in one and two, from single-board warm-ups to time-travel and branching mates, every one proved by the engine.
- **Interactive Guide**: ten short rule lessons on live boards.
- **Save and load**: autosave with Continue, three slots, and copy/paste of game records.
- **Original art only**: six code-generated piece themes (Pixel, Medieval, Bauhaus, Neon, Origami, Ink) replace the three themes of unknown provenance; the default look is Pixel pieces on the Atlas board view.

### Added
- **Puzzles** (main menu -> Puzzles): 14 original puzzles in three tiers (5 Warm-up mates in 1, 4 Time travel mates in 1 on 2-3 timelines that need a move to another board, 3 mates in 2 and 2 "branching" mates in 1 in Deep; the mate-in-2 proof runs a few milliseconds per frame, with a progress status), each proven exhaustively with the official-rules engine, never the AI. The list shows title, goal and a check mark for solved ones; a puzzle opens on the normal multiverse board with a goal banner, **Hint** (text, then the piece to move ringed), **Reset** and **Show solution** (plays the stored line). Any mating turn is accepted; "Not quite - try again" resets the position; in a mate in 2 the engine first proves that every defence loses, then `ai::Search` (Hard, capped) chooses Black's reply, which is played on the board. Solving shows a success card (Next puzzle / Back to list) with a flourish that Reduce motion turns off. Puzzle games never touch the autosave; solved puzzles are saved in `puzzles.txt` next to `settings.txt` (localStorage `5dchess.puzzles` on the web). Files `assets/puzzles/*.5dp` (position format plus `goal`, `hint`, `difficulty`, `solution` lines; embedded in the web build through the asset manifest); format and authoring guide in [docs/PUZZLES.md](docs/PUZZLES.md).
- Tools / tests: `tools/puzzle_check` (built with the tests or tools) validates the set (`puzzle_check assets/puzzles`; `--mates` / `--mate2` examine a position) and runs as the `puzzle_check` ctest; `puzzles::forEachTurn` enumerates every legal turn (cross-checked against the unpruned enumeration and the full legal-turn proof). Unit tests for the puzzle metadata, the progress text and the shipped set (`tests/puzzle_test.cpp`); five UI scripts (`puzzles-*.ui`) with 11 new baselines (list, open, hint text and piece, wrong move and feedback and reset, success card, list with a check mark, a mate in 2 with the engine's reply, Show solution). `ui_script`'s `clicksq` also works on a puzzle's board.
- **Four new piece themes**, all original and generated by code (Pillow >= 10.1, deterministic, 256x256 RGBA, trilinear with mipmaps): **Bauhaus** (flat geometry, primary-colour accents), **Neon** (glowing glass-tube line art, cyan vs magenta), **Origami** (low-poly folded paper, ivory vs indigo) and **Ink** (sumi-e brush strokes on round tokens, vermilion on paper vs chalk on black lacquer). Generators `scripts/gen_<id>_theme.py`; Settings -> Piece Theme now lists Pixel, Medieval, Bauhaus, Neon, Origami, Ink. UI screenshots of each in the Atlas, Blueprint and Deep space views and in Settings.
- **Medieval piece theme** (Settings -> Piece Theme): original heraldic pieces, carved ivory with gold trim for White and dark walnut with brass trim for Black, crimson cloth on both (crossed-arch crown with cross, pearl coronet, portcullis tower, slit mitre with crosier, armoured horse head with plume, helmed foot soldier with shield). Drawn by `scripts/gen_medieval_theme.py` (Pillow >= 10.1, deterministic, 4x supersampled to 256x256 RGBA, trilinear filtering with mipmaps) into `assets/images/pieces/medieval/`; legible down to about 16 px and in the greyscale Blueprint view. The settings file accepts `theme = Medieval`; the main-menu creatures stay Pixel.
- Tests / tools: every piece theme's textures are checked against the asset manifest and disk (`tests/view_test.cpp`); new UI screenshots of Settings -> Piece Theme with Medieval and of a Standard game in each board view with Medieval pieces (`tests/ui/scripts/medieval.ui`).
- **Smoother piece art**: textures that are not pixel art (`filter=point`) are now sampled bilinearly, and trilinearly with mipmaps when their size is a power of two (the Medieval pieces, 256 px), so they stay clean when drawn small. Pixel is unchanged.
- **Interactive Guide** (main menu -> Guide): ten short pages, one rule each (boards and time, moving through time, timelines and who owns them, the present and active timelines, the four movement axes, pawns, check across timelines, checkmate and stalemate, special moves, your first game). Every page has a small live position you can play in the board view you chose (legal-move dots, Undo, Submit, the promotion picker) and, on nine of them, a "Try it" goal that shows a check mark when you reach it. Prev / Next, page dots, Left / Right arrow keys, Reset position and a button that starts a Standard game on the last page. The example positions are `assets/guide/*.5dp` (listed in the asset manifest, so the web build bundles them); the page texts and goal checks are in `include/guide/Guide.h`.
- Tests: every guide position loads, every goal is met by its intended move or turn and not by a wrong one (`tests/guide_test.cpp`); UI screenshots of every page and goal (`tests/ui/scripts/guide.ui`). `ui_script`'s `clicksq` also works on the Guide.
- **Save and load.** Every submitted turn is autosaved as a game record (`5dchess-record 1`, [docs/NOTATION.md](docs/NOTATION.md)) and the main menu gets **Continue** (top item, only while an unfinished autosave exists; it resumes the exact game: mode, history, side to move, present, branches). The game screen has **Save** (three slots, each listing mode, turns and date; the panel warns "Unsubmitted moves are not saved") and, on desktop, **Copy** (the record to the clipboard); the main menu has **Load game**, a Load screen with the slots (empty ones disabled), **Delete** (asks twice) and, on desktop, **Paste record**. Moves of the current, unsubmitted turn are never saved (a record holds submitted turns only). A save that is corrupted or oversized (more than 1 MiB) never crashes: the UI says "This save can't be loaded" and keeps the file. A game that ends removes its autosave.
- Files: `autosave.5dr` and `slot1.5dr` ... `slot3.5dr` in the config directory next to `settings.txt` (`$XDG_CONFIG_HOME/5dchess/`, `~/Library/Application Support/5DChess/`, `%APPDATA%\5DChess\`); in the browser the localStorage keys `5dchess.autosave`, `5dchess.slot1` ... Slot files carry a `# saved: <date>` comment line. `savegame::SaveStore` (`include/services/SaveStore.h`) with file and in-memory storages, unit tested (write / read / delete, corrupted and oversized files).
- Tools: `ui_script` commands `record <file.5dr>` (open a game from a record) and `slot <n> <file>` (fill a save slot); under the test harness saves live in memory only and the date is fixed, so screenshots stay deterministic. New scripts `save-continue`, `save-slots`, `load-records` and records in `tests/ui/records/`.
- Engine: an AI opponent, `ai::Search` (`include/ai`, `src/ai`; [docs/AI.md](docs/AI.md)). A resumable, single-threaded, seed-deterministic search for a whole legal turn (`step(nodeBudget)`, `bestTurn()`, progress info), so it runs in the web build and can be stepped a little per frame. Candidate turns are generated move by move with a beam (time jumps only when promising), searched by alpha-beta with iterative deepening over whole turns, with an exact mate/stalemate proof at every node; three levels (Easy, Normal, Hard) differ in depth, node limit and randomness. Hard node cap, fallback ladder and mate handling documented; tests in `tests/ai_test.cpp`; `tools/ai_bench` (`-DFDCHESS_BUILD_AIBENCH=ON`) measures speed and self-play.

- **Play vs Computer** (main menu -> Versus). The mode screen has an **Opponent** row (Two players / Computer); with the computer you pick **You play** (White, Black or Random) and a **Level** (Easy, Normal, Hard, with a one-line description each), then a game mode as before; the choices are remembered in the settings file (`opponent=`, `vs_side=`, `vs_level=`, stored by name, an unknown value keeps the default). On the computer's turn the game screen runs `ai::Search` a few milliseconds per frame (**6 ms native, 4 ms on the web**, in slices of 150 nodes; the engine stays clock-free; measured while Hard thinks: 5.5 ms average and 7.9 ms worst per frame native, worst `update()` 3.9-7.8 ms on the web, see docs/AI.md), shows "Computer is thinking" with three dots and a thin progress bar that follows `progress().fraction` (eased; still under Reduce motion), then plays the turn's moves one by one through the normal move animation and sounds and submits. The boards ignore clicks during its turn (the camera still pans and zooms; Esc and Back work), Submit and Deselect are disabled. **Undo** takes back your last turn and the computer's reply (just your turn while it thinks; the unsubmitted moves of your own turn go first, one at a time); the search is cancelled on Undo, Back and a new game. A game that has ended shows the normal end card. The search of a turn is seeded with the game's seed and the number of submitted turns, so a game is reproducible.
- Save / load for those games: the record gets one comment line `# vs-computer: you=<white|black> level=<easy|normal|hard> seed=<n>` ([docs/NOTATION.md](docs/NOTATION.md)); autosave, slots, Copy / Paste and Continue keep and restore the mode, the slot list says "Standard vs Computer - 7 turns", and records without the line (every older save) load as two-player games. Because it is a comment, older versions load such a record as an ordinary game.
- Engine-side helpers in `include/play/VsAi.h` (no graphics, unit tested in `tests/vsai_test.cpp`): names, which side the player gets (Random is decided by the seed), the search seed, the record line, and `replayPrefix` (a game rebuilt a few submitted turns back from its history, which is how vs-Computer Undo works since `IGame::undo` only knows the current turn).
- Tools: `ui_script` commands `ainodes <n>` (the search runs exactly n nodes a frame; 0 freezes it for a screenshot of the thinking HUD; `clock` is the shipped wall-clock budget) and `waitai` (run frames until the computer has replied); `TestMode::aiNodesPerFrame`. New scripts `vs-setup`, `vs-play`, `vs-undo` and `vs-save` and records `tests/ui/records/vs-*.5dr`.

### Changed
- The mode screen has the Opponent row above the list, so the mode list starts 30 px lower (Standard at y=184, was 154) and ends at y=680; the UI scripts' click coordinates and the two Versus baselines moved accordingly.
- Settings -> Piece Theme highlights the theme in use (the saved one) when the tab opens. `SettingsStore::apply` (pure, unit tested: a saved Modern/Classic/Fantasy keeps Pixel, `Medieval` switches) holds the settings-file rules.
- The SIL OFL 1.1 licence text for the bundled fonts is now shipped as `assets/fonts/OFL.txt` (installed with the assets, so in release packages, and embedded in the web build).
- Piece art moved to `assets/images/pieces/<id>/` (`Theme_3` is now `pieces/pixel`, `Theme_4` is `pieces/medieval`); the generators' default output paths follow. A new `Render/PieceThemes.h` holds the theme table without a graphics dependency, so the unit tests cover the name lookup.
- **New default look: Pixel pieces and the Atlas board view** (previously Modern pieces and Deep space). A saved settings file still wins, so players who chose a theme or view keep it; the Settings screen and the web build use the same defaults. UI baselines re-recorded; `board-styles.ui` now reaches Deep space by cycling.
- The main menu has a new **Load game** item between Versus and Puzzles (and **Continue** on top when there is an autosave), so the navigation column is taller; the UI scripts' click coordinates moved accordingly. Every game screenshot shows the new Save / Copy buttons under Back.

### Removed
- **Classic, Modern and Fantasy piece themes** (`assets/images/Theme_0` .. `Theme_2`): their provenance and licence were unknown. A settings file that still names one of them falls back to the default, Pixel.

## [0.5.0] - 2026-10-06

### Added
- **Three board views**, selectable in Settings -> Display -> *Board view*: **Deep space** (the default: an indigo night sky with a starfield and an aurora for the present, glowing boards on luminous threads, a dark HUD), **Atlas** (a warm paper map with tinted timeline lanes, soft card shadows and a terracotta present) and **Blueprint** (monochrome ink on off-white, orthogonal "subway" connectors, greyscale pieces, colour only for the present, the selection and check). One renderer draws all three from `BoardStyle` data (`include/play/BoardStyle.h`): lanes labelled in official notation (`L0`, `L+1`, `L-1`), a turn ruler on top (`T1 T2 ...` with w/b ticks), the present marker, board cards, branch connectors coloured by the player who branched, and a dashed arc with the moving piece for a time-travel jump. The HUD and controls bar follow the view; the menus stay cream.
- **Official-rules visuals**: a board's frame says whether you must move on it this turn (mandatory), may (optional) or it is history; boards on an inactive timeline are dimmed and tagged "inactive"; in check a line (pulsing, still under Reduce motion) runs from every attacker to the king, whose square is tinted.
- **Promotion picker**: a pawn reaching the last rank no longer auto-queens; a chooser (Queen, Rook, Bishop, Knight, drawn in the current piece theme; Q / R / B / N also work) opens next to the square.
- **Settings are saved**: piece theme, board view, music, sound effects and Reduce motion persist between runs (`$XDG_CONFIG_HOME/5dchess/settings.txt`, `~/Library/Application Support/5DChess/settings.txt`, `%APPDATA%\5DChess\settings.txt`, or the browser's localStorage on the web). The UI test harness never touches it.
- Engine: move notation and game records (`toNotation` / `parseMove`, `writeRecord` / `loadRecord`, e.g. `(L0T1)e2>(L0T1)e4`); a record replays through the engine and fails cleanly on malformed or illegal input ([docs/NOTATION.md](docs/NOTATION.md)). `IGame` keeps its move history. There is no save/load button in the app yet.
- Tools: `ui_script` commands `clicksq <timeline> <half-turn> <square>`, `mode <id>` and `position <file.5dp>`; `theme_preview --view <Deep space|Atlas|Blueprint>`; hand-made check, promotion and inactive-timeline positions in `tests/ui/positions/`.

### Changed
- **White's timelines are drawn above timeline 0 and Black's below it** (the official orientation; `L+1` used to be below `L0`).
- Selection: clicking another own piece switches the selection, clicking the selected piece deselects it, and clicking an empty square or an enemy piece no longer selects the board; a click on a non-target square no longer outlines that board. **ESC toggles the navigation buttons; Space no longer does.**
- Camera: it starts framed on the boards instead of gliding in from the world centre, fits the boards' cards into the free area between the HUD bars and the lane labels, and clips the boards to that area, so they no longer slide under the action row or the controls bar.
- The marching progression arrows are replaced by a per-timeline thread; only branch connectors and jump arcs are drawn between boards. Deep space animates (starfield, check pulse) only when motion is allowed.
- **The engine's x axis is now the displayed file** (a = 0 on the left, king on e1, as in 5d-chess-js); the "mirrored files" difference is gone. **Turns are 1-based**: `T1w` is the start board in `.5dp` files, `present:` is a turn label (`present: T3w`), and in notation. `.5dp` files written for 0.4.0 must be updated (mirror the rows, add 1 to every turn number); `size:` is capped at 8.
- Performance: `Piece` is a plain value and `Board` a fixed array of one-byte cells, so forking a board is a copy. Search runs at 2.2 vs 1.8-2.1 M nodes/s and 1000 random games take 42-46 s vs 55-56 s on the same machine ([docs/SEARCH.md](docs/SEARCH.md)).
- UI architecture rewritten (about 2,800 lines net removed, screenshots unchanged): one `Screen` per page (`MainMenuScreen`, `ModeSelectScreen`, `SettingsScreen`, `PlayScreen`) on a `ScreenStack` that applies navigation at frame end and cross-fades; a small widget layer (`include/ui/Widgets.h`: `Button`, `ButtonList`, `Toggle`, `ui::column` / `ui::row`) replaces the menu controllers, the Composite menu model, the `ICommand` classes, the GameState/Scene pairs and the SceneManager.
- The game screen is split into `play::` modules (`include/play`, `src/play`): `Selection`, `BoardLayout`, `MultiverseView` (caches the engine's rule answers once per game state), `BoardScene`, `BoardRenderer`, `MoveAnimator`, `BoardCamera`, `Hud`, `TimelineArrows`, `PromotionPicker`, orchestrated by `PlayScreen`. They replace `ChessModel` / `ChessView` / `ChessController` and `BoardView2D`.

### Fixed
- Web build: the main menu's drifting board field rendered as plain white blocks inside a white halo, because blending left the canvas's alpha below 1; every frame (and the cross-fade snapshot) now writes alpha = 1 back.

### Removed
- `Board::getPiece` / `PieceRef` (the UI reads pieces through `Board::at`) and the unused texture `ChessBoardNoBound.png`.
- The legacy `IGame` subclasses (`StandardGame`, `CustomGame*`, `MiscGame*`), `NameOfGame`, `Constant`, `createGame<T>` and the `Piece` class hierarchy: `GameCatalog` and the `.5dp` files are the only source of game modes.

## [0.4.0] - 2026-10-06

### Added
- Official 5D Chess rules engine ([docs/RULES.md](docs/RULES.md)) with check, checkmate and stalemate; Submit is enabled only when the turn is legal. Differential testing against the reference engine 5d-chess-js (`tools/refcheck`).
- UI screenshot test harness (`tests/ui`, baselines in `tests/ui/baseline`) and a CI job that runs it.
- MIT license.
- Engine value API: `Chess::Core::Coord` / `Chess::Core::Move` (hashable, comparable, pointer-free) and `IGame::legalMovesFrom`, `makeMove(Core::Move)`, `boardExists(Coord)`, `board()`, `selected()`; the old shared_ptr API still works.
- Position text format (`.5dp`, [docs/POSITIONS.md](docs/POSITIONS.md)) with parser/writer (`include/engine/Position.h`); `GameCatalog` loads the nine game modes from `assets/positions/*.5dp` and is the single mode registry for the menu, the scenes and the developer tools.

### Changed
- Internal: one `App` context (src/App.cpp) owns Assets, audio, settings, theme and scenes; `assets/manifest.txt` is the single list of loaded assets and also generates the web build's preload list. The ResourceManager/ThemeManager/UI::Fonts singletons are gone.
- CI: bumped actions/checkout (v7), cache (v6), upload-artifact (v7), download-artifact (v8) and softprops/action-gh-release (v3).
- The engine now implements the official 5D Chess rules ([docs/RULES.md](docs/RULES.md)): active and inactive timelines, the present, mandatory boards, check through time and across timelines, castling, en passant and promotion choice, with every rule traced to its source and compared move by move against the reference engine 5d-chess-js (`tools/refcheck`; Standard and Simplify modes; the three multi-timeline Misc modes have no reference mapping).
- Checkmate and stalemate end the game (the end card reads "White wins!" / "Black wins!" with "Checkmate", or "Draw" with "Stalemate"). Capturing a king no longer exists: such moves are never offered.
- The "does the side to move have a legal turn?" result search is non-blocking: it runs a little each frame, the HUD shows "Checking position..." meanwhile, and the game never freezes ([docs/SEARCH.md](docs/SEARCH.md)).
- Submit is enabled only when the turn is legal; when the turn would leave a king capturable the HUD says "Your king would be capturable". The present line is drawn at the present (the earliest end of the active timelines).
- The HUD and menu cache the turn status until the game state changes (`IGame::stateVersion()`) instead of recomputing it every frame.
- UI screenshot tests: the endgame script now plays a real two-turn checkmate and a new script a six-turn stalemate.

### Removed
- Unused assets (old buttons, backgrounds, `chess.png`, `images/EndGame.png`, Nunito and unused Montserrat fonts), for a smaller download.

### Fixed
- "Simplify - No Bishop" was a copy of "No Knight" (bishops, no knights). It now has knights and no bishops (`omit-bishop`: rnqknr / PPPPPP / RNQKNR on 6x6); the test `omit-bishop fix` pins this.
- Rules bugs found while aligning the engine with the reference: bishops, rooks, queens and kings could not move or slide forward in time (the king had no +t steps, the rook no +t slide, the bishop only -t, and queen +t slides were capped by the source board's turn number).
- Pawn timeline direction was reversed: a pawn's forward step on the timeline axis is towards the opponent's timelines (White towards lower IDs, Black towards higher), and timeline IDs follow the creator (White positive, Black negative).
- Moves were not required on every board of the present: a turn could be ended with boards unmoved, or with a king left capturable; this is now judged per turn (`canSubmit()`).

## [0.3.0] - 2026-10-05

### Added
- Motion system (`include/Render/Motion.h`): shared duration tokens (fast 120 ms / base 220 ms / slow 380 ms), easing, `Tween` and `Spring` helpers, and a global **Reduce motion** setting (Settings -> Display -> Motion: Full / Reduced) that makes animation instant or cross-fade only.
- In-game motion: moved pieces slide (same board) or arc across boards/time (travel move), captured pieces shrink and fade, new boards grow in, new branch arrows draw progressively, selected pieces lift, legal-target markers pop in with a stagger, a turn banner and HUD chip cross-fade announce the side to move, the end card springs in (Pixel theme: the winner's king hops). State and input never wait for animations; new input finishes them.
- Animated main menu: code-drawn hero (title, tagline, drifting field of timeline boards, idling Pixel creatures) replacing the raster splash; menu buttons and list items stagger in; scenes cross-fade; the selection indicator in lists and Settings glides between items.
- Pixel theme pieces blink at random 3-7 s intervals (`{side}_{piece}_blink.png`, generated by `scripts/gen_pixel_theme.py`), only when zoomed in.
- `theme_preview` options `--demo`, `--dump`, `--perf`, `--reduce` for animated captures and frame timing; `FDCHESS_PERF=1` logs game frame times.
- WebAssembly (Emscripten) build with a custom HTML shell, and a GitHub Pages deploy workflow that publishes it on each `v*` tag. The web build has no Exit menu item.
- Audio: `AudioManager` singleton with streamed background music (Settings -> Music, default Off, global across scenes), sound effects for moves, captures, game won and menu clicks, and a Sound effects on/off toggle. No-ops without an audio device; music starts after the first click (browser autoplay policy).

### Changed
- Replaced the background music and sound effects with CC0 / public-domain assets (credits in `assets/CREDITS.md`); the web build now has audio.
- The camera follows with a critically damped spring (frame-rate independent) instead of a lerp.
- `assets/5DChess.png` is no longer loaded or bundled in the web build.

### Fixed
- Web build audio: miniaudio needs `Module.HEAPF32`, now exported; the page no longer shows runtime errors under the title and has a favicon.
- The GitHub Pages workflow runs on `v*` tags (releases created with `GITHUB_TOKEN` do not trigger `release` events).
- New branch arrows now draw progressively at the correct length with intact dashes.
- The click that ends a scene cross-fade no longer also reaches the new scene.
- Boards re-created after an undo grow in again.
- The render texture of the scene cross-fade is released before the window closes.

## [0.2.0] - 2026-10-05

### Added
- Pixel piece theme (original pixel-art creatures) and its generator script (`scripts/gen_pixel_theme.py`).
- Restyled UI: colour and typography tokens, button states, in-game HUD with turn/timeline count and hints, controls bar, end-game overlay, clearer selection, legal-move and capture highlights.
- `theme_preview` developer tool (`-DFDCHESS_BUILD_TOOLS=ON`).

### Changed
- Boards are drawn from White's side (White at the bottom, standard orientation).
- The camera keeps boards clear of the HUD.
- The present line is a thin line.

### Fixed
- Theme names in Settings were mapped inconsistently.
- Fonts are now unloaded before the window closes.
- Stale hover tint on the board while the pointer is over the navigation menu.

## [0.1.0] - 2026-10-05

### Added
- CMake build; raylib is fetched automatically.
- doctest unit tests and property/fuzz tests for the engine.
- Optional ASan/UBSan build (`FDCHESS_SANITIZE`).
- GitHub Actions CI on Linux, macOS and Windows, and tag-driven release packaging.
- Dependabot configuration.

### Fixed
- Engine: bishop cross-timeline moves targeted the wrong board (crash for white).
- Engine: the queen could not reach the board edge.
- Engine: memory leak from `shared_ptr` cycles.
- Engine: dangling `initializer_list` (undefined behaviour) in queen move generation.
- Engine: the present turn could skip a timeline after a move onto a board ahead in time.
- Engine: undo after a king capture kept the game over.
- Engine: submitting a turn with no moves.
- UI: ESC closed the game.
- UI: selection state machine (null-board crash, moves on the wrong board, stale target board).
- UI: undo/submit left a stale selection.
- UI: clicks on menu buttons fell through to the board.
- UI: the camera could not reach later boards.
- UI: missing arrowheads/diamonds (triangle winding).
- UI: colour overflow in pulsing arrows.
- UI: disabled buttons vanished.
- UI: settings theme and music shared one index.
- UI: scenes were initialised twice.
- UI: Exit skipped cleanup and never called `CloseWindow`.
- UI: assets are now resolved relative to the executable.
- UI: hovering the navigation menu froze the game.

### Removed
- Dead and unused files and code.
- Tracked `.DS_Store` and `.vscode` files.

[Unreleased]: https://github.com/LLaammTTeerr/5DChess/compare/v1.1.0...HEAD
[1.1.0]: https://github.com/LLaammTTeerr/5DChess/compare/v1.0.0...v1.1.0
[1.0.0]: https://github.com/LLaammTTeerr/5DChess/compare/v0.5.0...v1.0.0
[0.5.0]: https://github.com/LLaammTTeerr/5DChess/compare/v0.4.0...v0.5.0
[0.4.0]: https://github.com/LLaammTTeerr/5DChess/compare/v0.3.0...v0.4.0
[0.3.0]: https://github.com/LLaammTTeerr/5DChess/compare/v0.2.0...v0.3.0
[0.2.0]: https://github.com/LLaammTTeerr/5DChess/compare/v0.1.0...v0.2.0
[0.1.0]: https://github.com/LLaammTTeerr/5DChess/releases/tag/v0.1.0
