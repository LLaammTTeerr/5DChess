# Move notation and game records

`include/engine/Notation.h` writes and reads single moves (`toNotation`, `parseMove`) and whole games (`writeRecord`,
`loadRecord`). A record is plain text: the starting position and, for every submitted turn, the moves of that turn.
Replaying a record goes through the engine, so a record can only describe a legal game.

## Moves

```
(L0T3)e2>(L1T2)e4          a move from timeline 0, turn 3, square e2 to timeline 1, turn 2, square e4
(L-1T2)a1>(L-1T2)a2        negative timelines are written with the sign
(L0T1)e7>(L0T1)e8=Q        a promotion carries the chosen piece
```

Grammar (no spaces inside a move):

```
move      = square ">" square [ "=" piece ]
square    = "(L" timeline "T" turn ")" file rank
timeline  = [ "-" ] digits            -1000 .. 1000
turn      = digits                    0 .. 10000
file      = "a" .. "h"                x = 0 .. 7
rank      = "1" .. "8"                y = 0 .. 7
piece     = "Q" | "R" | "B" | "N"
```

* **Timeline** `L<id>` is the engine's timeline ID, as in `.5dp` files ([POSITIONS.md](POSITIONS.md)); `L-1` is the first
  timeline created by Black, `L1` the first created by White.
* **Turn** `T<n>` is the *full* turn number of the board, counted from 0 like `T0w` in position files: the board of
  half-turn `h` is `T(h / 2)`. A move always starts from and lands on a board that ends on the mover's turn, so all its
  boards are White's (even half-turns) or all Black's (odd). The text therefore does not say which: `parseMove(text, mover)`
  takes the side, and inside a record it is the side to move. `(L0T3)` is half-turn 6 for White and 7 for Black.
* **File** is `'a' + x`, **rank** is `y + 1`, with the engine's own coordinates (`x` = file from the left, `a` = 0;
  `y` = 0 is White's back rank). The letters are therefore **not mirrored**: they show the squares exactly as the board text
  in a `.5dp` file lists them, and in the engine's standard setup the **king stands on d1 and the queen on e1** (x = 3 and
  4), the other way round from the notation of the game and of 5d-chess-js (queen d, king e). `e2>e4` in the standard
  mode is the pawn in front of the *queen*. To get the letters of the game's notation, mirror the file within the board:
  `file' = 'a' + (N - 1 - x)` for a board of N files (standard N = 8: `a <-> h`, `d <-> e`). This is the same mirroring
  `tools/refcheck` applies when it compares the engine with 5d-chess-js ([RULES.md](RULES.md), difference 2). The
  mirroring is not built in, so that a move's text does not depend on the board size.
* **Promotion** `=Q`, `=R`, `=B`, `=N` is the piece the pawn becomes. `toNotation(move, promotes)` writes it when
  `promotes` is true or `move.promotion` is not the default Queen; the game knows which moves promote (`IGame::history()`
  records `PlayedMove::promotes`), `Core::Move` alone does not. In a record the suffix is **required exactly when a pawn
  reaches the last rank** (so a Queen promotion is written `=Q`, a plain move never has a suffix); `loadRecord` rejects both
  omissions and extras, which makes the text of a game canonical.
* Castling is the king's two-file step (`(L0T0)d1>(L0T0)b1` in the engine's mirrored setup), en passant the pawn's diagonal
  step onto the empty square: as in the engine, the geometry says it, the notation has no extra mark. Captures are not marked.
* Numbers are range-checked while reading (at most six characters, within the ranges above); anything else, including
  `+1`, spaces, lower-case pieces and trailing text, is a `Core::ParseError`.

## Records

```
5dchess-record 1
mode: standard
T0w: (L0T0)e2>(L0T0)e4
T0b: (L0T0)e7>(L0T0)e5
T1w: (L0T1)b1>(L0T1)c3
```

A turn with several moves lists them on one line, e.g. `T4w: (L0T4)... (L1T4)...` (one move on each timeline that had to be moved on).

* The first non-comment line is `5dchess-record 1` (magic and version). `#` starts a comment line, blank lines are ignored,
  `\r` at the end of a line is ignored.
* **Header**: exactly one of
  * `mode: <id>`: a game mode of the catalog (`GameCatalog`, `assets/positions/<id>.5dp`);
  * a block `position:` ... `end-position` containing a complete `.5dp` text ([POSITIONS.md](POSITIONS.md)), for games
    that did not start from a catalog mode, for instance one that started from `Position::fromGame` of a game in progress.
  `writeRecord` uses `mode:` when the game was made by `GameCatalog::create`, the embedded position otherwise (the text of
  `IGame::startPosition()`, i.e. the normalised position the game was built from). A `mode:` record depends on the file
  of that mode staying the same.
* **One line per submitted turn**, in order: `T<n><w|b>: <move> <move> ...`. The label is the **present** when the turn
  started (half-turn `h` is `T(h / 2)` followed by `w` for even, `b` for odd `h`) and must equal the replayed game's present,
  which also says whose turn it is; it is a check, because a move into the past can pull the present back and then the same
  side moves again in the next line. The moves of a turn are separated by single spaces, in the order they were played.
  A turn has at least one move.
* The moves of an unfinished turn are not written. The game result is not written either: it follows from the moves.

### What `loadRecord` checks

Every move is replayed on a real game; the first problem throws `Core::ParseError` with `line N: ...` (the embedded
position block reports `position block starting at line N: line K: ...`). A record is rejected when

* the header, a label or a move is malformed, the mode is unknown or the embedded position is invalid;
* the label differs from the present of the replayed game;
* a move is not a legal move of the side to move (`IGame::legalMovesFrom`: right colour, board that can be moved on, target
  reachable, a pawn's promotion piece chosen), or its promotion suffix is missing or superfluous;
* after the moves of a line the turn cannot be submitted (a mandatory board was not moved on, or a king of the mover
  could be captured, `IGame::canSubmit`), or the line has no moves;
* another turn follows after the game was decided (the next move fails, since a position without a legal turn admits none).

At the end the result is resolved with a bounded search (2 000 000 nodes); a position it cannot decide in that time stays
`Ongoing`. The replayed game has the same history as the one the record was written from:
`writeRecord(loadRecord(text)) == text` for every record written by `writeRecord`, and the boards, bookkeeping and result
equal those of the original (tested on random games of all modes, `tests/notation_test.cpp`).

### Limits

Reading is bounded like the position parser, so a hostile file cannot make it run or allocate without limit: at most
4 MiB of text, 5000 turns, 256 moves per turn, 50 000 moves, timelines within -1000..1000 and turns up to 10000 in the
moves, and whatever the position parser allows in an embedded position. Each submitted turn arms a result search that copies
the boards, which is why the turn count is capped well below the position format's board limit.

## API

```cpp
std::string toNotation(const Core::Move&, bool promotes = false);
Core::Move  parseMove(std::string_view text, PieceColor mover);        // throws Core::ParseError
std::string writeRecord(const IGame&);                                  // std::logic_error if the game has no start position
std::shared_ptr<IGame> loadRecord(std::string_view text);               // throws Core::ParseError
```

`IGame` keeps what a record needs: `history()` (the submitted turns, each a `Core::PlayedTurn` with the present it started
at and its `Core::PlayedMove`s), `pendingMoves()`, `startPosition()` and `modeId()`. Games built directly in code (the
engine's test sandboxes) have no start position and cannot be written as a record.
