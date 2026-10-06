#pragma once

// The AI opponent's search (engine side only; see docs/AI.md). Resumable, single-threaded, deterministic given a seed:
//
//   ai::Search search(game, {ai::Level::Normal, seed});
//   while (search.step(500) == ai::Search::Status::Running) { /* draw a frame */ }
//   if (search.hasTurn()) for (const auto& m : search.bestTurn()) game.makeMove(m);   // then game.submitTurn()
//
// The search works on its own clones of `game`; the game may change (or die) after the constructor returns.

#include "chess.h"

#include <cstdint>
#include <memory>
#include <vector>

namespace Chess::ai {

enum class Level { Easy, Normal, Hard };

struct Options {
  Level level = Level::Normal;
  std::uint64_t seed = 1;
  /** Test/tuning knobs; 0 means "use the level's value" (see levelParams in src/ai/Search.cpp and docs/AI.md). */
  long long maxNodes = 0;
};

struct Progress {
  long long nodes = 0;          ///< nodes used so far (one node = one move tried, plus the result-search chunks)
  int candidatesTotal = 0;      ///< root turns generated in the running iteration
  int candidatesDone = 0;       ///< of those, how many are evaluated
  int depth = 0;                ///< turns searched by the last completed iteration (0 until the first one is done)
  int bestScore = 0;            ///< centipawns for the side to move after that iteration; ~MateScore when a mate is proven
  bool mateFound = false;       ///< a turn that leaves the opponent without a legal turn was found (proven by the engine: a mate in one)
  bool usedFallback = false;    ///< the turn comes from the engine's exhaustive search (TurnSearch), not from the AI's generator
  double fraction = 0;          ///< 0..1 estimate of the budget used (for a progress bar)
};

constexpr int MateScore = 100000;

class Search {
public:
  enum class Status { Running, Done };

  explicit Search(const IGame& game, Options options = {});
  ~Search();
  Search(const Search&) = delete;
  Search& operator=(const Search&) = delete;

  /** Continue for at most about `nodeBudget` nodes (a step may overshoot by the slice it is in: one generator move or proof slice, see docs/AI.md). Once Done, stays Done. */
  Status step(int nodeBudget);
  Status status() const;

  /**
   * True once Done if a legal turn exists. False means the side to move has no legal turn (the game is over: mate or
   * stalemate; the caller's game should already say so), or the search is not Done yet.
   */
  bool hasTurn() const;

  /**
   * The moves to make, in order, with IGame::makeMove(Core::Move) before submitTurn(): a complete legal turn (verified with
   * canSubmit() on a clone). If the game already had pending moves, they are kept and these continue them. Empty unless
   * Done and hasTurn().
   */
  std::vector<Core::Move> bestTurn() const;

  Progress progress() const;

private:
  struct Impl;
  std::unique_ptr<Impl> _impl;
};

} // namespace Chess::ai
