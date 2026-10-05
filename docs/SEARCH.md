# How the engine decides "is there any legal turn?"

This document explains the search behind checkmate and stalemate in `include/chess.h` / `src/chess.cpp`
(`TurnSearch`). It is written for a reader who knows chess but not 5D chess. The rules themselves are in
[RULES.md](RULES.md); the way the engine is checked against another implementation is in
[../tools/refcheck/](../tools/refcheck/compare.js).

## 1. The problem

In ordinary chess a turn is one move, so "is the player checkmated?" means "does any single move leave the king safe?" and
the answer comes from one move list.

In 5D chess a *turn* is a **set of moves on several boards**. The multiverse is a collection of timelines, and each
timeline ends in a board ("tip"). On your turn you must make one move on every *mandatory* board (the tips that lie on the
present, see RULES.md), and you may move on *optional* boards as well. A move can leave its board, land on another
board (also in the past, which forks a new timeline), and a turn is only legal if, **when all your moves are made**, the
opponent can capture none of your kings on any board it will be able to move on.

So "does this player have a legal turn?" is not a lookup in a move list but a search over *combinations* of moves:
choose a move on board 1, then one on board 2, and so on, and check the result. Checkmate is "no combination works and I
am in check", stalemate is "no combination works and I am not in check". The engine has to *prove* the "no" cases.

## 2. Why the naive search explodes

A plain depth-first search ("try every move, recurse, undo") has three sources of blow-up:

1. **Orderings.** The same set of moves played in a different order is the same turn. With `k` mandatory boards there are
   `k!` orderings of every combination; with 4 boards that is 24 times the work.
2. **Optional boards.** Every optional board adds a choice of "move here too (any of ~30 moves) or not". Most of those
   moves are irrelevant, but a naive search cannot know that, so it multiplies the space by ~30 per optional board.
3. **Late failure.** A king that is capturable after the first move stays capturable whatever else is played, but a naive
   search only notices at the end of the turn and keeps trying all combinations of the *other* boards below it.

Measured on the old exhaustive search (budget 200 000 nodes, see section 7): one decision took up to 39 s in random
games, 3-board mates came back "unknown" and so could not be decided.

## 3. The strategy, step by step

All of this lives in `TurnSearch` (`src/chess.cpp`, the comment above `struct TurnSearch::Impl` has the full argument).

**A compact private copy.** The search copies the position into plain arrays (one 64-byte block per board) and applies and
undoes moves there instead of building real `Board` objects. A node costs a fraction of a microsecond to a few
microseconds instead of ~10 µs. The *move generator is the same code* as the game's (a template over a small board-access
interface), so the search cannot disagree with the rules about which moves exist.

**The move set is fixed (F1).** A move starts on a tip of the mover's colour and ends on a board of the mover's colour.
Moves create boards of the *opponent's* colour only (the board after the move, or the first board of a forked timeline).
So the set of possible moves is computed once, at the start; during the turn it only shrinks, when a tip has been used up
(by moving from it or onto it).

