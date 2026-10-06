# Position files (`.5dp`)

A position file describes a game between turns: board size, rules, whose turn it is, and every board of every timeline.
The game modes in the menu are position files under `assets/positions/`, listed by `Chess::GameCatalog`
(`include/engine/GameCatalog.h`); the format is read and written by `include/engine/Position.h`
(`parsePosition`, `writePosition`, `Position::fromGame`, `Position::makeGame`).

```
5dchess-position 1
title: Standard
size: 8
rules: double-step castling
to-move: white
L0 T0w: rnbkqbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBKQBNR
```

## Orientation

The engine addresses a square as `(x, y)` on a board of `size` x `size`: **x is the file** (0 = left, the "a" file) and
**y the rank, 0 = White's back rank**; White's pawns advance towards y = size-1. A board line lists the rows **from the
top (y = size-1) down to y = 0**, separated by `/`, each row **from x = 0 to x = size-1**. The text is therefore the
picture a player sees with White at the bottom. In the standard row `rnbkqbnr` the king is on x = 3 and the queen on
x = 4 (this engine's standard setup has the king on the d-file).

## Rows

FEN-like. Upper case = White, lower case = Black: `K` king, `Q` queen, `R` rook, `B` bishop, `N` knight, `P` pawn. A
number is that many empty squares. A piece is *unmoved* (may still castle or double-step) unless followed by `*`, which
marks a piece that has already moved: `P*` is a pawn that has moved. Setup positions never need `*`.

## Lines

| Line | Meaning |
| --- | --- |
| `5dchess-position 1` | magic/version, must be the first non-comment line |
| `title: ...` | display name (optional) |
| `size: N` | 1..16 |
| `rules: double-step castling` | enabled rules, any of `double-step` (pawns may advance two squares on their first move) and `castling`, or `none` |
| `to-move: white\|black` | side to move |
| `present: H` | half-turn of the present; optional, default = the lowest half-turn among the timelines' latest boards. Its parity must agree with `to-move` |
| `L<id> T<turn><w\|b>: rows` | the board of timeline `id` at half-turn `2*turn + (b ? 1 : 0)`. `L-1 T2w` is timeline -1, turn 2, White to move |
| `L<id> parent: L<id>` | marks a timeline branched off by a player (original timelines have no parent) |

`#` starts a comment line and blank lines are ignored. Header lines (`size`, `to-move` at least) come before the first
board line. The boards of one timeline must have consecutive half-turns, oldest first; one line gives a fresh start, more
lines give its history. The first board of a timeline may start after `T0w`: *Misc - Time Line Fragment* starts its
timeline 0 at `T0b` (Black to move there, White to move on timeline 1, the present is half-turn 0):

```
L0 T0b: kppp/4/4/NBRN
L1 T0w: nbrn/4/4/KPPP
```

A timeline's id is its sign convention: White's new timelines are above the original ids, Black's below
(see [RULES.md](RULES.md)); a timeline with a `parent` line counts as created by a player.

## What a position does not hold

The moves of a pending (unsubmitted) turn and the game result. Snapshot between turns. `Position` is plain data with
`operator==`, and `parsePosition(writePosition(p)) == p` holds for every position of a played game (tested on fuzz
games in `tests/position_test.cpp`).

## Adding a game mode

Put `assets/positions/<id>.5dp` in place and add `<id>` to the menu-order list in `src/engine/GameCatalog.cpp`. The
file is picked up by the web build automatically (CMake globs `assets/positions/*.5dp`).
