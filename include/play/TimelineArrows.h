#pragma once
#include <tuple>
#include <vector>
#include <raylib.h>
#include "chess.h"
#include "Render/Motion.h"
#include "play/BoardLayout.h"
#include "play/BoardStyle.h"

namespace play {

/// One link between two boards: the thread of a timeline (a board to its successor) or a branch (the board a timeline
/// was forked from to the first board of the new timeline).
struct Arrow {
  Rect from, to;          // the boards it connects
  int timeline = 0;       // identity: the destination board
  int halfTurn = 0;
  bool branch = false;
  bool byWhite = true;    // branches: the player who created the timeline
  bool inactive = false;  // the destination timeline is inactive
};

/// All links of a game: the threads of every timeline, then the branches.
std::vector<Arrow> timelineArrows(const Chess::IGame& game);

/// Draws the links behind the boards in the connector style of the board view; links that appear after the first call
/// draw themselves in progressively.
class TimelineArrows {
public:
  void update(float dt);
  void set(const std::vector<Arrow>& arrows);
  /// New input: links that are still drawing in jump to complete.
  void finish() { _active.clear(); }
  /// In world space (inside BeginMode2D).
  void draw(const BoardStyle& style, float zoom) const;

private:
  struct Key {
    bool branch;
    int timeline, halfTurn;
    bool operator<(const Key& o) const { return std::tie(branch, timeline, halfTurn) < std::tie(o.branch, o.timeline, o.halfTurn); }
  };
  struct Anim { Key key; UI::Motion::Tween t; };
  struct Line { Vector2 start, end; bool byWhite, inactive; Key key; };

  std::vector<Line> _lines;
  std::vector<Anim> _active; // only unfinished ones; absent == fully drawn
  std::vector<Key> _known, _scratch; // sorted keys seen at the last set()
  bool _seeded = false;

  float progressOf(const Key& key) const;
};

} // namespace play