**Threat test (F2): captures are permanent.** A king is "capturable" when an opponent piece standing on one of its tips
can capture it (same movement rules, any board of the opponent's colour). Boards are immutable, tips of the opponent's
colour stay tips, and moves only *add* boards, so once a king is capturable it stays capturable whatever else is played.
Therefore:

* the search tests every position it reaches and **abandons it as soon as a king is capturable** (no waiting for the end
  of the turn);
* the test is a pair test, *(opponent piece on an opponent tip) x (my king on a board of the opponent's colour)*,
  decided by arithmetic on the displacement between the two squares plus a check that the squares in between exist and are
  empty. No move lists are built. (`attacks`, `anyThreat`; kept honest by a test that compares it with
  `IGame::threatsAgainst`.)

**"Threat reduction" ordering.** 5d-chess-js (`mate.js:80-110`) orders candidate moves by how much they reduce the set of
checking pieces. Here the same idea is applied with two cheaper devices that need no separate bookkeeping:

* every candidate gets a *dead* flag, computed once at the start: if the board a move leaves behind (its source half alone)
  already lets a king be captured, the move can never be part of a legal turn, wherever it lands; the same for the
  whole move when it extends a tip. In a position with a king in check, these flags kill every move that does not capture
  the checker, block it or move the king, before the search starts. Dead candidates are skipped without a node;
* the mandatory boards are searched first, the ones with **fewest live moves first** (they are the likeliest to fail, so
  failure is found at the top of the tree), and within a board king moves, then captures, then other moves, and
  moves inside the board before moves to other boards before jumps into the past.

**Mandatory boards first, optional boards only when needed (F4).** A move from an optional board that neither forks a
timeline nor lands on a mandatory board cannot change which boards are mandatory and can only add threats, so a legal turn
without it is still legal, *unless* another move of the turn lands on its source or target tip (then that move would fork
instead of extend), or the board belongs to a timeline the mover can still activate by forking (activation changes where the
present is). Such moves are only tried in those cases. What stays in the search from optional boards are exactly the
moves that matter: past jumps that create a timeline and so can move the present back (clearing the obligation to move on
the mandatory boards, "a jump into the past clears obligations", as in `tests/directed_test.cpp`), and moves onto
mandatory boards. (A turn needs at least one move; while no move has been made this rule is switched off.)

**Canonical order of independent moves (F3).** Moves from different boards commute (same result in either order, both still
playable) unless one lands on the other's source board, both land on the same board, or both create a timeline. Only one
order of independent moves is searched. Order *does* matter when a move creates a timeline: IDs are handed out in creation
order (White max+1, Black min-1) and the ID decides which timelines are neighbours, so two forks cannot be swapped. The
rule used is: moves that create a timeline come after those that do not, otherwise the order of a ranking of the source
boards. This removes the `k!` factor.

**Forward checking (F5).** A mandatory board stops being mandatory only if a move leaves it, a move lands on it, or a fork
pulls the present back before it. After each move the search checks that every mandatory board still has such a way
out that is not already dead; if one has none, the whole subtree is hopeless and is dropped. This is what turns
"board 3 cannot be saved" from an exponential re-discovery into an immediate failure. (Dead flags found at a node are
remembered for its subtree.)

**Two phases and restarts.** Phase 1 searches only optional-board moves that obviously matter (landing on a mandatory
board, or able to pull the present back); if it finds a legal turn that is a proof. Phase 2 is the complete search. When
a search keeps failing, it restarts with the boards that failed most ranked first (the run limit doubles, so the last run is
effectively unbounded and the procedure stays complete).

## 4. The resumable design

`TurnSearch` never recurses across calls: the DFS state is an explicit stack of frames plus an undo stack, so it can stop
after any node and continue later.

```cpp
TurnSearch search(game);                   // snapshots the position
while (search.step(5000) == TurnSearch::Status::Running) { /* draw a frame */ }
// Found: search.turn() lists the moves     None: no legal turn (checkmate or stalemate)
```

Statuses: `Found` (a legal turn exists; `turn()` lists its moves and they are verified by the tests), `None` (proved:
no legal turn), `Running` (budget used up, call `step` again). `step(nodeBudget)` counts one node per move tried (and one
per candidate when the dead flags are computed at the start).

`IGame` owns the search for the result: `submitTurn()` only *arms* it (`resultPending()`), and the caller drives it with
`stepResultSearch(budget)` (or `resolveResult()` in tests and tools). Until it proves otherwise `result()` is `Ongoing`;
on `None` it becomes a win for the other side if the side to move is in check, else a draw.

**How the UI uses it** (`src/Render/Controller.cpp`): after the Submit button, `ChessController::update` runs
`stepResultSearch(100)` repeatedly until about 4 ms of the frame are used, and the HUD hint reads "Checking
position..." while `resultPending()`. When the search resolves to a win or draw, the existing end-game overlay appears.
The common case resolves in the first frame. There are **no threads**: the web (Emscripten) build runs single-threaded,
and a time-boxed step needs no synchronisation with the game state.

## 5. Why the pruning never discards the only legal turn

The invariant: *if the position has a legal turn, the search reaches a position in which that turn's moves have been made
(in some order), and it is then reported Found.* Each rule keeps it:

* **F2 (threat pruning, dead flags).** A pruned position contains a capturable king. Capturability is permanent, so no
  completion of it is legal. A dead flag says the same about a single move (its source half alone, or both halves while the
  tip it extends is still a tip), so no legal turn contains that move. Flags are computed in the starting position only,
  and the argument that they stay valid uses monotonicity (more boards can only add captures).
* **F3 (canonical order).** Take any legal turn. Swap adjacent *independent* moves that are out of canonical order; each swap
  keeps the turn legal and its result identical, and the process ends. The result has no out-of-order independent pair, so the
  search visits it. The "creates a timeline" status of a move does not change under such swaps.
* **F4 (irrelevant moves).** Take a *shortest* legal turn. If it contained a move the rule skips, deleting that move would give
  a shorter legal turn (it neither creates a timeline nor lands on a mandatory board, nobody lands on its boards, its
  timelines cannot be activated; the turn is not left empty), a contradiction. So F4 never removes a move of a shortest
  legal turn; combined with F3 on that turn the search still visits it.
* **F5 (forward checking).** It only drops a node in which some mandatory board has no move that could free it. A legal
  completion would need such a move.
* **Restarts, phases.** Phase 1 is an optimisation that can only find turns; phase 2 repeats the search without its extra
  restriction.

The argument is checked, not just stated: `tests/search_test.cpp` compares the search with a plain exhaustive search that
uses nothing but `IGame::makeMove` / `canSubmit` on 100 random small multiverses, with every combination of switching a
reduction off. That test found a real bug during development (F4 skipping the only available move when no move had been made
yet).

### Known limits

* The search is exhaustive, so a position that is *not* decided cheaply stays `Running`; the game keeps working (the result
  stays `Ongoing`, the HUD keeps showing "Checking position..."), it just does not end by itself. In the random-game
  benchmark below this still happens in about 1 of ~900 turns, always in huge positions (18+ timelines, 7-10 mandatory boards)
  where every board has dozens of live moves and the failure comes from several boards at once. Searching such a position
  is a constraint-satisfaction problem; stronger propagation (learning which *pair* of boards conflicts) is future work.
* The threat test is recomputed in full at every node (a pair test, no move lists), not updated incrementally from the parent. It
  is exact and simple but its cost grows with the number of timelines; a delta version is future work.
* Reproduction of a position that stays `Running`: `refcheck --mode standard --seed 173 --turns 30` (turn start ~14, eight
  timelines). The 16-minute stall that was noticed there was `refcheck` waiting for this search with an unlimited budget; it now
  uses `--search-budget` and ends the game as "unresolved".
* Only single-move captures count as threats (the same limit as 5d-chess-js).
* Not implemented, would help: a transposition table, learning of failing board pairs, multi-threading (deliberately not).

## 6. A worked example

Two mandatory boards, **A** and **B** (timelines 0 and 1), Black to move on both: the benchmark's `MateRow` with two boards.
On each board the Black king h8 is in check from a White rook on a8 along the eighth rank, and Black has pawns g7 and h7.

*Step 1, dead flags (one node per candidate move, 12 here).* Each candidate move is tried once in the starting position, source half only:

```
A: a pawn push -> the board it leaves still has Ra8 attacking Kh8   dead
A: Kg8         -> Kg8 is still on the rook's rank                   dead
B: (the same moves)                                                 dead
```

Nothing is left to search: **None after 12 nodes**. Black is in check, so this is checkmate. The old search needed
about `6^k` leaf tests here and gave up (budget) on the larger variants.

Remove the pawn on h7 on both boards and the king has an escape square:

```
 flags: only  A: Kh8-h7  and  B: Kh8-h7  stay alive (15 nodes in total for the whole search)
 root  (mandatory A, B; A ranked first)
  +-- A: Kh7      king safe; forward check: B still has a live move
        +-- B: Kh7   king safe, no mandatory board left                     =>  Found
```

`search.turn()` is `[A: Kh8-h7, B: Kh8-h7]`. Without the dead flags the same search would try the g- and h-pawn moves of
board A first and, below each of them, every move of board B before ever reaching the king.

A case where the *order* matters is a jump into the past: in `tests/directed_test.cpp` (the knight on timeline 1 jumping back
to its own first board) the fork is a candidate from an optional-looking board, it is searched after every move that does not
fork (the canonical order), and it ends the turn because the new timeline's board lies before the present.

## 7. Benchmark

`tools/refcheck/turnbench.cpp` (target `turnbench`, built with `-DFDCHESS_BUILD_REFCHECK=ON`, Release):

```bash
cmake -S . -B build-bench -DCMAKE_BUILD_TYPE=Release -DFDCHESS_BUILD_GAME=OFF -DFDCHESS_BUILD_TESTS=OFF -DFDCHESS_BUILD_REFCHECK=ON
cmake --build build-bench -j --target turnbench
./build-bench/tools/refcheck/turnbench                # random games over all 9 modes, then the constructed mates
TURNBENCH_SLOW=1 ./build-bench/tools/refcheck/turnbench --frame-nodes 1000   # list the slow decisions
```

The "before" column was measured on the commit before the rewrite (`5832353`) with the same random games (seeds 1-6, 25
turns, 9 modes, decision time = `submitTurn()` including its search with the default budget 200 000) and the same mates.

| Measurement | Before (`5832353`, synchronous exhaustive DFS) | After (`TurnSearch`) |
|---|---|---|
| Random games, 9 modes (585 turns before, 895 after): median decision | 0.09 ms | 0.2 ms |
| Random games: 99th percentile decision | 11 300 ms | 23 ms |
| Random games: slowest decision | 38 800 ms (blocking `submitTurn`) | 1 turn in 895 does not finish in 60 s (huge position, see limits); next slowest 10 s |
| Slowest single call | the whole decision (blocking) | `step(5000)`: 207 ms (huge positions); 967 of 1 831 calls took over 8 ms |
| Mate on 1 / 2 / 3 / 4 boards (no bystanders) | 0.1 / 0.3 / 3.4 / 115 ms, proven | 0.01 ms each, 5 / 12 / 19 / 26 nodes, proven |
| Mate on 1 / 2 / 3 / 4 boards (5 bystander pawns per board) | 0.1 / 2.3 / 96 ms, **k=4: unknown after 788 ms and 200 000 nodes** | 0.01 ms each, 30 / 62 / 94 / 126 nodes, proven |
| Cost per node | ~10 us (real `Board` copies) | 0.1-2 us in the mates, up to ~40 us in 18+ timeline positions |

Note on the frame budget: a `step(5000)` call finishes under 8 ms in about half of the calls of the random-game run and always
in the constructed mates (6 ms at most); in positions with many timelines a node costs up to ~40 us, so 5000 nodes can take
200 ms. That is why the game does not call `step(5000)`: it calls `stepResultSearch(100)` in a loop until ~4 ms of the frame
are used, which is independent of the position.

The mates are Black to move on `k` identical 8x8 boards (king h8 boxed in by pawns g7/h7, White rook a8 giving check),
"bystanders" adds five Black pawns per board with harmless moves.

## 8. Reproducing the comparison with 5d-chess-js

The *rules* (not the search) are checked against the reference engine by `tools/refcheck/compare.js`; see its header and the
"Differences from 5d-chess-js" section of [RULES.md](RULES.md).
