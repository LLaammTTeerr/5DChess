#pragma once
#include <optional>
#include <vector>
#include "chess.h"

namespace play {

/// A world-space rectangle (no graphics dependency, so the layout can be unit tested).
struct Rect {
  float x = 0, y = 0, w = 0, h = 0;
  float centerX() const { return x + w / 2; }
  float centerY() const { return y + h / 2; }
  bool contains(float px, float py) const { return px >= x && px < x + w && py >= y && py < y + h; }
};

/// Where every board of a game sits in the world. A board is a grid cell: its column is the half-turn, its row the
/// timeline id, White's timelines (positive ids) above timeline 0 and Black's (negative ids) below it, as in the official game. boardRect() is the pure (timeline, half-turn) -> rectangle
/// function; the BoardLayout object adds what depends on the game: the list of existing boards, their bounds, and the
/// inverse mapping hitTest(). It is rebuilt only when the game's stateVersion() changes.
class BoardLayout {
public:
  static constexpr float kBoardSize = 250.0f; // world size of one board
  static constexpr float kSpacing = 60.0f;    // gap between neighbouring boards
  static constexpr float kPitch = kBoardSize + kSpacing;

  static Rect boardRect(int timeline, int halfTurn);
  /// The card a board is drawn on: the board plus a frame all round and a label strip below it. It fits in the gap
  /// between neighbouring boards; branch connectors and threads attach to its left and right edges.
  static constexpr float kCardPad = 8.0f, kCardFooter = 26.0f;
  static Rect cardRect(const Rect& board) {
    return {board.x - kCardPad, board.y - kCardPad, board.w + 2 * kCardPad, board.h + 2 * kCardPad + kCardFooter};
  }
  /// Square (file x, rank y) of a board of size `dim`: White (y = 0) at the bottom, file a on the left.
  static Rect squareRect(const Rect& board, int dim, int x, int y);

  struct Slot {
    int timeline = 0, halfTurn = 0;
    Rect rect;
  };

  /// Rebuilds from `game` unless it is the same game in the same state as at the last call. Returns true if it rebuilt.
  bool sync(const Chess::IGame& game);

  /// Every board of the game, timelines in ascending id order, each oldest board first.
  const std::vector<Slot>& boards() const { return _boards; }
  bool contains(int timeline, int halfTurn) const;
  /// Union of all board rectangles.
  const Rect& bounds() const { return _bounds; }
  int minTimeline() const { return _minTimeline; }
  int maxTimeline() const { return _maxTimeline; }
  int dim() const { return _dim; } // board size of the game; mainly for tests

  /// The square under a world point, if it lies on an existing board.
  std::optional<Chess::Core::Coord> hitTest(float worldX, float worldY) const;

private:
  std::vector<Slot> _boards; // sorted by (timeline, halfTurn)
  Rect _bounds;
  int _minTimeline = 0, _maxTimeline = 0, _dim = 8;
  const Chess::IGame* _game = nullptr;
  unsigned long long _version = 0;
};

} // namespace play
