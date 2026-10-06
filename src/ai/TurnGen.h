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
//   * a child that completes the turn (canSubmit) is a leaf: advance() returns Leaf with game() standing in the finished,
//     not yet submitted position; the next advance() undoes it and goes on.
//
// Moves of different boards are chosen in a fixed board order, so no turn is produced twice in another move order.
// Leaves come out best-first along the first frames, so cutting the enumeration short loses the least.
// Not complete (beam, no moves on optional boards): ai::Search falls back to TurnSearch when it yields nothing.

#include "ai/Eval.h"
#include "chess.h"

#include <memory>
#include <vector>

namespace Chess::ai {

struct GenParams {
  int beam = 6;       ///< children kept per frame at depth 0
  int deepBeam = 3;   ///< children kept per frame at depth >= 1
  int maxTravel = 2;  ///< time jumps among them
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
    int taken = 0, travel = 0;
  };

  std::unique_ptr<IGame> _game;
  PieceColor _mover;
  GenParams _params;
  std::vector<Frame> _stack;
  bool _started = false;
  bool _undoLeaf = false;

  void generateMoves(Frame& f);
  /** Work of one move tried: a node costs more in positions with many timelines (copies, threat tests); at least 1. */
  long long cost() const { return 1 + _game->timeLineCount() / 3; }
};

} // namespace Chess::ai
