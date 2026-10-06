#include "play/BoardLayout.h"
#include <algorithm>
#include <cmath>

namespace play {

namespace {
bool before(const BoardLayout::Slot& a, const BoardLayout::Slot& b) {
  return a.timeline != b.timeline ? a.timeline < b.timeline : a.halfTurn < b.halfTurn;
}
} // namespace

Rect BoardLayout::boardRect(int timeline, int halfTurn) {
  return {static_cast<float>(halfTurn) * kPitch, static_cast<float>(timeline) * kPitch, kBoardSize, kBoardSize};
}

Rect BoardLayout::squareRect(const Rect& board, int dim, int x, int y) {
  const float sw = board.w / dim, sh = board.h / dim;
  return {board.x + x * sw, board.y + (dim - 1 - y) * sh, sw, sh};
}

bool BoardLayout::sync(const Chess::IGame& game) {
  if (_game == &game && _version == game.stateVersion()) return false;
  _game = &game;
  _version = game.stateVersion();
  _dim = game.dim();
  _boards.clear();
  for (const auto& timeLine : game.getTimeLines())
    for (const auto& board : timeLine->getBoards())
      _boards.push_back({board->timeLineId(), board->halfTurnNumber(),
                         boardRect(board->timeLineId(), board->halfTurnNumber())});
  std::sort(_boards.begin(), _boards.end(), before);
  _minTimeline = game.minTimeLineId();
  _maxTimeline = game.maxTimeLineId();
  _bounds = {};
  if (!_boards.empty()) {
    float x0 = _boards[0].rect.x, y0 = _boards[0].rect.y, x1 = x0, y1 = y0;
    for (const Slot& s : _boards) {
      x0 = std::min(x0, s.rect.x);
      y0 = std::min(y0, s.rect.y);
      x1 = std::max(x1, s.rect.x + s.rect.w);
      y1 = std::max(y1, s.rect.y + s.rect.h);
    }
    _bounds = {x0, y0, x1 - x0, y1 - y0};
  }
  return true;
}

bool BoardLayout::contains(int timeline, int halfTurn) const {
  const Slot key{timeline, halfTurn, {}};
  const auto it = std::lower_bound(_boards.begin(), _boards.end(), key, before);
  return it != _boards.end() && it->timeline == timeline && it->halfTurn == halfTurn;
}

std::optional<Chess::Core::Coord> BoardLayout::hitTest(float worldX, float worldY) const {
  if (_boards.empty()) return std::nullopt;
  const int timeline = static_cast<int>(std::floor(worldY / kPitch));
  const int halfTurn = static_cast<int>(std::floor(worldX / kPitch));
  if (timeline < _minTimeline || timeline > _maxTimeline || !contains(timeline, halfTurn)) return std::nullopt;
  const Rect area = boardRect(timeline, halfTurn);
  if (!area.contains(worldX, worldY)) return std::nullopt;
  const int col = std::min(_dim - 1, static_cast<int>((worldX - area.x) / (area.w / _dim)));
  const int row = std::min(_dim - 1, static_cast<int>((worldY - area.y) / (area.h / _dim)));
  return Chess::Core::Coord{static_cast<int8_t>(col), static_cast<int8_t>(_dim - 1 - row), static_cast<int16_t>(halfTurn),
                            static_cast<int16_t>(timeline)};
}

} // namespace play
