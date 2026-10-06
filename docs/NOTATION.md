# Move notation and game records

`include/engine/Notation.h` writes and reads single moves (`toNotation`, `parseMove`) and whole games (`writeRecord`,
`loadRecord`). A record is plain text: the starting position and, for every submitted turn, the moves of that turn.
Replaying a record goes through the engine, so a record can only describe a legal game.

## Moves

```
(L0T3)e2>(L1T2)e4          a move from timeline 0, turn 3, square e2 to timeline 1, turn 2, square e4
(L-1T2)a1>(L-1T2)a2        negative timelines are written with the sign; a single leading + (`L+1`) is accepted when reading, never written
(L0T9)e7>(L0T9)e8=Q        a promotion carries the chosen piece
```

Grammar (no spaces inside a move):

```
move      = square ">" square [ "=" piece ]
square    = "(L" timeline "T" turn ")" file rank
timeline  = [ "+" | "-" ] digits      -1000 .. 1000 ("+" is only read, never written)
turn      = digits                    1 .. 10000
file      = "a" .. "h"                x = 0 .. 7
rank      = "1" .. "8"                y = 0 .. 7
piece     = "Q" | "R" | "B" | "N"
```

* **Timeline** `L<id>` is the engine's timeline ID, as in `.5dp` files ([POSITIONS.md](POSITIONS.md)); `L-1` is the first
  timeline created by Black, `L1` the first created by White.
* **Turn** `T<n>` is the *full* turn number of the board, **counted from 1** like `T1w` in position files: the board of
  half-turn `h` is `T(h / 2 + 1)`, so the starting board is `T1`. A move always starts from and lands on a board that ends on
  the mover's turn, so all its boards are White's (even half-turns) or all Black's (odd). The text therefore does not say
  which: `parseMove(text, mover)` takes the side, and inside a record it is the side to move. `(L0T4)` is half-turn 6 for
  White and 7 for Black.
* **File** is `'a' + x`, **rank** is `y + 1`: `a` is the left file seen from White's side and `y` = 0 is White's back rank, so
  the squares read exactly as on the board the player sees and as in the board text of a `.5dp` file; the king of the
  standard setup stands on e1, the queen on d1. This is also the convention of 5d-chess-js, whose boards are indexed the same
  way, so `tools/refcheck` maps files one to one. Square names do not depend on the board size.
* **Promotion** `=Q`, `=R`, `=B`, `=N` is the piece the pawn becomes. `toNotation(move, promotes)` writes it when
  `promotes` is true or `move.promotion` is not the default Queen; the game knows which moves promote (`IGame::history()`
  records `PlayedMove::promotes`), `Core::Move` alone does not. In a record the suffix is **required exactly when a pawn
  reaches the last rank** (so a Queen promotion is written `=Q`, a plain move never has a suffix); `loadRecord` rejects both
  omissions and extras, which makes the text of a game canonical.
* Castling is the king's two-file step (`(L0T1)e1>(L0T1)g1`), en passant the pawn's diagonal
  step onto the empty square: as in the engine, the geometry says it, the notation has no extra mark. Captures are not marked.
* Numbers are range-checked while reading (at most six characters, within the ranges above); anything else, including
  `+-1`, `++1`, spaces, lower-case pieces and trailing text, is a `Core::ParseError`.

## Records

```
5dchess-record 1
mode: standard
T1w: (L0T1)e2>(L0T1)e4
T1b: (L0T1)e7>(L0T1)e5
T2w: (L0T2)b1>(L0T2)c3
```

A turn with several moves lists them on one line, e.g. `T5w: (L0T5)... (L1T5)...` (one move on each timeline that had to be moved on).

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
  started (half-turn `h` is `T(h / 2 + 1)` followed by `w` for even, `b` for odd `h`) and must equal the replayed game's present,
  which also says whose turn it is; it is a check, because a move into the past can pull the present back (a later
  line can then start at an earlier turn than the line before it). The moves of a turn are separated by single spaces, in the order they were played.
  A turn has at least one move.
* The moves of an unfinished turn are not written: `writeRecord(game, &droppedPending)` reports whether there were any, so that a
  save function can warn the player. The game result is not written either: it follows from the moves.

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
4 MiB of text, 5000 turns, 256 moves per turn, 50 000 moves, timelines within -1000..1000 and turns 1..10000 in the
moves, and whatever the position parser allows in an embedded position. Each submitted turn arms a result search that copies
the boards, which is why the turn count is capped well below the position format's board limit.

## Saved games in the app

The game stores records through `savegame::SaveStore` (`include/services/SaveStore.h`): the **autosave** (written after every
submitted turn, deleted when the game ends) and three **slots**, as `autosave.5dr` / `slot1.5dr` ... `slot3.5dr` next to
`settings.txt` (the browser's localStorage keys `5dchess.autosave`, `5dchess.slot1` ... on the web). A slot file is an ordinary
record with one extra comment line after the magic, `# saved: 2026-10-06 14:32`, which the slot list shows. Only submitted
turns are in a save: **the unsubmitted moves of the current turn are not saved, by design** (`writeRecord`'s `droppedPending`
tells the Save panel to warn). Reading is bounded: a file over 1 MiB is not read, and any `ParseError` is shown as "This save
can't be loaded" while the file is kept. "Copy" / "Paste record" move the same text through the clipboard (desktop only).

**Games against the computer** carry one more comment line after the magic (before or after `# saved:`):

```
5dchess-record 1
# vs-computer: you=black level=normal seed=1234567890123
# saved: 2026-10-06 14:32
mode: standard
T1w: (L0T1)e2>(L0T1)e4
```

`you` is the player's side (`white` or `black`: a "Random" choice is resolved once, when the game starts), `level` is `easy`, `normal` or
`hard`, and `seed` is the game's seed (decimal, 64 bits); the AI's decision of a turn depends only on the position, the level and
`searchSeed(seed, number of submitted turns)`, so the record replays the same game. Exactly those three fields, each once; a line that does
not parse is just a comment, so the game loads as an ordinary two-player game, which is also what every record without the line is and what
older versions make of one. `play::formatMeta` / `parseMeta` / `findMeta` (`include/play/VsAi.h`) write and read it; `loadRecord` itself
ignores it. The slot list says "Standard vs Computer - 7 turns".

## API

```cpp
std::string toNotation(const Core::Move&, bool promotes = false);
Core::Move  parseMove(std::string_view text, PieceColor mover);        // throws Core::ParseError
std::string writeRecord(const IGame&, bool* droppedPending = nullptr);                               // std::logic_error if the game has no start position
std::shared_ptr<IGame> loadRecord(std::string_view text);               // throws Core::ParseError
```

`IGame` keeps what a record needs: `history()` (the submitted turns, each a `Core::PlayedTurn` with the present it started
at and its `Core::PlayedMove`s), `pendingMoves()`, `startPosition()` and `modeId()`. Games built directly in code (the
engine's test sandboxes) have no start position and cannot be written as a record.

## Differences from other notations

Squares, turns (from 1), file letters and the sign of timelines follow the game and 5d-chess-js. The text itself is not
Axioms' notation ([RULES.md](RULES.md) S4): a move names only its two squares (no piece letter, no capture or check marks,
no `>>` for jumps), which is enough because the engine decides what the move does from the board.
