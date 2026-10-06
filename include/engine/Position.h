#pragma once

// Position: a pointer-free, value-comparable snapshot of a game between turns, and its text format (".5dp").
//
//   5dchess-position 1
//   title: Standard
//   size: 8
//   rules: double-step castling
//   to-move: white
//   L0 T0w: rnbkqbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBKQBNR
//
// Orientation. A board is N x N with N = `size`. The engine addresses a square as (x, y): x is the file (0 = left, the
// "a" file) and y the rank (0 = White's back rank, White's pawns advance towards y = N-1). A board line lists the rows
// from the TOP (y = N-1) down to the bottom (y = 0), separated by '/', each row from x = 0 to x = N-1, so the text is
// the picture a player sees with White at the bottom. In "rnbkqbnr/.../RNBKQBNR" the king is on x = 3 and the queen on
// x = 4, as in StandardGame.
//
// Rows are FEN-like: an upper-case letter is a White piece, lower-case a Black one (K king, Q queen, R rook, B bishop,
// N knight, P pawn), a number is that many empty squares. A piece is "unmoved" (it may still castle or double-step)
// unless it is followed by '*', which marks a piece that has already moved.
//
// Header lines (before the first board line; '#' starts a comment line, blank lines are ignored):
//   title:    free text shown in menus (optional)
//   size:     N, 1..16
//   rules:    any of  double-step  castling  (or "none")
//   to-move:  white | black
//   present:  half-turn of the present; optional; must equal the lowest half-turn among the latest boards of the
//             ACTIVE timelines (the default; its parity must agree with to-move)
// Board lines:  L<timeline> T<turn><w|b>: <rows>
//   half-turn = 2 * turn + (b ? 1 : 0). A timeline's boards must have consecutive half-turns; the first one may start
//   later than T0w (the "Fragment" mode starts L0 at T0b). Several lines of the same timeline give its history.
//   "L-1 T2w" is timeline -1, turn 2, White to move.
// Timeline lines:  L<timeline> parent: L<id>
//   marks a timeline a player branched off (as opposed to the timelines the game started with).
//
// Validation, limits and en passant (derived from the previous board of a timeline, so keep it) are described in
// docs/POSITIONS.md.
//
// Position is plain data: parsePosition(writePosition(p)) == p, and Position::fromGame / Position::makeGame convert
// from / to IGame. Not captured: the pending (unsubmitted) moves of a turn and the game result; snapshot between turns.

#include "chess.h"

#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace Chess::Core {

struct PieceCell {
  PieceType type = PieceType::Pawn;
  PieceColor color = PieceColor::PIECEWHITE;
  bool moved = false;
  bool operator==(const PieceCell&) const = default;
};

struct BoardData {
  int halfTurn = 0;
  /** size*size cells, index y * size + x (so the first row is White's back rank). */
  std::vector<std::optional<PieceCell>> cells;
  bool operator==(const BoardData&) const = default;
};

struct TimelineData {
  int id = 0;
  /** Set for a timeline a player branched off. */
  std::optional<int> parent;
  /** Consecutive half-turns, oldest first, never empty. */
  std::vector<BoardData> boards;
  bool operator==(const TimelineData&) const = default;
};

struct Position {
  std::string title;
  int size = 8;
  bool doubleStep = true; ///< pawns may advance two squares from their first move
  bool castling = true;
  PieceColor toMove = PieceColor::PIECEWHITE;
  int present = 0;
  std::vector<TimelineData> timelines; ///< ascending id

  bool operator==(const Position&) const = default;

  /** Snapshot of `game` (which should have no pending moves). */
  static Position fromGame(const IGame& game, std::string title = {});
  /** A fresh game in this position. */
  std::shared_ptr<IGame> makeGame() const;
};

struct ParseError : std::runtime_error {
  using std::runtime_error::runtime_error;
};

/** Parses the text format; throws ParseError ("line N: ...") on malformed input. */
Position parsePosition(std::string_view text);
std::string writePosition(const Position& position);

/** Reads and parses a file; throws ParseError (also when it cannot be read). */
Position loadPositionFile(const std::string& path);

} // namespace Chess::Core
