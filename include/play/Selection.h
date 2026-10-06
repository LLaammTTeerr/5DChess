#pragma once
#include <optional>
#include <vector>
#include "chess.h"

namespace play {

/// What a click asks the game screen to do.
struct Intent {
  enum class Kind { None, Select, Move, Clear };
  Kind kind = Kind::None;
  Chess::Core::Coord from{};  // Select: the piece that was picked up
  Chess::Core::Move move{};   // Move: the move to make
};

/// The player's in-progress move: Idle, or PieceSelected{from, its legal moves}. A pure state machine over board
/// squares; it never touches the game, it only reads it. The screen performs the Intent it returns.
class Selection {
public:
  /// A click on `square` (nullopt: the click was not on a board).
  ///   - on a legal target of the selected piece: Move (the Queen when the move promotes), back to Idle
  ///   - on the selected piece again: Clear
  ///   - on a piece of the side to move on a board it can move on: Select (also switches from another piece)
  ///   - anywhere else, or once the game is over: None
  Intent click(std::optional<Chess::Core::Coord> square, const Chess::IGame& game);

  void clear();
  bool active() const { return _from.has_value(); }
  const std::optional<Chess::Core::Coord>& from() const { return _from; }
  /// Legal moves of the selected piece (a promotion appears once per piece choice).
  const std::vector<Chess::Core::Move>& moves() const { return _moves; }
  /// The squares those moves lead to, each once, in move order.
  const std::vector<Chess::Core::Coord>& targets() const { return _targets; }

private:
  std::optional<Chess::Core::Coord> _from;
  std::vector<Chess::Core::Move> _moves;
  std::vector<Chess::Core::Coord> _targets;
};

} // namespace play
