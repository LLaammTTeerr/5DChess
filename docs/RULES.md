# 5D Chess With Multiverse Time Travel: rules implemented by the engine

This document says which rules `include/chess.h` / `src/chess.cpp` implement, where each one comes from, and where
sources were missing or disagreed. Wording: a board's *end-turn* is its half-turn number (h0 = White to move,
h1 = Black to move, ...). A timeline's *tip* is its latest board. "Mover" is the side whose turn it is.

## Sources

- S1 Wikipedia, "5D Chess with Multiverse Time Travel" (gameplay summary, check definition):
  https://en.wikipedia.org/wiki/5D_Chess_with_Multiverse_Time_Travel
- S2 Steam community discussions (players quoting the game's rules: active timelines, mandatory moves, playing on
  inactive timelines):
  https://steamcommunity.com/app/1349230/discussions/0/3737376621733988784/ (active timelines),
  https://steamcommunity.com/app/1349230/discussions/0/3124912256626913518/ (inactive timelines),
  https://steamcommunity.com/app/1349230/discussions/0/3737376621724951009/ (required moves)
- S3 `5d-chess-js`, the open-source engine by the community (Alex Bay / the `5d-chess` GitLab group), read from source:
  https://gitlab.com/5d-chess/5d-chess-js (`src/board.js`: `active`, `present`, `moves`; `src/piece.js`: pawn,
  en passant, castling, promotion; `src/mate.js`: `checks`, `checkmate`, `stalemate`; `src/index.js`: `submittable`)
- S4 Axioms' 5D chess notation (timeline numbering, promotion, checkmate/softmate):
  https://github.com/adri326/5dchess-notation
- The game's own wiki (5dchesswithmultiversetimetravel.fandom.com) could not be fetched (HTTP 402 to automated
  clients), so none of the rules below was checked against it directly. Where S1/S2 are silent, S3 decides ("the most
  authoritative open-source engine"), because it has been tested against real games.

## Rules

### Timelines
- A move whose target board is not the tip of its timeline (a jump into the past, or onto an older board of
  another timeline) creates a new timeline forked from that board. Moving onto a tip extends it. The source board's
  timeline is always extended. (Existing behaviour, confirmed by S1.)
- **Ownership (S4, S3):** a branch created by White gets the next unused ID above the highest one (`max + 1`, positive),
  one created by Black the next unused ID below the lowest (`min - 1`, negative). Implemented in
  `allocateTimeLineId`. The timelines present at game start (IDs 0, or 0..k for the misc modes) are "original" and belong
  to nobody.
- **Active timelines (S1, S2):** an original timeline is active. The n-th timeline created by a player is active iff the
  opponent has created at least n - 1 timelines. (`isTimeLineActive`.) S3 encodes the same rule through odd/even
  timeline indices.
- **Present (S1, S2, S3):** the earliest end-turn among the active timelines (`bufferHalfTurn()`, recomputed live after every
  move, so a new Black timeline can reactivate a White one and pull the present back). Inactive timelines never move the
  present.
- **Mandatory boards (S1, S2, S3):** while the present belongs to the mover, the tips of the active timelines lying on the
  present must each be moved on (`mandatoryBoards()`). The turn ends when the present has passed to the opponent. Every
  other tip of the mover's colour (inactive ones, and ones ahead of the present) may be played on but need not be
  (`getMoveableBoards()`). S3's default move list only offers present boards, but its mate/stalemate search and the
  game itself (S2) allow all of them; we follow the latter.

### Movement
- **Vector sets (S3 `piece.js` `movePos` / `moveVecs`, copied exactly, see `pieceVectors()`):** a vector is
  (dx, dy, dz, dw) = (file, rank, full turns, timeline ID); a step of dz changes the half-turn by 2 dz so that a move always
  lands on a board of the mover's colour. Both signs of every axis are allowed, **including forward in time** (onto a
  board that exists on a timeline that is ahead).
  - Rook: the 8 single-axis vectors, sliding. Bishop: the 24 vectors with exactly two non-zero components (the six planes
    x-y, x-t, y-t, x-l, y-l, t-l, four diagonals each), sliding. Queen: all 80 non-zero vectors of {-1,0,1}^4, sliding.
    King: the same 80 vectors, one step. Knight: 48 jumps, 2 along one axis and 1 along another.
  - A slide continues while the next board exists (a missing board, e.g. before a fork, ends it), stops on an own piece
    (not capturable) and ends after capturing an enemy piece. Move targets must be boards ending on the mover's turn.
  - Before this was aligned with S3 the king had no +t steps, the rook no +t slide, the bishop only -t, and the queen's
    +t slides were capped by the source board's own turn number.
