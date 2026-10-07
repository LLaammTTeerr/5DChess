#pragma once
#include <string>
#include <vector>
#include "chess.h"

namespace play {

/// Where a board stands in the turn being played.
enum class RowState {
  MustMove, // the side to move must still move on it before it can submit
  Optional, // it may move on it, but does not have to
  Moved,    // it was moved on this turn (or another move of the turn landed on it: both timelines advanced)
  Waiting,  // the latest board of an active timeline that is ahead: it is the opponent's to move on, nothing to do
};

/// One board of this turn. The rows are the boards as they stood when the turn began, so a row keeps its place while it is played.
struct ChecklistRow {
  int timeline = 0, halfTurn = 0;
  RowState state = RowState::Waiting;
  std::string label;  // "L+1 · T5w"
  std::string detail; // Moved: the move ("e2-e4", "Ng1 -> L-1 · T3w f3", "queen arrived from L-2"); otherwise empty
  bool inactive = false; // an inactive timeline (it only has optional moves)
};

/// One move of the opponent's last turn: clicking it shows its board and flashes its squares.
struct LastMove {
  Chess::Core::Coord from, to;
  std::string text; // "L+3  Qd1-b3"
};

enum class SubmitState {
  Ready,       // the turn can be handed in
  BoardsLeft,  // boards must still be moved on
  MoveFirst,   // nothing was moved yet and no board is left to move on (cannot normally happen)
  KingExposed, // every board is done but a king could be captured: change a move
};

struct TurnChecklist {
  std::vector<ChecklistRow> rows; // Mandatory boards, then Optional, then Waiting; newest timeline (highest id) first within each
  int done = 0, total = 0;        // "3 / 5 boards": mandatory boards moved of those the turn began with (no mandatory board: the optional ones)
  int boardsLeft = 0;             // mandatory boards still to move
  SubmitState submit = SubmitState::MoveFirst;
  bool lastByWhite = true;        // who played `last`
  std::string lastLabel;          // "T5w": the half-turn the opponent's turn began at (empty: no turn played yet)
  std::vector<LastMove> last;     // the opponent's last submitted turn (once the game is over: the turn that decided it)
  bool over = false;              // the game is decided: no rows, nothing to submit
};

/// The checklist of the side to move: pure, from what the engine already knows (mandatory and moveable boards, the pending moves, the history).
/// Once the game is over there are no rows, only the last turn.
TurnChecklist turnChecklist(const Chess::IGame& game);

/// A move in readable form, as it was played on `game` (its boards still hold the position before the move): "e2-e4", "Ng1xf3",
/// "e7-e8=Q", and for a move to another board "Ng1 -> L-1 · T3w f3" (ASCII: the UI fonts have no arrows).
std::string moveText(const Chess::IGame& game, const Chess::Core::PlayedMove& played);

/// "Submit: ready" / "Submit: locked - 2 boards left".
std::string submitLine(const TurnChecklist& checklist);

} // namespace play
