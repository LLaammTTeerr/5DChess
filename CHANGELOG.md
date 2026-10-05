# Changelog

All notable changes to this project are documented in this file.
The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added
- WebAssembly (Emscripten) build with a custom HTML shell, and a GitHub Pages deploy workflow that publishes it on each release. The web build has no background music and no Exit menu item.
- Audio: `AudioManager` singleton with streamed background music (Settings -> Music, default Off, global across scenes), sound effects for moves, captures, game won and menu clicks, and a Sound effects on/off toggle. No-ops without an audio device; music starts after the first click (browser autoplay policy).

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

[Unreleased]: https://github.com/LLaammTTeerr/5DChess/compare/v0.2.0...HEAD
[0.2.0]: https://github.com/LLaammTTeerr/5DChess/compare/v0.1.0...v0.2.0
[0.1.0]: https://github.com/LLaammTTeerr/5DChess/releases/tag/v0.1.0
