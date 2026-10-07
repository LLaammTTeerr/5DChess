#include "play/MultiverseView.h"
#include <algorithm>

namespace play {

using Chess::Core::Coord;

std::string timelineLabel(int id) { return id > 0 ? "L+" + std::to_string(id) : "L" + std::to_string(id); }

std::string boardLabel(int halfTurn) { return "T" + std::to_string(halfTurn / 2 + 1) + (halfTurn % 2 == 0 ? "w" : "b"); }

MultiverseView MultiverseView::build(const Chess::IGame& game) {
  MultiverseView view;
  view.presentHalfTurn = game.bufferHalfTurn();

  // Boards the side to move must / may move on (the game answers both with a few searches: asked once per state)
  std::vector<std::pair<int, int>> mandatory, moveable;
  for (const auto& b : game.mandatoryBoards()) mandatory.push_back({b->timeLineId(), b->halfTurnNumber()});
  for (const auto& b : game.getMoveableBoards()) moveable.push_back({b->timeLineId(), b->halfTurnNumber()});
  const bool ongoing = game.result() == Chess::GameResult::Ongoing;
  auto has = [](const std::vector<std::pair<int, int>>& v, int l, int t) {
    return std::find(v.begin(), v.end(), std::make_pair(l, t)) != v.end();
  };

  bool first = true;
  for (const auto& timeLine : game.getTimeLines()) {
    const auto boards = timeLine->getBoards();
    if (boards.empty()) continue;
    TimelineInfo info;
    info.id = timeLine->ID();
    info.active = game.isTimeLineActive(info.id);
    info.created = timeLine->hasParent();
    info.byWhite = info.id > 0;
    if (info.created) {
      info.parent = timeLine->parentId();
      info.forkHalfTurn = timeLine->forkAt();
    }
    info.firstHalfTurn = boards.front()->halfTurnNumber();
    info.lastHalfTurn = boards.back()->halfTurnNumber();
    view.timelines.push_back(info);
    if (first) {
      view.firstHalfTurn = info.firstHalfTurn;
      view.lastHalfTurn = info.lastHalfTurn;
    }
    view.firstHalfTurn = std::min(view.firstHalfTurn, info.firstHalfTurn);
    view.lastHalfTurn = std::max(view.lastHalfTurn, info.lastHalfTurn);
    first = false;

    for (const auto& board : boards) {
      BoardInfo b;
      b.timeline = info.id;
      b.halfTurn = board->halfTurnNumber();
      b.inactive = !info.active;
      b.whiteToMove = b.halfTurn % 2 == 0;
      if (ongoing && has(mandatory, b.timeline, b.halfTurn)) b.role = BoardRole::Mandatory;
      else if (ongoing && has(moveable, b.timeline, b.halfTurn)) b.role = BoardRole::Optional;
      view.boards.push_back(b);
    }
  }
  std::sort(view.boards.begin(), view.boards.end(), [](const BoardInfo& a, const BoardInfo& b) {
    return a.timeline != b.timeline ? a.timeline < b.timeline : a.halfTurn < b.halfTurn;
  });
  std::sort(view.timelines.begin(), view.timelines.end(), [](const TimelineInfo& a, const TimelineInfo& b) { return a.id < b.id; });

  if (ongoing)
    for (const auto& threat : game.checkingAttacks()) view.checks.push_back({threat.attacker.coord(), threat.king.coord()});

  auto addJumps = [&](const std::vector<Chess::Core::PlayedMove>& moves, bool pending) {
    for (const auto& played : moves) {
      const Coord& from = played.move.from;
      const Coord& to = played.move.to;
      if (from.l == to.l && from.t == to.t) continue;
      if (!game.boardExists(from)) continue;
      const auto piece = game.board(from.l, from.t).at(Chess::Position2D(from.x, from.y));
      if (piece) view.jumps.push_back({played.move, *piece, pending});
    }
  };
  if (!game.history().empty()) addJumps(game.history().back().moves, false);
  addJumps(game.pendingMoves(), true);
  return view;
}

const TimelineInfo* MultiverseView::timeline(int id) const {
  const auto it = std::lower_bound(timelines.begin(), timelines.end(), id,
                                   [](const TimelineInfo& t, int value) { return t.id < value; });
  return it != timelines.end() && it->id == id ? &*it : nullptr;
}

const BoardInfo* MultiverseView::board(int timeline, int halfTurn) const {
  const auto it = std::lower_bound(boards.begin(), boards.end(), std::make_pair(timeline, halfTurn),
                                   [](const BoardInfo& b, const std::pair<int, int>& key) {
                                     return b.timeline != key.first ? b.timeline < key.first : b.halfTurn < key.second;
                                   });
  return it != boards.end() && it->timeline == timeline && it->halfTurn == halfTurn ? &*it : nullptr;
}

} // namespace play
