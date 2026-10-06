#pragma once
#include <optional>
#include <string>
#include <utility>
#include <vector>
#include <raylib.h>
#include "Render/Motion.h"
#include "play/BoardLayout.h"

namespace play {

/// Identity of a board: (timeline id, half-turn). Everything that animates is keyed by this, never by object.
using BoardKey = std::pair<int, int>;
inline BoardKey keyOf(const Chess::Core::Coord& c) { return {c.l, c.t}; }

/// A piece travelling between squares. Visual only: the game already holds the finished move, and the destination
/// piece is hidden on its board until the flight lands.
struct FlightSpec {
  std::string piece;      // texture key, e.g. "white_pawn"
  std::string victim;     // captured piece (empty if none)
  BoardKey from{0, 0};    // board holding the source square (== to for same-board moves)
  BoardKey to{0, 0};      // the newly created board holding the moved piece
  int fromX = 0, fromY = 0, toX = 0, toY = 0;
  int dim = 8;
};

/// All motion of the board view that is not the camera: pieces flying to their squares, new boards growing in, the
/// picked-up piece lifting and the legal-target dots popping in, and the hover tint fading. State only; the
/// BoardRenderer draws it.
class MoveAnimator {
public:
  MoveAnimator();

  void update(float dt, const std::optional<Chess::Core::Coord>& hover);

  /// Call when the layout was rebuilt: boards that did not exist before grow in (never on the first call).
  void syncBoards(const BoardLayout& layout);
  void startFlight(const FlightSpec& spec);
  /// New input: running flights and grow-ins jump to their end.
  void finish();

  /// A piece was picked up (lifts) and its targets are shown (dots pop in, rippling outwards); nullopt: nothing selected.
  void select(const std::optional<Chess::Core::Coord>& from, const std::vector<Chess::Core::Coord>& targets, int dim);

  struct Flight {
    FlightSpec spec;
    Vector2 from{}, to{};
    float size = 0.0f;
    float arcHeight = 0.0f;
    UI::Motion::Tween move, victimFade;
  };
  const std::vector<Flight>& flights() const { return _flights; }
  /// 1 for a board that is fully there, 0..1 while it grows in.
  float enterProgress(BoardKey key) const;
  float lift() const { return _lift.value; }
  /// Scale of the legal-target dot `i` (pop-in with a small overshoot).
  float dotScale(size_t i) const;

  struct Hover { BoardKey key{0, 0}; int x = -1, y = -1; float alpha = 0.0f; };
  const Hover& hoverNow() const { return _hoverCur; }
  const Hover& hoverFading() const { return _hoverOld; }

private:
  struct Enter { BoardKey key; UI::Motion::Tween t; };
  std::vector<Flight> _flights;
  std::vector<Enter> _enters;
  std::vector<BoardKey> _known, _scratch; // sorted
  bool _seeded = false;

  std::optional<Chess::Core::Coord> _selected;
  UI::Motion::Spring _lift;
  std::vector<float> _dotDelay;
  float _dotClock = 0.0f;

  Hover _hoverCur, _hoverOld;
};

} // namespace play
