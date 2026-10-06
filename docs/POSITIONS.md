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
| `size: N` | 1..8 (`Board::MAX_DIM`; the search and the one-byte board cells need N <= 8) |
| `rules: double-step castling` | enabled rules, any of `double-step` (pawns may advance two squares on their first move) and `castling`, or `none` |
| `to-move: white\|black` | side to move |
| `present: H` | half-turn of the present; optional. It must equal what the engine computes: the lowest half-turn among the latest boards of the **active** timelines (below). Its parity must agree with `to-move` |
| `L<id> T<turn><w\|b>: rows` | the board of timeline `id` at half-turn `2*turn + (b ? 1 : 0)`. `L-1 T2w` is timeline -1, turn 2, White to move |
| `L<id> parent: L<id>` | marks a timeline branched off by a player (original timelines have no parent) |

`#` starts a comment line and blank lines are ignored. All header lines (`title`, `size`, `rules`, `to-move`, `present`;
`size` and `to-move` are required, `size` only once) come before the first board line; a header after a board is an
error. The boards of one timeline must have consecutive half-turns, oldest first; one line gives a fresh start, more
lines give its history (needed for en passant, below). The first board of a timeline may start after `T0w`: *Misc - Time Line Fragment* starts its
timeline 0 at `T0b` (Black to move there, White to move on timeline 1, the present is half-turn 0):

```
L0 T0b: kppp/4/4/NBRN
L1 T0w: nbrn/4/4/KPPP
```


## Timeline structure (validated)

A timeline's id is its sign convention: White's new timelines are above the original ids, Black's below
(see [RULES.md](RULES.md)); a timeline with a `parent` line counts as created by a player. The parser checks that

- at least one timeline has no parent (the originals), and the originals have consecutive ids;
- every timeline with a parent lies outside the original id range, all ids together are consecutive (the engine
  allocates `max+1` / `min-1`), every parent exists and the parent chains end at an original (no cycles);
- the **active** timelines are the originals plus the n-th timeline created by a player iff the opponent has created
  at least n-1 (`IGame::isTimeLineActive`); the present is the lowest latest half-turn among them, so a lagging inactive
  timeline does not hold the present back.

## Limits

`size` 1..8, timeline ids within -1000..1000, half-turns up to 20000 (`T10000w`), at most 20000 boards in all. The turn
search sizes its arena by the id span and the board count (and `Core::Coord` stores the timeline and half-turn in
int16), so a hostile file such as `L100000000` could otherwise allocate gigabytes; real games stay far below these caps.
Numbers are range-checked while they are read, so overflowing input is a parse error, never undefined behaviour.

## En passant

A position has no `ep:` field. The engine derives en passant from the *previous board of the same timeline* (the enemy
pawn must have stood unmoved two ranks behind on that board). `writePosition` always emits every board of every
timeline, so round trips of a played game keep en passant. A hand-written or truncated position loses it unless you keep
the board before the last one: give `L0 T2b` and `L0 T3w` for the capture to be available at `T3w`; with `L0 T3w` alone
it is not. The first board of a timeline created by a branch has no previous board either, so no en passant there.

## What a position does not hold

The moves of a pending (unsubmitted) turn and the game result. Snapshot between turns. `Position` is plain data with
`operator==`, and `parsePosition(writePosition(p)) == p` holds for every position of a played game (tested on fuzz
games in `tests/position_test.cpp`).

## Adding a game mode

Put `assets/positions/<id>.5dp` in place and add `<id>` to the menu-order list in `src/engine/GameCatalog.cpp`. The
file is picked up by the web build automatically (CMake globs `assets/positions/*.5dp`).
