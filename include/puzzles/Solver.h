#pragma once
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>
#include "chess.h"
#include "puzzles/Puzzle.h"

// The exhaustive proofs behind the puzzles. They use only the official-rules engine (IGame, TurnSearch through
// IGame::resolveResult), never the beam-search AI: a puzzle's claim "mate in N" is a statement about all legal turns.
//
//   forEachTurn      every legal turn of the side to move (a set of moves that can be submitted), each once
//   matingTurns      those after which the opponent is checkmated
//   DefenceProver    "after this turn, every defence of the opponent loses to a mate in 1" (resumable, a defence per step)
//   validate         everything docs/PUZZLES.md promises about a puzzle file
//
// A turn is a sequence of moves; the same set of moves played in another order counts once. At most one move is made from
// each tip, so a turn has at most as many moves as there are boards the mover can move on; the enumeration is therefore
// complete, not sampled. It is exponential in that number: puzzle positions keep to a few pieces and tips.
namespace puzzles {

/// Resumable enumeration of the legal turns of the side to move (the engine behind forEachTurn). next() looks for the following
/// turn for at most `nodeBudget` moves tried, so a caller on a frame loop can stop after a few milliseconds and go on the next frame.
/// A turn is identified by the position it leads to (the same set of moves in another order is one turn).
class TurnEnumerator {
public:
  enum class Status { Turn, Running, Done, Limit };

  /// `prune = false` turns off the one shortcut (a state in which a king can already be captured is abandoned) for cross-checks.
  /// `maxNodes`: the whole enumeration gives up (Limit) after that many moves tried.
  explicit TurnEnumerator(const Chess::IGame& game, bool prune = true, long long maxNodes = 50'000'000);
  ~TurnEnumerator();
  TurnEnumerator(const TurnEnumerator&) = delete;
  TurnEnumerator& operator=(const TurnEnumerator&) = delete;

  /// Turn: `turn` holds the moves and pending() the game in which they are pending (canSubmit() holds). Running: the budget is used up,
  /// call again. Done: no more turns. Limit: maxNodes was reached.
  Status next(long long nodeBudget, Turn& turn);
  /// Valid after next() returned Turn, until the following call.
  const Chess::IGame& pending() const;
  long long nodes() const;

private:
  struct Impl;
  std::unique_ptr<Impl> _impl;
};

/// Called for every complete turn with its moves and a game (a clone of the one given) in which they are pending and
/// canSubmit() holds. Return false to stop. forEachTurn returns false only when `maxNodes` (moves tried) ran out first.
using TurnVisitor = std::function<bool(const Turn& turn, const Chess::IGame& pending)>;
bool forEachTurn(const Chess::IGame& game, const TurnVisitor& visit, long long maxNodes = 50'000'000, bool prune = true);

/// A clone of `game` with `turn` made and submitted and its legal-turn proof resolved (so result() says whether the
/// opponent is mated or stalemated). nullptr when a move of the turn is not offered or the turn cannot be submitted.
/// Throws std::runtime_error when the proof does not finish within `resolveNodes`.
std::unique_ptr<Chess::IGame> submitted(const Chess::IGame& game, const Turn& turn, long long resolveNodes = 50'000'000);

/// Does the (already complete) pending turn of `pending` checkmate the opponent? Same answer as
/// submitted(...)->result() == win for the mover, but skips the proof unless the opponent is in check (a mate needs one).
bool mates(const Chess::IGame& pending, long long resolveNodes = 50'000'000);

/// All mating turns of the side to move; `complete` (optional) is false when a limit stopped the enumeration. `stopAfter`
/// stops after that many (the first turn found is what the UI's hint uses).
std::vector<Turn> matingTurns(const Chess::IGame& game, bool* complete = nullptr, size_t stopAfter = SIZE_MAX);
std::optional<Turn> findMate(const Chess::IGame& game);

/// Proves that after the turn that led to `game` (the opponent to move, game still Ongoing) every legal turn of the opponent
/// allows White a mating turn. Resumable down to a few moves tried: step(ms) works for about that long (at least one unit of work: a few
/// moves of the enumeration, one mate test, a slice of a legal-turn proof) and returns, so a frame loop stays smooth.
/// The first defence that escapes is kept in refutation(). step() may throw std::runtime_error when a legal-turn proof runs out of budget.
class DefenceProver {
public:
  enum class Status { Running, Proven, Refuted };

  explicit DefenceProver(const Chess::IGame& game);
  ~DefenceProver();
  DefenceProver(const DefenceProver&) = delete;
  DefenceProver& operator=(const DefenceProver&) = delete;

  Status step(double milliseconds);
  Status status() const;
  /// Known once the enumeration of the defences is done (enumerated() true); done() of them have been checked.
  bool enumerated() const;
  size_t defenceCount() const;
  size_t done() const;
  /// 0..1: how much of the proof is behind us (the enumeration counts as the first tenth).
  double fraction() const;
  const std::vector<Turn>& defences() const;
  /// Valid once Refuted (empty when there was no defence at all): a defence after which White has no mate in 1.
  const Turn& refutation() const;

private:
  struct Impl;
  std::unique_ptr<Impl> _impl;
};

struct Report {
  bool ok = false;
  std::vector<std::string> errors;
  /// Mate in 1: the number of mating first turns. Mate in 2: the number of first turns after which every defence loses.
  size_t winningFirstTurns = 0;
  size_t firstTurns = 0;          ///< legal first turns of White
  size_t defences = 0;            ///< Black's legal replies after the stored first turn (mate in 2)
  size_t travelWins = 0;          ///< winning first turns that include a move to another board
  size_t branchingWins = 0;       ///< winning first turns that create a timeline
  int timelines = 0;              ///< timelines of the starting position
  double seconds = 0;
  std::vector<Turn> winners;      ///< the winning first turns (for the tool's listing)
};

/// Checks a puzzle against the engine, exhaustively:
///  - the position is a legal start: White to move, the game is not decided, White has a legal turn;
///  - mate in 1: the stored solution is a legal mating turn; no other claim needed (any mating turn wins);
///  - mate in 2: White has no mate in 1; the stored first turn is legal and every defence loses (DefenceProver), the stored
///    reply is one of the defences, and the stored last turn mates after it;
///  - tier 2 (time travel): the position has two or more timelines and every winning first turn includes a move to another board;
///  - tier 3 "branching" is a property reported (branchingWins), not required.
Report validate(const Puzzle& puzzle);

/// One line per puzzle for tools and logs.
std::string describe(const Puzzle& puzzle, const Report& report);

} // namespace puzzles
