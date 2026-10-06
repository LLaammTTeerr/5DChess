# 5DChess

Chess with multiverse time travel: a 5D Chess game with timelines, built in C++20 with raylib, for desktop and the browser.

[![CI](https://github.com/LLaammTTeerr/5DChess/actions/workflows/ci.yml/badge.svg?branch=main)](https://github.com/LLaammTTeerr/5DChess/actions/workflows/ci.yml)
[![Latest release](https://img.shields.io/github/v/release/LLaammTTeerr/5DChess)](https://github.com/LLaammTTeerr/5DChess/releases/latest)
[![Pages deploy](https://github.com/LLaammTTeerr/5DChess/actions/workflows/pages.yml/badge.svg)](https://github.com/LLaammTTeerr/5DChess/actions/workflows/pages.yml)

**▶ Play in your browser: <https://chess.lamter.cc/>**

**⬇ Download for Linux/macOS/Windows: <https://github.com/LLaammTTeerr/5DChess/releases/latest>**

![A Standard game in the Deep space board view: boards as lit windows on glowing threads, the present as an aurora, timelines labelled L0, L+1, L-1](docs/screenshots/board-deep-space.png)

## Table of Contents
- [Overview](#overview)
- [Screenshots](#screenshots)
- [Prerequisites](#prerequisites)
- [Installation](#installation)
- [Building](#building)
- [Running](#running)
- [Project Structure](#project-structure)
- [Controls](#controls)
- [Features](#features)
- [Roadmap](#roadmap)
- [Known limitations](#known-limitations)
- [Troubleshooting](#troubleshooting)
- [Contributing](#contributing)

## Overview

This project implements a 5D Chess game with an advanced UI system featuring:
- Multi-timeline chess mechanics
- Three board views (Deep space, Atlas, Blueprint) that show every timeline at once: lanes, a turn ruler, the present, branches and time-travel jumps
- Interactive board visualization with highlighting and an auto-focusing camera
- Small widget layer (buttons, lists, toggles) and a screen stack with cross-fades
- In-turn undo, deselect and submit controls
- One `Screen` per page (main menu, mode select, settings, game), navigated with direct calls
- Modular rendering pipeline

## Screenshots

Three board views, selectable in Settings -> Display -> *Board view* (the same Standard game in each):

<table>
  <tr>
    <td width="33%"><img src="docs/screenshots/board-deep-space.png" alt="Deep space board view: indigo night sky, glowing boards, aurora present"><br><sub><b>Deep space</b> (default): glow encodes state; luminous branches in gold or violet by who branched.</sub></td>
    <td width="33%"><img src="docs/screenshots/board-atlas.png" alt="Atlas board view: warm paper map with tinted timeline lanes and card boards"><br><sub><b>Atlas</b>: a paper map with tinted lanes, card boards and a terracotta present.</sub></td>
    <td width="33%"><img src="docs/screenshots/board-blueprint.png" alt="Blueprint board view: monochrome ink on off-white with subway-style connectors"><br><sub><b>Blueprint</b>: ink on off-white, elbow connectors, colour only for what needs attention.</sub></td>
  </tr>
</table>

Official-rules visuals: a board's frame says whether you must move on it (mandatory), may (optional) or not (history); inactive timelines are dimmed and tagged; a check draws a pulsing line from every attacker to the king; promotion opens a piece picker.

<table>
  <tr>
    <td width="50%"><img src="docs/screenshots/rules-check.png" alt="A check: a red line from the attacking rook to the white king"><br><sub>Check: the attack line (it stays still under Reduce motion).</sub></td>
    <td width="50%"><img src="docs/screenshots/rules-promotion.png" alt="The promotion picker above the square e8"><br><sub>Promotion: choose Queen, Rook, Bishop or Knight (Q / R / B / N).</sub></td>
  </tr>
</table>

![The animated main menu](docs/screenshots/main-menu.png)

*The main menu.*

![A piece sliding to its new square in the Pixel theme](docs/screenshots/move.gif)

*Pieces slide, new boards grow in, and a banner announces the next turn.*

![The end card after a checkmate](docs/screenshots/checkmate.png)

*The game ends on checkmate; no legal turn without check is a stalemate (draw).*

<table>
  <tr>
    <td width="50%"><img src="docs/screenshots/timelines.png" alt="Timeline Battle mode with eight timelines in the Deep space view"><br><sub>Timeline Battle: eight timelines branching from the start.</sub></td>
    <td width="50%"><img src="docs/screenshots/settings.png" alt="Settings screen, Display tab with the Board view option"><br><sub>Settings -> Display: board view and Reduce motion (settings are saved).</sub></td>
  </tr>
</table>

## Prerequisites

- A C++20 compiler (GCC 11+, Clang 14+, or MSVC 2022)
- CMake 3.28 or newer
- Git is not required to build; raylib 5.5 is downloaded automatically by CMake
  (a system-installed raylib >= 5.0 is used instead if found)

### Linux (Debian/Ubuntu)
```bash
sudo apt-get install -y build-essential cmake libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev libgl1-mesa-dev libasound2-dev
```

### macOS
Install the Xcode Command Line Tools (`xcode-select --install`) and CMake (`brew install cmake`).
`brew install raylib` is optional: CMake fetches raylib itself when it is not installed.

### Windows
Install Visual Studio 2022 (Desktop development with C++) and CMake. Use a
"Developer PowerShell" or any shell with `cmake` on the PATH.

## Installation

```bash
git clone https://github.com/LLaammTTeerr/5DChess.git
cd 5DChess
```

## Building

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release -j
```

On Windows (MSVC, multi-config generator) use the same two commands; the binary ends up in `build/Release/`.
Assets are copied next to the executable after every build.

### Web build

A WebAssembly build (emsdk 4.0.10, raylib's Web platform) runs in the browser. It needs CMake 3.28+, so with the
`emscripten/emsdk` image install a newer CMake first (`pip install cmake`):

```bash
emcmake cmake -S . -B build-web -DCMAKE_BUILD_TYPE=Release -DFDCHESS_BUILD_TESTS=OFF
cmake --build build-web -j
python3 -m http.server 8765 -d build-web   # then open http://localhost:8765/5dchess.html
```
Only the assets the game loads are bundled (including the audio); the web build has no Exit menu item. Releases are deployed to GitHub Pages by `.github/workflows/pages.yml`.

### CMake options

| Option | Default | Meaning |
| --- | --- | --- |
| `FDCHESS_BUILD_GAME` | ON | Build the raylib game (`5dchess`) |
| `FDCHESS_BUILD_TESTS` | ON | Build the unit/property tests (doctest) |
| `FDCHESS_SANITIZE` | OFF | Enable AddressSanitizer + UBSan (GCC/Clang) |
| `FDCHESS_BUILD_REFCHECK` | OFF | Build `tools/refcheck` (differential testing against 5d-chess-js, search benchmark) |

### Tests
```bash
cmake -S . -B build-test -DCMAKE_BUILD_TYPE=Debug -DFDCHESS_BUILD_GAME=OFF -DFDCHESS_SANITIZE=ON
cmake --build build-test -j
ctest --test-dir build-test --output-on-failure
```
The engine tests do not need raylib or a display. Drop `-DFDCHESS_SANITIZE=ON` for a plain run.

### Engine docs and differential testing
- [docs/RULES.md](docs/RULES.md): the rules the engine implements, their sources, and the deliberate differences from the
  reference engine 5d-chess-js.
- [docs/POSITIONS.md](docs/POSITIONS.md): the `.5dp` position file format used by the nine game modes (`assets/positions/`).
- [docs/NOTATION.md](docs/NOTATION.md): move notation (`(L0T1)e2>(L0T1)e4`) and the `5dchess-record` game record format.
- [docs/SEARCH.md](docs/SEARCH.md): how checkmate / stalemate are decided without blocking the game (the resumable
  `TurnSearch`), why its pruning is safe, and the benchmark (`turnbench`).
- `tools/refcheck/` (`-DFDCHESS_BUILD_REFCHECK=ON`, default OFF): plays random legal games, dumps every position with its full
  move list and replays them in 5d-chess-js (`node tools/refcheck/compare.js --ref <5d-chess-js checkout> --bin <refcheck>`).

### Packaging
```bash
cmake --build build --config Release
cmake --build build --config Release --target package   # produces 5DChess-<version>-<platform>.zip
```

### Releasing
1. Bump `VERSION` in the `project()` call of `CMakeLists.txt`.
2. Update `CHANGELOG.md` (move `[Unreleased]` entries under the new version).
3. Tag and push: `git tag vX.Y.Z && git push origin vX.Y.Z`.

The release workflow checks that the tag matches `VERSION`, builds on Linux/macOS/Windows, and publishes the ZIPs to a GitHub Release.

## Running

```bash
./build/5dchess            # Linux/macOS
.\build\Release\5dchess.exe   # Windows
```
The game changes its working directory to the executable's folder, so it can be launched from anywhere.

## Project Structure

```
5DChess/
├── CMakeLists.txt             # Build, install and CPack configuration
├── CHANGELOG.md               # Release notes
├── .github/                   # CI, release workflows and Dependabot config
├── assets/                    # Game assets
│   ├── images/                # Piece and board textures
│   ├── fonts/                 # Custom fonts
│   ├── backgroundmusic/       # Audio files
│   └── soundeffect/           # Sound effects
├── include/                   # Header files
│   ├── engine/                # Value API, .5dp positions, GameCatalog (no raylib)
│   ├── services/              # Assets, settings and other app services
│   ├── play/                  # The game screen's parts: Selection, BoardLayout, MultiverseView, BoardStyle, BoardScene, BoardRenderer, MoveAnimator, BoardCamera, Hud, TimelineArrows, PromotionPicker
│   ├── Render/                # Motion tokens, UI theme, piece themes
│   ├── Screens/               # One Screen per page of the game
│   └── ui/                    # Widget layer and the Screen / ScreenStack model
├── src/                       # Source files
│   ├── play/
│   ├── Render/
│   ├── engine/                # Rules engine implementation
│   ├── services/
│   ├── App.cpp                # App context (assets, audio, settings, screens)
│   ├── Screens/               # MainMenuScreen, ModeSelectScreen, SettingsScreen, PlayScreen
│   ├── ui/                    # Widgets (Button, ButtonList, Toggle, layout) and ScreenStack
│   ├── main.cpp               # Entry point
│   └── chess.cpp              # Core rules engine (no raylib dependency)
├── tests/                     # doctest unit and property tests for the engine
│   └── ui/                    # UI screenshot tests (scripts, baselines)
├── tools/                     # theme_preview, ui_script, refcheck (differential tests against 5d-chess-js)
├── docs/                      # RULES.md, SEARCH.md, POSITIONS.md, NOTATION.md, screenshots
└── README.md                  # This file
```

## Controls

### Menu Navigation
- **Mouse**: Click to select menu items
- **Hover**: Visual feedback on interactive elements

### Game Controls
- **Mouse Click**: Select a piece, then click a highlighted square to move (hovering a square only tints it; legal targets appear after selecting a piece); a pawn reaching the last rank opens the promotion picker (or press Q / R / B / N)
- **Drag / Mouse Wheel**: Pan and zoom the camera
- **In-game buttons**: Undo, Deselect and Submit (greyed out when unavailable)

### Keyboard Shortcuts
- **ESC**: Show or hide the navigation buttons (shown by default; Back returns to game selection)
- **Z**: Toggle camera auto-zoom
- **X**: Fit the boards in view (manual auto-zoom)

## Features

### Core Gameplay
- **Official 5D Chess rules**: moves across time and parallel universes, active/inactive timelines and the present, mandatory moves on the present boards, check through time, castling, en passant and promotion (see [docs/RULES.md](docs/RULES.md); checked against the reference engine 5d-chess-js)
- **Winning**: you win by checkmate (the opponent has no legal turn and is in check); no legal turn without check is a stalemate and a draw. Kings are never captured. The position is checked in the background after every turn ("Checking position..."), so the game never freezes
- **Board views**: Deep space (default), Atlas and Blueprint, chosen in Settings -> Display. Timelines are lanes labelled L0, L+1, L-1 (White's above L0, Black's below); a ruler on top counts turns (T1 T2 ... with w/b ticks); the present is marked; branches are drawn as connectors coloured by the player who created the timeline; a time-travel move shows a dashed arc with the moving piece. One renderer draws all three from `BoardStyle` data (`include/play/BoardStyle.h`)
- **Rules at a glance**: boards you must move on, may move on, or cannot (history) are framed differently; boards of inactive timelines are dimmed and desaturated and tagged "inactive"; check draws a line from every attacker to the king; promotion asks which piece (no auto-queening)
- **Legal Move Highlighting**: Visual guides for valid moves; Submit is enabled only when the whole turn is legal (otherwise the HUD says why, e.g. "Your king would be capturable")
- **Notation and records (engine only)**: moves have a notation (`(L0T1)e2>(L0T1)e4`) and games can be written to and replayed from a `5dchess-record` ([docs/NOTATION.md](docs/NOTATION.md)); there is no save/load button in the app yet
- **Undo**: Take back moves within the current turn before submitting (no redo)
- **Board orientation**: Boards are drawn from White's side

### User Interface
- **Runs everywhere**: native on Linux, macOS and Windows, and in the browser (WebAssembly build)
- **Animated main menu**: a code-drawn hero with a drifting field of timeline boards and idling Pixel creatures
- **Motion**: shared tokens and easing (`include/Render/Motion.h`); pieces slide or arc across boards, new boards grow in, selected pieces lift, menus stagger in, screens cross-fade, and a turn banner announces the side to move. Animations never delay game state or input. Settings -> Display -> **Motion: Reduced** makes changes instant or short cross-fades
- **Pixel theme with blinking creatures**: Pixel-theme pieces blink at random intervals when zoomed in
- **HUD and controls bar**: side to move, turn, next-step hint, and an end-of-game card; styled to match the board view (the menus stay cream)
- **Settings are saved**: piece theme, board view, music, sound effects and Reduce motion persist between runs (`~/.config/5dchess/settings.txt` on Linux, `~/Library/Application Support/5DChess` on macOS, `%APPDATA%\5DChess` on Windows; the browser's localStorage on the web)
- **Camera**: smooth auto-centering and auto-zoom (spring-damped), plus manual pan and zoom
- **Menus**: button and list layouts, rounded buttons with smooth hover, warm cream + terracotta theme (`include/Render/UITheme.h`)

### Audio
- **Background music**: three CC0 piano tracks, off by default; pick one in Settings → Music. The choice is global and persists across screens; the track is streamed and starts after your first click (browser autoplay policy).
- **Sound effects**: move, capture, game won and menu-button clicks, with an on/off toggle ("Sound effects" in Settings → Music). Check, castle, promotion and draw sounds are bundled but wait for those rules to exist.
- Without an audio device the game runs silently.

### Piece Themes
Four piece themes are available: Classic, Modern, Fantasy and Pixel. Choose one in Settings → Piece Theme. Pixel is an original pixel-art theme generated by `python3 scripts/gen_pixel_theme.py` (requires Pillow); `--out DIR` sets the output directory (default `assets/images/Theme_3`) and `--sheet PATH` writes a preview contact sheet.

![Contact sheet of the twelve Pixel theme pieces](docs/screenshots/pixel-pieces.png)

### Developer Tools
Configure with `-DFDCHESS_BUILD_TOOLS=ON` to also build `theme_preview`, which renders a scripted game and saves a screenshot:
```bash
theme_preview <Classic|Modern|Fantasy|Pixel> <Standard|Battle|Invasion|Fragment> <out.png> [turns] [--view "Deep space"|Atlas|Blueprint]
```
`turns` defaults to 2; `--perf` measures frame times. `ui_script` runs the real game from a script with injected input (`clicksq 0 2 g1` clicks a square by name; `mode <id>` / `position <file.5dp>` open a game): see [tests/ui/README.md](tests/ui/README.md).

### Technical Features
- **Engine without raylib**: a plain value API (`Piece`, `Board`), `.5dp` position files for the nine game modes, and a resumable background result search
- **Screens and widgets**: one `Screen` per page on a `ScreenStack`, a small widget layer, and a game screen split into `play::` modules
- **Manifest-driven assets** with loading and caching
- **Game modes as data**: add a mode by adding a `.5dp` file

## Roadmap

- ~~Official 5D Chess rules~~ (check, checkmate, stalemate, active timelines, castling, en passant, promotion choice): done.
- ~~v0.4.0~~: official rules, non-blocking result search, differential testing, `.5dp` position files, engine value API, UI screenshot tests, manifest-driven assets: done.
- ~~v0.5.0~~: widget/screen UI rewrite, three board views (Deep space by default, Atlas, Blueprint), official-rules visuals, promotion picker, saved settings, move notation and game records in the engine: done.
- **v0.6.0** (next): AI opponent, save/load in the UI, puzzles and an interactive guide.

## Known limitations

- In rare, huge positions the checkmate/stalemate search may not finish in reasonable time; the result then stays undecided ("Checking position...") and the game simply continues ([docs/SEARCH.md](docs/SEARCH.md)).
- The rules engine is cross-checked against 5d-chess-js on Standard and the Simplify modes only; the Misc modes (Time Line Invasion, Battle, Fragment) are not cross-checked.
- The Puzzles and Guide menu items are placeholders; there is no AI opponent yet.
- Games cannot be saved or loaded from the app (the engine can read and write records, see [docs/NOTATION.md](docs/NOTATION.md)).

## Troubleshooting

### Build issues

- **`raylib.h`/X11/GL headers missing on Linux**: install the apt packages listed under Prerequisites.
- **CMake too old** (`cmake_minimum_required` error): install CMake 3.28+ (`pip install cmake` or Kitware's apt repository).
- **raylib download fails (offline/proxy)**: install raylib >= 5.0 system-wide (`brew install raylib`, or your distro package) and re-run CMake with a clean build directory.
- **Stale configuration**: delete the build directory and configure again.

### Runtime issues

- **Missing assets**: the `assets/` folder must sit next to the executable. CMake copies it after the build; packaged ZIPs include it.
- **Window doesn't open**: check graphics drivers (OpenGL 3.3 is required). Headless machines need a display, e.g. `xvfb-run -a ./build/5dchess`.
- **Performance issues**: lower the FPS limit in `main.cpp` or reduce texture sizes in `assets/images/`.

## Contributing

1. Fork the repository
2. Create a feature branch (`git checkout -b feature/amazing-feature`)
3. Commit your changes (`git commit -m 'Add amazing feature'`)
4. Push to the branch (`git push origin feature/amazing-feature`)
5. Open a Pull Request

### Code Style
- Use C++20 standards
- Follow the existing naming conventions
- Document new classes and methods
- Maintain the established design patterns

### Testing
- Run `ctest` (see Tests above) before submitting; CI runs it on Linux, macOS and Windows
- Verify all menu interactions work correctly
- Ensure no memory leaks with new features

## License

The source code is released under the [MIT License](LICENSE). It started as an academic assignment for the Object-Oriented Programming course at HCMUS.

Assets keep their own licences, listed in [assets/CREDITS.md](assets/CREDITS.md): the music and sound effects are CC0, the fonts are under the SIL Open Font License, and the Pixel piece theme is original to this project. The provenance of piece themes 0–2 and some board images is unknown, so they are not covered by the MIT licence.

## Credits

The sound effects and music are CC0 / public domain; fonts are under the SIL Open Font License. See [assets/CREDITS.md](assets/CREDITS.md) for the full list of sources and licences.

## Acknowledgments

- **Raylib**: Graphics and audio library
- **HCMUS**: University of Science, VNU-HCM
- **Course**: Object-Oriented Programming (OOP)

---

For questions or issues, please open an issue on the GitHub repository or contact the development team.
