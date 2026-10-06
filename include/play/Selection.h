#pragma once
#include <optional>
#include <vector>
#include "chess.h"

namespace play {

/// What a click asks the game screen to do.
struct Intent {
  enum class Kind { None, Select, Move, Clear, Promote };
  Kind kind = Kind::None;
  Chess::Core::Coord from{};  // Select: the piece that was picked up
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
  ///   - anywhere else, or once the game is over: None
  Intent click(std::optional<Chess::Core::Coord> square, const Chess::IGame& game);

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
