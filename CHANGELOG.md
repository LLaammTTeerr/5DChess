# Changelog

All notable changes to this project are documented in this file.
The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added
- Move notation and game records: `toNotation` / `parseMove`, `writeRecord` / `loadRecord` (`include/engine/Notation.h`), e.g. `(L0T1)e2>(L0T1)e4`; a record replays through the engine and fails cleanly on malformed or illegal input ([docs/NOTATION.md](docs/NOTATION.md)). `IGame` keeps its move history.

### Changed
- **The engine's x axis is now the displayed file** (a = 0 on the left, king on e1, as in the game and in 5d-chess-js); the nine `.5dp` files are mirrored and the "mirrored files" difference is gone. **Turns are 1-based**: `T1w` is the start board in `.5dp` files, `present:` is a turn label too (`present: T3w`), and in notation. `.5dp` files written by 0.4.0 must be updated (mirror the rows, add 1 to every turn number); `size:` is capped at 8.
- `Piece` is a plain value (`type`, `color`, `unmoved`) and `Board` a fixed array of one-byte cells, so forking a board is a copy. Search and random games are faster: 2.2 vs 1.8-2.1 M nodes/s, 1000 random games in 42-46 s vs 55-56 s on the same machine ([docs/SEARCH.md](docs/SEARCH.md)).
- UI architecture: one `Screen` per page (`MainMenuScreen`, `ModeSelectScreen` (was Versus), `SettingsScreen`, `PlayScreen` (was TestingScene)) on a `ScreenStack` that applies navigation at frame end and cross-fades. A small widget layer (`include/ui/Widgets.h`: `Button`, `ButtonList`, `Toggle`, `ui::column` / `ui::row`, a per-frame "pointer consumed" flag) replaces the four menu controllers, the Composite menu model, the ten `ICommand` classes, the GameState/Scene pairs and the SceneManager (about 2,800 lines net removed). Screenshots are unchanged.
- **Space no longer toggles the navigation buttons; ESC does** (Space only duplicated ESC).

### Removed
- The legacy `IGame` subclasses (`StandardGame`, `CustomGame*`, `MiscGame*`), `NameOfGame`, `Constant`, `createGame<T>`, the `Piece` class hierarchy: `GameCatalog` and the `.5dp` files are the only source of game modes.

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

[Unreleased]: https://github.com/LLaammTTeerr/5DChess/compare/v0.4.0...HEAD
[0.4.0]: https://github.com/LLaammTTeerr/5DChess/compare/v0.3.0...v0.4.0
[0.3.0]: https://github.com/LLaammTTeerr/5DChess/compare/v0.2.0...v0.3.0
[0.2.0]: https://github.com/LLaammTTeerr/5DChess/compare/v0.1.0...v0.2.0
[0.1.0]: https://github.com/LLaammTTeerr/5DChess/releases/tag/v0.1.0
