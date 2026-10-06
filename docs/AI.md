# The AI opponent engine

This document describes `ai::Search` (`include/ai/`, `src/ai/`), the engine-side AI opponent. It decides a whole turn of the
side to move; it has no UI and no threads, and it runs in the web build. For the rules see [RULES.md](RULES.md); the legality
machinery it reuses (`TurnSearch`) is explained in [SEARCH.md](SEARCH.md).

```cpp
#include "ai/Search.h"

ai::Search search(game, {ai::Level::Normal, /*seed=*/42});
while (search.step(500) == ai::Search::Status::Running) { /* draw a frame, show search.progress().fraction */ }
if (search.hasTurn()) {
  for (const Core::Move& m : search.bestTurn()) game.makeMove(m);   // a complete legal turn, in playing order
  game.submitTurn();
} // else: the side to move has no legal turn (the game is over: mate or stalemate)
```

`ai::playTurn(game, options)` (`include/ai/Play.h`) does exactly this synchronously (tests, tools, self-play).

`progress().fraction` (for a thinking bar) is `max(nodes / cap, share of the planned iterations done)`, the iterations weighted
6^i (measured cost ratio 4-17x per deeper iteration) and the running one by its root turns done; it never decreases and is 1.0 only
once Done.

## 1. What is hard about a 5D chess AI

A *turn* is a set of moves on several boards (one on every mandatory board, plus jumps into the past that move the present
back). So the unit of search is the turn, not the move, and the first problem is to **generate candidate turns**: with `k`
mandatory boards of ~30 moves each there are ~30^k turns, and most are illegal (a king left capturable on some board).

The second problem is that the engine's own mate proof (`TurnSearch`) is exact and cheap, but only answers "does a legal turn
exist?" - it does not enumerate turns. The AI uses it as an oracle for mates, and builds the candidates itself.

## 2. Design

### Turn generation (`src/ai/TurnGen.*`)

A resumable generator that builds turns move by move on its own clone of the game, as a depth-first search with a **beam**:

1. A *frame* belongs to one mandatory board (the one with the lowest timeline id that still has to be moved on). Its
   candidate moves are all pseudo-legal moves of that board (`IGame::legalMovesFrom`) plus the **time jumps** of the other
   mandatory boards (a jump that creates a timeline moves the present back and so clears the obligation on all boards: it is
   the way out of some checks, and the only way to a turn made of one move).
2. Every candidate is tried once (one *node*): the move is made, **discarded if one of the mover's kings can now be
   captured** (this is permanent, SEARCH.md F2: `TurnSearch::kingCapturable`), otherwise scored with the static evaluation
   of the position after it.
3. Candidates that give check (the moved piece attacks an enemy king on its board; same-board geometry only) are ordered
   before all others and are exempt from the jump limit; the rest by score. The best `beam` become children, at most
   `maxTravel` of them time jumps ("travel moves only when promising": a jump is promising if it captures, scores well or
   gives check).
