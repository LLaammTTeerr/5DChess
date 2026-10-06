#include "ai/TurnGen.h"

#include <algorithm>

namespace Chess::ai {

namespace {

constexpr int CostDivisor = 16; // pieces on all tips per extra node of cost (calibrated with ai_bench big)

bool isTravel(const Core::Move& m) { return m.from.t != m.to.t or m.from.l != m.to.l; }

/** Does the piece on (x, y) of `board` attack a king of colour `victim` on that board (ordinary chess geometry)? */
bool attacksKing(const Board& board, int x, int y, PieceColor victim) {
  const Cell piece = board.cell(x, y);
  if (piece.empty()) return false;
  const int n = board.dim();
  auto isKing = [&](int tx, int ty) {
    if (tx < 0 or ty < 0 or tx >= n or ty >= n) return false;
    const Cell c = board.cell(tx, ty);
    return !c.empty() and c.type() == PieceType::King and c.color() == victim;
  };
  static const int knight[8][2] = {{1, 2}, {2, 1}, {-1, 2}, {-2, 1}, {1, -2}, {2, -1}, {-1, -2}, {-2, -1}};
  static const int dirs[8][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}, {1, 1}, {1, -1}, {-1, 1}, {-1, -1}};
  switch (piece.type()) {
    case PieceType::Knight:
      for (const auto& d : knight) if (isKing(x + d[0], y + d[1])) return true;
      return false;
    case PieceType::King:
      for (const auto& d : dirs) if (isKing(x + d[0], y + d[1])) return true;
      return false;
    case PieceType::Pawn: {
      const int dy = piece.color() == PieceColor::PIECEWHITE ? 1 : -1;
      return isKing(x - 1, y + dy) or isKing(x + 1, y + dy);
    }
    default: {
      const int from = piece.type() == PieceType::Bishop ? 4 : 0;
      const int to = piece.type() == PieceType::Rook ? 4 : 8;
      for (int i = from; i < to; ++i) {
        for (int tx = x + dirs[i][0], ty = y + dirs[i][1]; tx >= 0 and ty >= 0 and tx < n and ty < n; tx += dirs[i][0], ty += dirs[i][1]) {
          if (isKing(tx, ty)) return true;
          if (!board.cell(tx, ty).empty()) break;
        }
      }
      return false;
    }
  }
}

} // namespace

void TurnGen::generateMoves(Frame& f) {
  f.generated = true;
  _cost = 1 + positionLoad(*_game) / CostDivisor + _game->timeLineCount() / 3 + int(_game->mandatoryBoards().size());
  auto boards = _game->mandatoryBoards(); // (a copy: sorted below)
  if (boards.empty()) return;
  std::sort(boards.begin(), boards.end(), [](const auto& a, const auto& b) { return a->timeLineId() < b->timeLineId(); });
  const std::size_t mandatory = boards.size();
  if (_params.optionalBoards) {
    auto movable = _game->getMoveableBoards();
    for (const auto& b : movable) {
      const bool known = std::any_of(boards.begin(), boards.begin() + std::ptrdiff_t(mandatory), [&](const auto& m) { return m->timeLineId() == b->timeLineId(); });
      if (!known) boards.push_back(b);
    }
  }
  // Every move of the first board; from the other mandatory boards only time jumps, which can clear the obligation to move
  // on the whole present (a jump that creates a timeline moves the present back) and so are the way out of some checks.
  // Rescue mode adds every move of the optional boards.
  for (std::size_t i = 0; i < boards.size(); ++i) {
    const auto& board = boards[i];
    for (int y = 0; y < board->dim(); ++y) {
      for (int x = 0; x < board->dim(); ++x) {
        const Cell c = board->cell(x, y);
        if (c.empty() or c.color() != _mover) continue;
        const Core::Coord from{int8_t(x), int8_t(y), int16_t(board->halfTurnNumber()), int16_t(board->timeLineId())};
        for (const Core::Move& m : _game->legalMovesFrom(from)) {
          if (i == 0 or i >= mandatory or isTravel(m)) f.moves.push_back(m);
        }
      }
    }
  }
}

void TurnGen::spend() {
  _spent += cost();
  if (_params.squeezeAfter > 0 and _spent >= _params.squeezeAfter) {
    _params.squeezeAfter = 0;
    _squeezed = true;
    _params.beam = _params.deepBeam = 1;
    _params.maxFailures = std::min(_params.maxFailures, _failures + 2);
  }
}

std::vector<std::array<int, 9>> TurnGen::turnKey() const {
  std::vector<std::array<int, 9>> key;
  for (const auto& pm : _game->pendingMoves()) {
    const Core::Move& m = pm.move;
    key.push_back({m.from.x, m.from.y, m.from.t, m.from.l, m.to.x, m.to.y, m.to.t, m.to.l, int(m.promotion)});
  }
  std::sort(key.begin(), key.end());
  return key;
}

TurnGen::Result TurnGen::advance(long long& budget) {
  if (_undoLeaf) {
    _game->undo();
    _undoLeaf = false;
  }
  if (!_started) {
    _started = true;
    _stack.emplace_back();
  }
  while (true) {
    if (_stack.empty()) return Result::Done;
    if (budget <= 0) return Result::Pause;
    Frame& f = _stack.back();
    if (!f.generated) generateMoves(f);

    if (_squeezed and f.cands.size() >= 1) f.scan = f.moves.size(); // squeezed: one survivor per frame is enough
    if (f.scan < f.moves.size()) { // scoring phase: one node per move
      const Core::Move m = f.moves[f.scan++];
      budget -= cost();
      spend();
      _game->makeMove(m);
      if (!TurnSearch::kingCapturable(*_game, _mover)) {
        const bool check = _params.checksFirst and attacksKing(*_game->getNewBoard(), m.to.x, m.to.y, opposite(_mover));
        f.cands.push_back({m, evaluate(*_game, _mover, _params.weights), check});
      }
      _game->undo();
      continue;
    }
    if (!f.sorted) {
      f.sorted = true;
      std::stable_sort(f.cands.begin(), f.cands.end(), [](const Cand& a, const Cand& b) { return a.check != b.check ? a.check : a.score > b.score; });
    }
    const int beam = _stack.size() == 1 ? _params.beam : _params.deepBeam;
    bool descended = false;
    while (f.next < f.cands.size() and f.taken < beam and _failures < _params.maxFailures) {
      const Cand& cand = f.cands[f.next++];
      const Core::Move m = cand.move;
      if (isTravel(m) and !cand.check) {
        if (f.travel >= _params.maxTravel) continue;
        ++f.travel;
      }
      budget -= cost();
      spend();
      _game->makeMove(m);
      if (_game->canSubmit()) {
        if (!_seen.insert(turnKey()).second) { // the same set of moves in another order
          _game->undo();
          continue;
        }
        for (std::size_t i = 0; i + 1 < _stack.size(); ++i) _stack[i].yielded = true;
        ++f.taken;
        _undoLeaf = true;
        return Result::Leaf;
      }
      if (_game->mandatoryBoards().empty()) { // nothing left to move on yet not submittable: dead end
        _game->undo();
        ++_failures;
        continue;
      }
      _stack.emplace_back(); // the move stays made; undone when the child frame is exhausted
      descended = true;
      break;
    }
    if (descended) continue;
    _stack.pop_back();
    if (!_stack.empty()) {
      _game->undo();
      Frame& parent = _stack.back();
      if (parent.yielded) ++parent.taken; // a child counts against the beam only if it produced a turn
      else ++_failures;
      parent.yielded = false;
    }
  }
}

} // namespace Chess::ai
