#pragma once

// TurnGen: the resumable generator of CANDIDATE TURNS used by ai::Search (internal; see docs/AI.md, "Turn generation").
//
// A turn is a set of moves, one on every mandatory board (plus jumps that clear the obligation). The generator builds turns
// move by move on its own clone of the game, as an explicit depth-first search with a beam:
//
//   * a frame belongs to one mandatory board (the one with the lowest timeline id that still has to be moved on);
//   * its moves are the pseudo-legal moves of every piece on that board; each is tried once (1 node): the move is made,
//     discarded when a king of the mover can now be captured (permanent, see docs/SEARCH.md F2) and otherwise scored with
//     the static evaluation of the position after it;
//   * the best `beam` survivors become children, in score order, at most `maxTravel` of them time jumps;
//     A child that yields no turn (every continuation leaves a king capturable) does not use up a beam slot; the generator
//     explores at most `maxFailures` such dead ends in all (then it reports Done), so they cost a bounded amount of work;
//   * a child that completes the turn (canSubmit) is a leaf: advance() returns Leaf with game() standing in the finished,
//     not yet submitted position; the next advance() undoes it and goes on.
//
// Moves of different boards are chosen in a fixed board order; the few turns that still come out in two orders (a lateral time
// jump listed in the first frame and in its own board's frame) are filtered by comparing the sorted move sets.
// Leaves come out best-first along the first frames, so cutting the enumeration short loses the least.
// Not complete (beam; optional boards only in rescue mode): ai::Search retries in rescue mode, then falls back to TurnSearch.

#include "ai/Eval.h"
#include "chess.h"

#include <array>
#include <memory>
#include <set>
#include <vector>

namespace Chess::ai {

constexpr int NoLimit = 1 << 20;

struct GenParams {
  int beam = 6;       ///< children kept per frame at depth 0
  int deepBeam = 3;   ///< children kept per frame at depth >= 1 (a child counts only when it yields a turn)
  int maxTravel = 2;  ///< time jumps among them (NoLimit = all)
  /** Rescue mode (used when the normal generator finds no turn at all): moves on optional boards are candidates too, so that a
   *  quiet move of a board the mover does not have to move on (e.g. a jump that answers a check) is found. */
  bool optionalBoards = false;
  /** Children that yield no turn (dead ends) the whole generator may explore before it gives up; bounds the work in positions
   *  where almost every move leaves a king capturable. */
  int maxFailures = 64;
  /** Squeeze: once the generator itself has spent this many nodes (0 = never) it narrows to beam 1 and tolerates only a few
   *  more dead ends, so that it ends soon with whatever it has. The search sets it on the first pass, where the node cap does
   *  not interrupt generation before a first turn exists. Counted in the generator's own work: independent of step slicing. */
  long long squeezeAfter = 0;
  /** Moves after which the moved piece attacks a king of the opponent on its board (a mate threat the static score cannot
   *  see) are ordered before all others and are exempt from the time-jump limit. Same-board attacks only: discovered and
   *  cross-board checks are not detected. */
  bool checksFirst = true;
  EvalWeights weights;
};

class TurnGen {
public:
  enum class Result { Leaf, Done, Pause };

  TurnGen(std::unique_ptr<IGame> game, GenParams params)
      : _game(std::move(game)), _mover(_game->getCurrentTurnColor()), _params(params) {}

  /** The position the generator stands in; after Leaf it holds the complete turn (pendingMoves()). */
  IGame& game() { return *_game; }
  const IGame& game() const { return *_game; }
  PieceColor mover() const { return _mover; }

  /** Continue until the next leaf, exhaustion, or until `budget` (decremented by cost() per move tried, so a call may overshoot it by one move's cost) is used up. */
  Result advance(long long& budget);

private:
  struct Cand {
    Core::Move move;
    int score;
    bool check;
  };
  struct Frame {
    bool generated = false, sorted = false;
    std::vector<Core::Move> moves;
    std::vector<Cand> cands;
    std::size_t scan = 0, next = 0;
    int taken = 0;       ///< children that yielded at least one turn
    int travel = 0;
    bool yielded = false;///< the child being explored has produced a turn
  };

  std::unique_ptr<IGame> _game;
  PieceColor _mover;
  GenParams _params;
  std::vector<Frame> _stack;
  bool _started = false;
  bool _undoLeaf = false;
  int _failures = 0;
  long long _spent = 0;
  bool _squeezed = false;
  std::set<std::vector<std::array<int, 9>>> _seen; ///< move sets of the leaves returned so far

  void generateMoves(Frame& f);
  void spend();
  std::vector<std::array<int, 9>> turnKey() const;
  /** Work of one move tried: a node costs more in positions with many timelines and pieces (copies, threat tests, evaluation: all scan every tip); at least 1. */
  long long cost() const { return _cost; }
  long long _cost = 1;
};

} // namespace Chess::ai