4. A child that completes the turn (`canSubmit()`) is a *leaf* = one candidate turn; the generator then goes on with the next
   one. A child counts against the beam only if it produced a leaf (dead ends are free, but the generator gives up after
   `maxFailures` of them). Moves of different boards are chosen in a fixed board order; the few turns that still arise
   twice (a lateral time jump listed in the first frame and in its own board's frame) are dropped by comparing sorted move sets.

Not complete by design: beams, and no moves on optional boards (except jumps that clear the obligation). **Fallback ladder**
when the generator finds no turn at all (e.g. the only escape from check is a quiet jump of an optional board): (1) the first
pass is repeated in *rescue mode* (optional-board moves included, unlimited jumps, wider beam, up to `maxNodes / 50` dead ends; about 3% of extreme in-check
multi-board positions are still beyond it), so the turns are still
searched and evaluated; (2) only if that fails too, `TurnSearch` (exact) supplies a legal turn, which is then scored (mate /
stalemate proof and static evaluation) and reported with `Progress::usedFallback`.

### Search (`src/ai/Search.cpp`)

**Negamax alpha-beta over whole turns**, with **iterative deepening** over 1, 2, 4, 6 turns (even depths only, so that the
last turn searched is the opponent's reply and a capture is never judged without the recapture). Each node is a position at
the start of a side's turn; its children are the generator's turns; after a turn is generated the position is cloned and
submitted (`submitTurn()`).

* **Node cap and cost model.** `maxNodes` is a hard cap on everything: generator moves, proof slices and turn clones all
  count, in proportion to the real work, so that a "node" takes about the same time in every position. Only the excess over a single full board (1 timeline, 1 mandatory board, 32 pieces, which costs 1 per move) is charged:
  a generator move costs `1 + (pieces on all tips - 32) / 12 + (timelines - 1) / 3 + (mandatory boards - 1)` (computed once
  per frame), cloning and submitting a turn `4 + (timelines - 1) + (pieces - 32) / 12`, one 32-node slice of the opponent's
  legal-turn proof `32 x (1 + ((pieces - 32) / 64 + (mandatory boards - 1)) / 2)`. The cap is tested at every logical point (a turn was generated, a proof ended) and inside the
  generator (it is handed only what is left of the cap, so it stops at the same move however the caller slices its
  budget). The only work allowed past it is the first pass up to its first evaluated root turn, because a legal turn is
  always returned; that work is bounded too: after a quarter of the cap spent in the first pass's generator it *squeezes*
  (beam 1, one scored survivor per frame, few more dead ends). On many mandatory boards the root turn count and the proof
  size shrink (`scaleForBoards`). A cap smaller than one complete turn on 14 boards (about 10-40k nodes) cannot be
  honoured. Result: a decision stays within about 1.0-1.3x of its cap, and Hard stays near 2 s native on 16-21 timeline
  multiverses; the price is depth: Hard reaches only depth 1-2 there, Normal depth 1-2.
* **Mate / stalemate proof at every node.** After submitting, the game's own legal-turn proof (the `TurnSearch` armed by
  `submitTurn()`) is stepped for up to `probeNodes` nodes. "No legal turn" is a mate (`+-MateScore - ply`) or a stalemate
  (0). This is exact, so a mate in one is found whenever the mating turn is among the candidates, and a turn that allows a
  mate in one is avoided whenever the mating reply is among the opponent's candidates (the last ply uses a wide beam for that).
* **Static evaluation at the horizon** (section 3), taken after the last turn of the line.
* **Mate claims.** Only a mate in one for us at the root counts as *proven* (it is verified by the engine's exact proof,
  whatever the beams were): it ends the search and is always chosen. A deeper mate value comes through the opponent's beam
  and may miss a defence, so it stays an (extremely high) score and the search keeps deepening. A proof that times out
  (`probeNodes`) is treated as "not mate" and the static evaluation is used.
* **Iterations are anytime.** The depth-1 pass visits root turns (static eval + mate proof) and is the only one that can be
  cut short by the cap after a single turn; the next iteration searches the root turns in the order of the previous one (so the best move is searched first and alpha-beta
  prunes the rest). When the node cap is reached the decision is made from the last *completed* iteration (or, in the first pass, from the root
  turns evaluated so far), never from a half-searched one.
* **Randomness.** Root turns within `margin` centipawns of the best are all candidates; one is picked with a seeded
  SplitMix64 (a proven mate is always chosen). Alpha-beta runs with the root window `[best - margin - 1, inf)`, so every
  turn that can be chosen has an exact value.
* **Verification.** The chosen turn is replayed on a clone of the original game and must satisfy `canSubmit()`; otherwise the
  next candidate is tried, then `TurnSearch`. `bestTurn()` therefore never returns an illegal turn; when there is no legal
  turn `hasTurn()` is false.

### Resumable and deterministic

All state lives in `Search::Impl`: an explicit stack of plies (each with its generator), no recursion across `step()`
calls, no threads, no globals. `step(nodeBudget)` returns after about `nodeBudget` nodes (it may overshoot by the slice it
is in: one generator move or one proof slice, whose charges are given in the cost model above: a few nodes in small
positions, up to a few hundred in big multiverses). A node is one unit of that charge.

The result depends only on the position, the options and the seed: not on how the caller slices its budgets, and not on
timing. The node limit is only tested at logical points (a root turn is complete, an iteration ends), where the node count is
independent of the slicing.

## 3. Evaluation (`src/ai/Eval.cpp`)

Centipawns, from the side to move's point of view; summed over the **latest board of every timeline** (a "tip"):

| Term | Value | Why |
|---|---|---|
| Material | P 100, N 300, B 320, R 500, Q 900 | standard; a piece that exists on several tips counts on each: a timeline is another front, so leads are worth more |
| Centralisation | knights, bishops, queens: 6 per step towards the centre (max 3) | cheap proxy for mobility |
| Development | +12 per knight/bishop off its back rank | opening play |
| Pawn advance | 5 per rank | pawns must go somewhere |
| Timelines created | -12 per timeline created by the side minus those of the opponent | a new timeline is another board the opponent may attack and that must be defended; kept small |
| King safety | search, not evaluation | (a) every move that leaves a king capturable is dropped by the generator; (b) a position in which a king is mated is found by the mate proof; (c) check-giving moves are searched first |

"Mobility" and "present pressure" are not separate terms: the mate proof and the opponent-reply plies make the
consequences of check and zugzwang visible exactly, and the evaluation stays a few array scans per tip (cost matters: it
runs once per candidate move). Weights are in `EvalWeights` (`include/ai/Eval.h`).

## 4. Difficulty

`Options{level, seed, maxNodes}`; `maxNodes` (0 = the level's value) is a knob for tests and tuning.

| Level | Deepest iteration | Root beam / turns | Randomness | Node limit |
|---|---|---|---|---|
| Easy | 1 turn (own turn + mate proof; jumps allowed, 1 per node) | 5 / 8 | any turn within 120 cp of the best | 10 000 |
| Normal | 4 turns | 16 / 60 | within 8 cp | 300 000 |
| Hard | 6 turns | 24 / 80 | none (ties by seed) | 1 200 000 |

Beams narrow towards the leaves; exact numbers are in `levelParams()` (`src/ai/Search.cpp`).

## 5. Measured (Release, one core of the development machine)

`ai_bench speed` (a decision = stepping `step(500)` to Done; nodes as counted by `Progress::nodes`):

| Position | Level | Nodes | Time per decision | Longest `step(500)` |
|---|---|---|---|---|
| Standard opening | Easy / Normal / Hard | 185 / 42 000 / 910 000 | 0.1 / 30 / 760 ms | 0.2 / 0.7 / 1.6 ms |
| Standard, after 12 turns | Easy / Normal / Hard | 356 / 104 000 / 1.28 M | 0.5 / 190 / 2 900 ms | 0.5 / 1.7 / 3.2 ms |
| Timeline Battle opening | Easy / Normal / Hard | 1 000 / 218 000 / 946 000 | 0.6 / 146 / 620 ms | 0.4 / 0.6 / 0.8 ms |
| Timeline Battle, after 12 turns | Easy / Normal / Hard | 2 200 / 150 000 / 905 000 | 2 / 178 / 970 ms | 0.9 / 1.3 / 1.5 ms |

Throughput is 0.5-2 M nodes/s (the table above is from before the cost model was introduced and is only indicative; Easy is
weak through depth and beam, not through blindness to jumps). The cap is hard. On big multiverses (`ai_bench big`, Release,
one core, loaded machine):

| Positions | Level (cap) | Nodes / cap | Time |
|---|---|---|---|
| Timeline Invasion, 15 timelines (`big`) | Easy (10 000) / Normal (300 000) / Hard (1 200 000) | 0.1-0.4 / 1.00 / 1.00 | 1 / 0.1 / 0.4-0.6 s |
| Standard, 16-21 timelines, 9-18 mandatory boards, in check | Easy / Normal / Hard | 0.4-0.9 / 1.0 / 1.0 | 5 ms / 0.2-0.4 s / 0.7-2.1 s |

Before the cost model the same kind of positions cost Normal up to 2.0x its cap (12.8 s on one 17-timeline position) and Hard
8.7-9.1 s on full-board Standard multiverses (14-18 mandatory boards, in check), Normal about 2 s, Easy up to 5x its cap.

Self-play (`ai_bench selfplay --games 18 --turns 40`, nine modes, colours alternate, unfinished games adjudicated by material
> 300 cp, so a score is wins + draws/2):

| Pair | Score of the first | W / D / L |
|---|---|---|
| Normal vs Easy | 15.0 / 18 | 14 / 2 / 2 |
| Hard vs Easy | 15.0 / 18 | 15 / 0 / 3 |

Hard vs Normal was not completed (each Hard game takes minutes). Losses to Easy happen mostly in the tiny, tactical modes
(Timeline Fragment, Timeline Battle) where a lucky random move can mate; 18 games are noisy (+-2.5 points).

## 6. Limits

* Turn generation is a beam: a mate or a defence that needs one particular move on a *second* board, among many equal ones, can
  be missed unless it gives check on its board (check detection is same-board geometry; discovered and cross-board checks
  are not detected, they rank by score). The mate proof itself is exact.
* On 16+ timelines Hard reaches only depth 1-2 within its cap (the cap is in work units: a big multiverse makes every node
  expensive); on the web build, which is several times slower, think time grows accordingly (lower `maxNodes` there if needed).
* Optional boards are used only in rescue mode (when nothing else is legal); otherwise only jumps that clear the obligation,
  so the AI does not make "extra" moves on boards it is not obliged to move on. Jumps are limited per node (`maxTravel`); big multiverses are searched shallowly because a node costs
  more (the node cap counts that, so time stays roughly constant).
* The evaluation knows nothing about timeline strategy (who controls more active timelines, the present). It is a material
  engine that sees tactics through search; weights are untuned beyond the self-play below.
* No transposition table, no quiescence search, no move-ordering history: depth is bought with beams.
* The first `step()` call can take a few milliseconds longer (cloning the game, the generator's first frame).

## 7. UI integration (Play vs Computer)

`PlayScreen` (`src/Screens/PlayScreen.cpp`) runs the opponent; the pure parts (side, level, seed, record metadata, rebuilding a game a
few turns back) are in `include/play/VsAi.h` and unit tested (`tests/vsai_test.cpp`).

* **Time slicing.** The engine is clock-free; the screen is the only thing that looks at a clock. Each frame that it is the computer's turn
  and the game's own legal-turn search is not pending, it calls `step(150)` in a loop, timing every slice and stopping when the time used plus the longest slice so far would exceed `kAiFrameBudget` (the `Search` itself is created in the frame before the first slice, since it clones the game):
  **6 ms native, 4 ms on the web** (the browser's one thread also renders, mixes audio and runs the result search). One `step` is
  about 0.1-0.3 ms natively, so the overshoot is small; the window stays responsive (pan, zoom, Esc, Back) and the frame times measured
  are below. `ainodes` in `tools/ui_script` replaces the clock by a fixed number of nodes a frame for the screenshot tests.
* **Seed.** A game has a seed (drawn when it starts, stored in the record); the search of a turn is seeded with
  `searchSeed(seed, history().size())`. The same position at the same turn of the same game always gets the same answer, whatever the
  slicing, the frame rate or Continue / Load in between (within one build of the engine: a later change to the evaluation or the
  node costs can change what a saved game's computer would have played, and so a replayed game's later turns).
* **Playing the turn.** When the search is `Done` (and at least half a second has passed, so an instant answer is a moment of thought and
  not a flicker) the moves of `bestTurn()` are played one at a time through the same path as a click: flight animation, move sound, camera.
  A move waits for the previous flight and 0.3 s; then `submitTurn()`. The board ignores clicks while it is the computer's turn
  (the camera still pans and zooms), Submit and Deselect are disabled, Undo works as below.
* **Thinking indicator.** The HUD pill says "Computer is thinking" with three dots and a thin bar that follows `progress().fraction`
  (eased; the engine's own estimate, used as is). Under Reduce motion the bar follows the
  value directly and the dots stay still. While the moves are played the pill says "Computer is moving".
* **No turn.** If the game is over there is nothing to search (`result()` is not Ongoing: the normal end card shows). If the search
  finds no turn although the game goes on (the legal-turn proof was cut off before), the screen stops asking and the side is played by hand.
* **Undo.** `IGame::undo()` only takes back the moves of the unsubmitted turn, so the screen first does that, move by move. With nothing
  pending, vs-Computer Undo takes back **the player's last turn and the computer's reply** (two turns) by replaying the game's history
  without them (`play::replayPrefix`: a fresh game from `modeId()` / `startPosition()`, no record text, no blocking result search), or only
  the player's turn when the computer is still thinking (its search is dropped). Playing Black, the computer's opening turn stays. It is
  disabled while the computer plays its moves.
* **Cancelling.** The search is dropped on Undo, Back, a new game, and when the game ends or changes hands; loading creates a new screen.
  The search works on clones, so dropping it needs nothing else.

Measured frame cost of `update()` plus drawing while the computer thinks (`FDCHESS_PERF=1`, Release, Hard, Standard opening, native
under Xvfb): average 5.5 ms, worst 7.9 ms per frame. Web build (emsdk 4.0.10, headless Chrome with software GL, a full Easy game of 20
turns): `update()` while the computer was busy averaged 0.2-0.5 ms with a worst of 3.9-7.8 ms (Easy decides in under a millisecond of
search; the browser's frame gap there was dominated by software rendering, not by the search).

## 8. Tuning and tools

`tools/ai_bench` (`-DFDCHESS_BUILD_AIBENCH=ON`, Release):

```bash
cmake -S . -B build-bench -DCMAKE_BUILD_TYPE=Release -DFDCHESS_BUILD_GAME=OFF -DFDCHESS_BUILD_TESTS=OFF -DFDCHESS_BUILD_AIBENCH=ON
cmake --build build-bench -j --target ai_bench
./build-bench/tools/ai_bench/ai_bench speed [--mode standard]
./build-bench/tools/ai_bench/ai_bench selfplay --games 18 --turns 40 --a hard --b easy [--mode ID] [--game K]
```

`big` plays cheap turns to a 15-timeline multiverse and prints nodes against the cap and the time at every level; `file POS.5dp` decides one position; `speed` prints time per decision and nodes/s per level on an opening and a midgame position; `selfplay` plays A against B
(colours alternate, the nine modes cycle), adjudicates unfinished games by material (> 300 cp) and prints the score;
`--game K` replays one game and prints every turn in notation. To tune: change `EvalWeights` or `levelParams()`, run
`selfplay` with a few dozen games per pair (scores of 18 games are noisy: +-2.5 points), and keep a change only if it holds
up across pairs. The tests (`tests/ai_test.cpp`) pin the behaviour that must not regress: legal turns in every mode, mates
(single board, two boards, a jump into the past), not hanging the queen, determinism, the step budget.
