#pragma once
#include <string>
#include <vector>
#include "chess.h"

namespace play {

/// "L0", "L+1", "L-2": the timeline label (ASCII minus; the UI fonts have no U+2212).
std::string timelineLabel(int id);
/// "T3w" / "T3b": the board label (turns count from 1; w = White to move there).
std::string boardLabel(int halfTurn);

/// What a board means for the player this turn.
enum class BoardRole {
  Past,      // not the latest board of its timeline (history)
  Optional,  // the side to move may move on it, but does not have to
  Mandatory, // the side to move must move on it before the turn can be submitted
};

struct BoardInfo {
  int timeline = 0, halfTurn = 0;
  BoardRole role = BoardRole::Past;
  bool inactive = false;    // lies on an inactive timeline (only optional moves; drawn dimmed and desaturated)
  bool whiteToMove = true;  // White is to move on this board (even half-turn)
};

struct TimelineInfo {
  int id = 0;
  bool active = true;
  bool created = false;     // branched off by a player (an original timeline otherwise)
  bool byWhite = true;      // who created it (positive ids are White's); meaningless for originals
  int parent = 0;           // the timeline it branched from, and the half-turn of the board it was forked from
  int forkHalfTurn = 0;
  int firstHalfTurn = 0, lastHalfTurn = 0;
};

/// A piece of one board that could capture a king on another (or the same) board.
struct CheckLine {
  Chess::Core::Coord attacker, king;
};

/// A move between two different boards, shown as an arc: the pending turn's moves and those of the last submitted turn.
struct JumpInfo {
  Chess::Core::Move move;
  Chess::Piece piece;
  bool pending = false;
};

/// A snapshot of everything about the multiverse the board view shows that the rules engine answers (and that is not
/// free to ask every frame): board roles, timeline activity and ancestry, checking attacks, jumps. Pure data, rebuilt
/// by PlayScreen whenever the game's stateVersion() changes; `boards` has the order of BoardLayout::boards().
struct MultiverseView {
  std::vector<BoardInfo> boards;       // ascending (timeline, half-turn)
  std::vector<TimelineInfo> timelines; // ascending id
  std::vector<CheckLine> checks;       // attackers of the side to move's kings; empty when not in check
  std::vector<JumpInfo> jumps;
  int presentHalfTurn = 0;             // the present including the pending moves (IGame::bufferHalfTurn)
  int firstHalfTurn = 0, lastHalfTurn = 0; // columns that hold boards

  static MultiverseView build(const Chess::IGame& game);

  const TimelineInfo* timeline(int id) const;
  const BoardInfo* board(int timeline, int halfTurn) const;
};

} // namespace play