- **Pawns (S3):** one step forward on the rank axis (White up, Black down) or one step "forward" on the timeline axis
  onto the same square of the same-time board, both only onto an empty square. **Forward on the timeline axis is towards
  the opponent's timelines: White towards lower IDs, Black towards higher** (S3 `timelineMove(l, -forward)`; an earlier
  version of the engine had it the other way round). An unmoved pawn may also make the double step on the rank axis (if
  empty between) and, as in S3, on the timeline axis (needs both boards to exist and be empty). Captures: diagonally
  forward on the same board (rank + file), or one timeline forward and one full turn into the past or future (same
  square). Pawns do not capture in other planes (S3's "brawn" variant does; it is not implemented).
- **En passant (S3), 2D only:** a pawn may capture an enemy pawn beside it diagonally onto the (empty) square behind it
  when that pawn made its double step in the last half-turn of the same timeline. Across timelines it does not exist.
  *Which board shows the double step.* The enemy pawn's double step is the move that produced the current board `h` from the
  board `h - 1` of the same timeline, so the engine requires: on board `h - 1` the pawn stood unmoved on its start square (and
  the squares it passed and now occupies were empty), on board `h` it stands beside the capturing pawn and its start square
  is empty. S3 (`piece.js` `enPassant`, lines 925-965) reads the board **`t - 2`** instead (the board before the capturing
  side's own last move on that timeline) and does not look at `t - 1`. Both agree whenever a timeline has been played on for at
  least two half-turns, which is every position but one: on the *second* board of a forked timeline (the opponent's double step
  made the second board from the first) `t - 2` is the board the fork was made from, which is not part of the new timeline (its
  earlier entries are `null`), so S3 refuses en passant there while `h - 1`, the first board of the new timeline, exists. The engine
  keeps `h - 1` because that is what the rule says (the double step must have been the opponent's *last* move), and because
  `t - 2` can also accept a pawn that is on its start square two half-turns ago but arrived beside the capturer by some other
  route. Consequence for the comparison: the engine offers en passant on the second board of a forked timeline where S3
  does not (`refcheck` filters exactly these, `--tolerate ep-fork`, and counts them). S3 also does not check that the landing
  square is empty; the engine does.
- **Castling (S3), 2D, same board:** king and rook unmoved, all squares between them empty, king moves two files
  towards the rook and the rook lands next to it on the other side. The king's square, the crossed square and the target
  square must not be attacked **on that board** (S3 uses a 2D attack test, `positionIsAttacked`). Attacks coming
  through time or from other boards are not considered for castling (S1/S2 do not say; unverified). Needs
  rook distance >= 3 (so the king's target is empty); on the small variant boards this rarely applies, and the misc
  modes disable castling.
- **Promotion (S3, S4):** reaching the last rank (by a push or a capture) promotes to Queen, Rook, Bishop or Knight, chosen
  with `makeMove(move, promotion)` (default Queen). No promotion by timeline moves, since the rank does not change.
- **"Unmoved" tracking:** every piece carries an `unmoved` flag (a field of the `Piece` value; true when placed, cleared when it moves,
  kept when a board is forked). The pawn double step and castling depend on it.

### Check and legality
- **Check (S1, S3 `mate.checks`):** the mover is in check if, after passing on every mandatory board (copying it forward
  unchanged), the opponent could capture one of the mover's kings with a single move from any of the boards it can move on
  (all tips ending on its turn, active or not, present or not), onto any existing board (past boards included).
  Implemented by `checkingAttacks()` / `inCheck()`; attacker and king are reported on the real boards they come from.
- **Legal turn (S3 `submittable`):** a turn may be submitted iff at least one move was made, no mandatory board is left, and
  the opponent could not capture any king of the mover (`canSubmit()`, `threatsAgainst(color)`, which uses the same
  definition without the pass). Individual moves are pseudo-legal; legality is only judged per turn. Capturing a king is
  therefore impossible in a legal game (`makeMove` asserts it).
- Only single-move captures count as threats (as in S3), not sequences of several opponent moves.

### Game end (S1, S3 `checkmate`/`stalemate`)
- Checkmate: the side to move has no legal turn and is in check, so the other side wins. Stalemate: no legal turn and
  not in check, a draw. A "turn" needs at least one move (there is no pass; S3's `pass` is used only for the check
  simulation).
- Deciding "no legal turn" is a search over combinations of moves across boards. It is done by `TurnSearch`
  (resumable: `step(nodeBudget)` returns `Found`, `None` (proved) or `Running`), which `submitTurn()` only arms; the caller
  steps it (`stepResultSearch`), so `result()` is `Ongoing` until the search proves otherwise and the game never blocks.
  The search is exhaustive (it proves `None`), see [SEARCH.md](SEARCH.md) for the pruning, why it is safe, and its limits
  (a rare huge position stays `Running`; the game then simply continues, S3 has a time limit of 60 s for the same reason).
- "Softmate" (S4: only moves backwards in time remain) is not a separate result.

## Differences from 5d-chess-js (S3)

`tools/refcheck` plays random legal games on the engine and replays every move in S3, comparing at every position (start of
each turn and after every move of the turn) the tip boards, the active timelines, the present boards, the **complete move
list** (every piece, every target, every promotion choice, castling, en passant), whether the position is in check, whether
the turn can be submitted and, at turn starts, checkmate / stalemate. Result of the last run (Release build, search budget 300 000): 500 Standard games (30 turns, 14 400 turn starts, 53 000 positions
compared) and 100 games each of Simplify-No-Bishop, -No-Queen, -No-Rook and K-vs-B: **no divergence** except the documented
en passant one (155 en passant moves offered only by the engine, all on the second board of a forked timeline). The compared
positions contained about 700 en passant, 12 000 castling and 24 000 promotion moves, 4 000 positions in check, and 13 600
checkmate/stalemate answers (S3 answers inside 2 s, positions with at most 3 timelines). Before the vector-table and pawn-direction
fix the same comparison diverged in 60 of 60 games.

The vector tables are compared separately (`compare.js --vectors`): rook, bishop, queen, king and knight are identical to S3.
Deliberate or known differences:

1. **En passant right after a fork** (above): engine `h - 1`, S3 `t - 2`. The engine offers en passant in one position S3 does
   not (second board of a forked timeline); it also checks that the landing square is empty.
2. **Mirrored files.** The engine's starting position has the king on the d-file and the queen on the e-file (S3: queen d,
   king e). Rules are symmetric, so the comparison mirrors files; nothing else is affected.
3. **Promotion pieces.** The engine always offers Queen, Rook, Bishop and Knight. S3 offers the non-royal types that exist in
   the starting position (`availablePromotionPieces`), which differs only in variants that lack a piece type; the comparison
   gives S3 the engine's four.
4. **Castling attack test.** S3's `positionIsAttacked` (`board.js`) lets an adjacent non-attacking enemy piece fail to block the
   ray behind it, and treats a diagonally adjacent enemy pawn as attacking whatever its direction. The engine's 2D test is
   the correct one. Neither case occurred in the compared games (castling is on the back rank, where the pawn case cannot
   arise); if they do arise the difference is intended.
5. **Variants with several original timelines** (Time Line Invasion / Battle / Fragment) cannot be compared: S3 indexes
   timelines even/odd for White/Black, so an original timeline +1 would count as a created one. They are covered only by the
   engine's own tests.
6. **Checkmate / stalemate.** S3's search is heuristic with a time limit; the engine's is exhaustive. They are compared where
   S3 answers within its limit (and the position is small).
7. The engine disables double steps and castling in some variants through rule flags; in S3 these are the "unmoved" marks,
   so the comparison maps such pieces as already moved.

## What is not verified
- The rules were checked against S3, not against the game itself (its wiki could not be fetched, see Sources). Where S3 and the
  game could differ, the engine follows S3 (except for the differences above).
- The double step along the timeline axis and the 2D-only castling attack test are S3's rules and could not be checked
  against the game's wiki.
- The property tests (`tests/property_test.cpp`) only show self-consistency (soundness of the move generator, immutability of
  boards, clone independence, invariants after every turn); they are not rule verification. The independent oracles are
  `tools/refcheck`, the vector-table comparison and the directed tests.
- Variant boards in the misc modes use the same rules, except castling, which they turn off.
