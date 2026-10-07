#pragma once
#include <optional>
#include <vector>
#include "chess.h"

namespace play {

/// What a click asks the game screen to do.
struct Intent {
  enum class Kind { None, Select, Move, Clear, Promote, Rejected };
  /// Why a click on a piece did nothing (Rejected): the screen shows it as a shake, a flash and a line in the HUD.
  enum class Reason { None, NotYourPiece, HistoryBoard, OtherSidesBoard, ComputerThinking };
  Kind kind = Kind::None;
  Reason reason = Reason::None; // Rejected only
  Chess::Core::Coord from{};  // Select: the piece that was picked up; Rejected: the square that was clicked
  Chess::Core::Move move{};   // Move: the move to make; Promote: the move with the default piece (the player must still choose)
};

/// The player's in-progress move: Idle, PieceSelected{from, its legal moves}, or Promoting (a pawn target was clicked and
/// the piece it becomes is still to be chosen). A pure state machine over board
/// squares; it never touches the game, it only reads it. The screen performs the Intent it returns.
class Selection {
public:
  /// A click on `square` (nullopt: the click was not on a board).
  ///   - on a legal target of the selected piece: Move, back to Idle; when the move promotes: Promote (the selection stays
  ///     until choosePromotion())
  ///   - any click while Promoting that is not on the same promotion target cancels the choice (the piece stays selected)
  ///   - on the selected piece again: Clear
  ///   - on a piece of the side to move on a board it can move on: Select (also switches from another piece)
  ///   - on a piece that cannot be picked up: Rejected (NotYourPiece: it belongs to the other side; HistoryBoard: it is the side to
  ///     move's piece but its board is no longer the latest of its timeline; OtherSidesBoard: the board is the latest but it is the other
  ///     side's turn there); the selection stays as it was
  ///   - anywhere else (an empty square, off the boards), or once the game is over: None
  Intent click(std::optional<Chess::Core::Coord> square, const Chess::IGame& game);

  /// Can the side to move pick up the piece on `square`? (A piece of that side on a board it can still move on.)
  static bool canPickUp(const Chess::Core::Coord& square, const Chess::IGame& game);
  /// The Rejected reason for a click on `square` that cannot select it (None: empty square, no board, game over).
  static Intent::Reason rejection(const Chess::Core::Coord& square, const Chess::IGame& game);

  void clear();
  /// Promoting: the square the pawn would promote on, and its moves (one per piece: Queen, Rook, Bishop, Knight).
  const std::optional<Chess::Core::Coord>& promotionTarget() const { return _promoting; }
  std::vector<Chess::Core::Move> promotionChoices() const;
  /// The Move for the chosen piece (back to Idle); None unless Promoting.
  Intent choosePromotion(Chess::PieceType piece);
  void cancelPromotion() { _promoting.reset(); }
  bool active() const { return _from.has_value(); }
  const std::optional<Chess::Core::Coord>& from() const { return _from; }
  /// Legal moves of the selected piece (a promotion appears once per piece choice); mainly for tests.
  const std::vector<Chess::Core::Move>& moves() const { return _moves; }
  /// The squares those moves lead to, each once, in move order.
  const std::vector<Chess::Core::Coord>& targets() const { return _targets; }

private:
  std::optional<Chess::Core::Coord> _from, _promoting;
  std::vector<Chess::Core::Move> _moves;
  std::vector<Chess::Core::Coord> _targets;
};

} // namespace play
