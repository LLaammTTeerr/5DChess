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
   one. Moves of different boards are chosen in a fixed board order, so no turn is produced twice in another order.

Not complete by design: beams, no moves on optional boards (except jumps that clear the obligation). When the generator
finds nothing, `Search` falls back to `TurnSearch` (exact, any budget) and plays whatever legal turn it finds.

### Search (`src/ai/Search.cpp`)

**Negamax alpha-beta over whole turns**, with **iterative deepening** over 1, 2, 4, 6 turns (even depths only, so that the
last turn searched is the opponent's reply and a capture is never judged without the recapture). Each node is a position at
the start of a side's turn; its children are the generator's turns; after a turn is generated the position is cloned and
submitted (`submitTurn()`).

* **Mate / stalemate proof at every node.** After submitting, the game's own legal-turn proof (the `TurnSearch` armed by
  `submitTurn()`) is stepped for up to `probeNodes` nodes. "No legal turn" is a mate (`+-MateScore - ply`) or a stalemate
  (0). This is exact, so a mate in one is found whenever the mating turn is among the candidates, and a turn that allows a
  mate in one is avoided whenever the mating reply is among the opponent's candidates (the last ply uses a wide beam for that).
* **Static evaluation at the horizon** (section 3), taken after the last turn of the line.
* **Iterations are anytime.** The depth-1 pass visits all root turns (static eval + mate proof) and always completes; the
  next iteration searches the root turns in the order of the previous one (so the best move is searched first and alpha-beta
  prunes the rest). When the node limit is reached the decision is made from the last *completed* iteration, never from a
  half-searched one.
* **Randomness.** Root turns within `margin` centipawns of the best are all candidates; one is picked with a seeded
  SplitMix64 (a proven mate is always chosen). Alpha-beta runs with the root window `[best - margin - 1, inf)`, so every
  turn that can be chosen has an exact value.
* **Verification.** The chosen turn is replayed on a clone of the original game and must satisfy `canSubmit()`; otherwise the
  next candidate is tried, then `TurnSearch`. `bestTurn()` therefore never returns an illegal turn; when there is no legal
  turn `hasTurn()` is false.

### Resumable and deterministic

All state lives in `Search::Impl`: an explicit stack of plies (each with its generator), no recursion across `step()`
calls, no threads, no globals. `step(nodeBudget)` returns after about `nodeBudget` nodes (it may overshoot by the slice it
is in, at most ~40 nodes: one generator move, whose cost is `1 + timelines / 3` so that big multiverses count more, or a
32-node slice of the mate proof). A node is one move tried by the generator, or one node of the mate proof.

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
| Easy | 1 turn (own turn + mate proof) | 5 / 8 | any turn within 120 cp of the best | 4 000 |
| Normal | 4 turns | 16 / 60 | within 8 cp | 150 000 |
| Hard | 6 turns | 24 / 80 | none (ties by seed) | 900 000 |

Beams narrow towards the leaves; exact numbers are in `levelParams()` (`src/ai/Search.cpp`).

## 5. Measured (Release, one core of the development machine)

`ai_bench speed` (a decision = stepping `step(500)` to Done; nodes as counted by `Progress::nodes`):

| Position | Level | Nodes | Time per decision | Longest `step(500)` |
|---|---|---|---|---|
| Standard opening | Easy / Normal / Hard | 185 / 42 000 / 910 000 | 0.1 / 30 / 760 ms | 0.2 / 0.7 / 1.6 ms |
| Standard, after 12 turns | Easy / Normal / Hard | 356 / 104 000 / 1.28 M | 0.5 / 190 / 2 900 ms | 0.5 / 1.7 / 3.2 ms |
| Timeline Battle opening | Easy / Normal / Hard | 1 000 / 218 000 / 946 000 | 0.6 / 146 / 620 ms | 0.4 / 0.6 / 0.8 ms |
| Timeline Battle, after 12 turns | Easy / Normal / Hard | 2 200 / 150 000 / 905 000 | 2 / 178 / 970 ms | 0.9 / 1.3 / 1.5 ms |

Throughput is 0.4-1.5 M nodes/s. Targets (Normal <= 1-2 s, Easy <= 0.3 s) are met with a wide margin; Hard is the
"think for a few seconds" level (the node limit is soft: an iteration in progress when it is reached is dropped, but the
depth-1 pass and the mate proofs always finish).

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
* Optional boards are not used (only jumps that clear the obligation), so the AI never makes "extra" moves on boards it is not
  obliged to move on. Jumps are limited per node (`maxTravel`); big multiverses are searched shallowly because a node costs
  more (the node limit counts that, so time stays roughly constant).
* The evaluation knows nothing about timeline strategy (who controls more active timelines, the present). It is a material
  engine that sees tactics through search; weights are untuned beyond the self-play below.
* No transposition table, no quiescence search, no move-ordering history: depth is bought with beams.
* The first `step()` call can take a few milliseconds longer (cloning the game, the generator's first frame).

## 7. Tuning and tools

`tools/ai_bench` (`-DFDCHESS_BUILD_AIBENCH=ON`, Release):

```bash
cmake -S . -B build-bench -DCMAKE_BUILD_TYPE=Release -DFDCHESS_BUILD_GAME=OFF -DFDCHESS_BUILD_TESTS=OFF -DFDCHESS_BUILD_AIBENCH=ON
cmake --build build-bench -j --target ai_bench
./build-bench/tools/ai_bench/ai_bench speed [--mode standard]
./build-bench/tools/ai_bench/ai_bench selfplay --games 18 --turns 40 --a hard --b easy [--mode ID] [--game K]
```

`speed` prints time per decision and nodes/s per level on an opening and a midgame position; `selfplay` plays A against B
(colours alternate, the nine modes cycle), adjudicates unfinished games by material (> 300 cp) and prints the score;
`--game K` replays one game and prints every turn in notation. To tune: change `EvalWeights` or `levelParams()`, run
`selfplay` with a few dozen games per pair (scores of 18 games are noisy: +-2.5 points), and keep a change only if it holds
up across pairs. The tests (`tests/ai_test.cpp`) pin the behaviour that must not regress: legal turns in every mode, mates
(single board, two boards, a jump into the past), not hanging the queen, determinism, the step budget.
