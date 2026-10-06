#pragma once
#include <tuple>
#include <vector>
#include <raylib.h>
#include "chess.h"
#include "Render/Motion.h"
#include "play/BoardLayout.h"

namespace play {

/// One arrow between two boards: dashed within a timeline (a board to its successor), curved for a branch (the board
/// a timeline was forked from to the first board of the new timeline).
struct Arrow {
  Rect from, to;          // the boards it connects
  int timeline = 0;       // identity: the destination board
  int halfTurn = 0;
  bool branch = false;
};

/// All arrows of a game: the progression arrows of every timeline, then the branch arrows.
std::vector<Arrow> timelineArrows(const Chess::IGame& game);

/// Draws the arrows behind the boards; arrows that appear after the first call draw themselves in progressively.
class TimelineArrows {
public:
  void update(float dt);
  void set(const std::vector<Arrow>& arrows);
  /// New input: arrows that are still drawing in jump to complete.
  void finish() { _active.clear(); }
  /// In world space (inside BeginMode2D).
  void draw() const;

private:
  struct Key {
    bool branch;
    int timeline, halfTurn;
    bool operator<(const Key& o) const { return std::tie(branch, timeline, halfTurn) < std::tie(o.branch, o.timeline, o.halfTurn); }
  };
  struct Anim { Key key; UI::Motion::Tween t; };
  struct Line { Vector2 start, end; Color color; float thickness; Key key; };

  std::vector<Line> _lines;
  std::vector<Anim> _active; // only unfinished ones; absent == fully drawn
  std::vector<Key> _known, _scratch; // sorted keys seen at the last set()
  bool _seeded = false;
  float _dashOffset = 0.0f, _pulsePhase = 0.0f;

  float progressOf(const Key& key) const;
  void drawCurved(const Line& line, float progress) const;
  void drawDashed(const Line& line, float progress) const;
};

} // namespace play
