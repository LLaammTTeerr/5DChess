#include "chess.h"

#ifdef NDEBUG
#error "chess.cpp must be built with assertions enabled (NDEBUG undefined)"
#endif

#include <stdexcept>
#include <iostream>
#include <climits>
#include <algorithm>
#include <cstdlib>

// using namespace Chess;
namespace Chess {
Vector4D::Vector4D(int x, int y, int z, int w) : _data({x, y, z, w}) {}

Piece::Piece(PieceColor color, std::shared_ptr<Board> board, Position2D position)
    : _color(color), _board(board), _position(position) {}

Board::Board(int N, int timeLineId, int halfTurnNumber) : _N(N), _halfTurnNumber(halfTurnNumber), _previousBoard(nullptr), _timeLineId(timeLineId) {
  _pieces.resize(N, std::vector<std::shared_ptr<Piece>>(N, nullptr));
}

void Board::placePiece(Position2D position, std::shared_ptr<Piece> piece) {
  assert(position.x() >= 0 && position.x() < _N);
  assert(position.y() >= 0 && position.y() < _N);
  if (piece != nullptr) {
    piece->setBoard(shared_from_this());
    piece->setPosition(position);
  }
  _pieces[position.x()][position.y()] = std::move(piece);
}

std::shared_ptr<Piece> Board::getPiece(Position2D position) const {
  assert(position.x() >= 0 && position.x() < _N);
  assert(position.y() >= 0 && position.y() < _N);
  return _pieces[position.x()][position.y()];
}

std::shared_ptr<Board> Board::createFork(int timeLineId) {
  std::shared_ptr<Board> forkedBoard = std::make_shared<Board>(_N, timeLineId);
  for (int x = 0; x < _N; ++x) {
    for (int y = 0; y < _N; ++y) {
      std::shared_ptr<Piece> piece = _pieces[x][y];
      if (piece != nullptr) {
        forkedBoard->placePiece(Position2D(x, y), std::shared_ptr<Piece>(piece->clone()));
      }
    }
  }
  forkedBoard->_previousBoard = shared_from_this();
  forkedBoard->_halfTurnNumber = _halfTurnNumber + 1;
  return forkedBoard;
}

TimeLine::TimeLine(int N, int IDX, int forkAt) : _N(N), _ID(IDX), _forkAt(forkAt), _parentId(NO_PARENT) {}

void TimeLine::pushBack(std::shared_ptr<Board> board) {
  _history.push_back(board);
}

std::vector<std::shared_ptr<Board>> IGame::getMoveableBoards(void) const {
  std::vector<std::shared_ptr<Board>> moveableBoards;
  for (const auto& [id, timeLine] : _timeLines) {
    if (timeLine->halfTurnNumber() % 2 == int(_currentTurnColor)) {
      moveableBoards.push_back(timeLine->back());
    }
  }
  return moveableBoards;
}

bool IGame::canMakeMoveFromBoard(std::shared_ptr<Board> board) const {
  if (!board) return false;
  auto it = _timeLines.find(board->timeLineId());
  if (it == _timeLines.end()) return false;
  return it->second->back() == board and board->halfTurnNumber() % 2 == int(_currentTurnColor);
}

bool IGame::isTimeLineActive(int id) const {
  if (id >= _origMin and id <= _origMax) return true;
  const int whiteCreated = std::max(0, maxTimeLineId() - _origMax);
  const int blackCreated = std::max(0, _origMin - minTimeLineId());
  // The n-th timeline created by a player is active iff the opponent has created at least n-1 timelines.
  if (id > _origMax) return id - _origMax <= blackCreated + 1;
  return _origMin - id <= whiteCreated + 1;
}

std::vector<int> IGame::activeTimeLineIds(void) const {
  std::vector<int> ids;
  for (const auto& kv : _timeLines) {
    if (isTimeLineActive(kv.first)) ids.push_back(kv.first);
  }
  return ids;
}

int IGame::bufferHalfTurn(void) const {
  int present = INT_MAX;
  for (const auto& kv : _timeLines) {
    if (isTimeLineActive(kv.first)) present = std::min(present, kv.second->halfTurnNumber());
  }
  return present;
}

std::vector<std::shared_ptr<Board>> IGame::mandatoryBoards(void) const {
  std::vector<std::shared_ptr<Board>> result;
  const int present = bufferHalfTurn();
  if (present % 2 != int(_currentTurnColor)) return result;
  for (const auto& kv : _timeLines) {
    if (kv.second->halfTurnNumber() == present and isTimeLineActive(kv.first)) result.push_back(kv.second->back());
  }
  return result;
}

bool IGame::boardExists(int timeLineID, int halfTurn) const {
  auto it = _timeLines.find(timeLineID);
  if (it == _timeLines.end()) return false;
  int pos = halfTurn - it->second->forkAt() - 1;
  return pos >= 0 && pos < static_cast<int>(it->second->size());
}

std::shared_ptr<Piece> IGame::_getPieceByVector4DFullTurn(Vector4D position, PieceColor mover) const {
  int x = position.x();
  int y = position.y();
  int halfTurn = 2 * position.z() + int(mover);
  int timeLineID = position.w();
  assert(hasTimeLine(timeLineID));
  std::shared_ptr<const Board> board = timeLine(timeLineID)->getBoardByHalfTurn(halfTurn);
  assert(board != nullptr);
  assert(x >= 0 && x < board->dim() && y >= 0 && y < board->dim());
  return board->getPiece(Position2D(x, y));
}

void IGame::undo(void) {
  assert(undoable());
  assert(_undoBuffer.size());
  // Get the last undo operation
  std::vector<int> lastUndo = _undoBuffer.back();
  _undoBuffer.pop_back();

  std::reverse(lastUndo.begin(), lastUndo.end());

  for (int timeLineID : lastUndo) {
    timeLine(timeLineID)->popBack();
    if (timeLine(timeLineID)->size() == 0) {
      _timeLines.erase(timeLineID);
    }
  }

  _currentTurnMoves.pop_back();
}

std::vector<Vector4D> genKnightMoves(const Vector4D& from) {
  std::vector<Vector4D> moves;
  moves.reserve(48);

  const int x = from.x();
  const int y = from.y();
  const int z = from.z();
  const int w = from.w();

  auto build = [&] (int axis2, int axis1, int s2, int s1) {
    int nx = x, ny = y, nz = z, nw = w;

    switch (axis2) {
      case 0: nx += 2 * s2; break;
      case 1: ny += 2 * s2; break;
      case 2: nz += 2 * s2; break;
      case 3: nw += 2 * s2; break;
    }

    switch (axis1) {
      case 0: nx += 1 * s1; break;
      case 1: ny += 1 * s1; break;
      case 2: nz += 1 * s1; break;
      case 3: nw += 1 * s1; break;
    }

    Vector4D to = Vector4D(nx, ny, nz, nw);

    moves.push_back(to);
  };

  for (int axis2 = 0; axis2 < 4; axis2 += 1) {
    for (int axis1 = 0; axis1 < 4; axis1 += 1) {
      if (axis1 == axis2) continue;
      for (int s2 : {-1, 1}) {
        for (int s1 : {-1, 1}) {
          build(axis2, axis1, s2, s1);
        }
      }
    }
  }

  return moves;
}

std::vector<SelectedPosition> IGame::_movesFor(PieceColor mover, SelectedPosition selected) const {
  std::shared_ptr<const Piece> piece = selected.board->getPiece(selected.position);
  Vector4D from = selected.toVector4D();
  int parity = int(mover);
  std::vector<SelectedPosition> moveablePositions;

  if (piece == nullptr) {
    throw std::runtime_error("No piece at selected position");
  }

  if (piece->color() != mover) {
    throw std::runtime_error("Piece color does not match current turn color");
  }

  if (piece->type() == PieceType::Rook) {
    for (int nx = from.x() + 1; nx < dim(); nx += 1) {
      std::shared_ptr<Piece> targetPiece = _getPieceByVector4DFullTurn({nx, from.y(), from.z(), from.w()}, mover);
      if (targetPiece != nullptr and targetPiece->color() == mover) {
        break;
      }
      moveablePositions.emplace_back(selected.board, Position2D(nx, from.y()));
      if (targetPiece != nullptr)
        break;
    }

    for (int nx = from.x() - 1; nx >= 0; nx -= 1) {
      std::shared_ptr<Piece> targetPiece = _getPieceByVector4DFullTurn({nx, from.y(), from.z(), from.w()}, mover);
      if (targetPiece != nullptr and targetPiece->color() == mover) {
        break;
      }
      moveablePositions.emplace_back(selected.board, Position2D(nx, from.y()));
      if (targetPiece != nullptr)
        break;
    }

    for (int ny = from.y() + 1; ny < dim(); ny += 1) {
      std::shared_ptr<Piece> targetPiece = _getPieceByVector4DFullTurn({from.x(), ny, from.z(), from.w()}, mover);
      if (targetPiece != nullptr and targetPiece->color() == mover) {
        break;
      }
      moveablePositions.emplace_back(selected.board, Position2D(from.x(), ny));
      if (targetPiece != nullptr)
        break;
    }

    for (int ny = from.y() - 1; ny >= 0; ny -= 1) {
      std::shared_ptr<Piece> targetPiece = _getPieceByVector4DFullTurn({from.x(), ny, from.z(), from.w()}, mover);
      if (targetPiece != nullptr and targetPiece->color() == mover) {
        break;
      }
      moveablePositions.emplace_back(selected.board, Position2D(from.x(), ny));
      if (targetPiece != nullptr)
        break;
    }

    for (int nz = from.z() - 1; nz >= 0; nz -= 1) {
      if (not boardExists(from.w(), 2 * nz + parity)) {
        break;
      }
      std::shared_ptr<Piece> targetPiece = _getPieceByVector4DFullTurn({from.x(), from.y(), nz, from.w()}, mover);
      if (targetPiece != nullptr and targetPiece->color() == mover) {
        break;
      }
      moveablePositions.emplace_back(getBoard(from.w(), 2 * nz + parity), Position2D(from.x(), from.y()));
      if (targetPiece != nullptr)
        break;
    }

    for (int nw = from.w() - 1; nw >= minTimeLineId(); nw -= 1) {
      if (not boardExists(nw, 2 * from.z() + parity)) {
        break;
      }
      std::shared_ptr<Piece> targetPiece = _getPieceByVector4DFullTurn({from.x(), from.y(), from.z(), nw}, mover);
      if (targetPiece != nullptr and targetPiece->color() == mover) {
        break;
      }
      moveablePositions.emplace_back(getBoard(nw, 2 * from.z() + parity), Position2D(from.x(), from.y()));
      if (targetPiece != nullptr)
        break;
    }

    for (int nw = from.w() + 1; nw <= maxTimeLineId(); nw += 1) {
      if (not boardExists(nw, 2 * from.z() + parity)) {
        break;
      }
      std::shared_ptr<Piece> targetPiece = _getPieceByVector4DFullTurn({from.x(), from.y(), from.z(), nw}, mover);
      if (targetPiece != nullptr and targetPiece->color() == mover) {
        break;
      }
      moveablePositions.emplace_back(getBoard(nw, 2 * from.z() + parity), Position2D(from.x(), from.y()));
      if (targetPiece != nullptr)
        break;
    }
  }

  if (piece->type() == PieceType::Knight) {
    std::vector<Vector4D> knightMoves = genKnightMoves(from);

    for (const Vector4D& move : knightMoves) {
      if (move.x() >= 0 && move.x() < dim() && move.y() >= 0 && move.y() < dim()) {
        if (boardExists(move.w(), 2 * move.z() + parity)
            && (_getPieceByVector4DFullTurn(move, mover) == nullptr || _getPieceByVector4DFullTurn(move, mover)->color() != mover)) {
          moveablePositions.emplace_back(getBoard(move.w(), 2 * move.z() + parity), Position2D(move.x(), move.y()));
        }
      }
    }
  }

  if (piece->type() == PieceType::Bishop) {
    for (int sx : {-1, +1}) for (int sy : {-1, +1}) {
      for (int d = 1; d < dim(); d += 1) {
        int nx = from.x() + sx * d;
        int ny = from.y() + sy * d;
        if (nx < 0 || nx >= dim() || ny < 0 || ny >= dim()) break;
        std::shared_ptr<Piece> targetPiece = _getPieceByVector4DFullTurn({nx, ny, from.z(), from.w()}, mover);
        if (targetPiece != nullptr and targetPiece->color() == mover) {
          break;
        }
        moveablePositions.emplace_back(selected.board, Position2D(nx, ny));
        if (targetPiece != nullptr) {
          break;
        }
      }
    }

    for (int sx : {-1, +1}) {
      for (int d = 1; d < dim(); d += 1) {
        int nx = from.x() + sx * d;
        int nz = from.z() - d;
        if (nz < 0) break;
        if (nx < 0 || nx >= dim()) break;
        if (!boardExists(from.w(), 2 * nz + parity)) break;
        std::shared_ptr<Piece> targetPiece = _getPieceByVector4DFullTurn({nx, from.y(), nz, from.w()}, mover);
        if (targetPiece != nullptr and targetPiece->color() == mover) {
          break;
        }
        moveablePositions.emplace_back(getBoard(from.w(), 2 * nz + parity), Position2D(nx, from.y()));
        if (targetPiece != nullptr) {
          break;
        }
      }
    }

    for (int sy : {-1, +1}) {
      for (int d = 1; d < dim(); d += 1) {
        int ny = from.y() + sy * d;
        int nz = from.z() - d;
        if (nz < 0) break;
        if (ny < 0 || ny >= dim()) break;
        if (!boardExists(from.w(), 2 * nz + parity)) break;
        std::shared_ptr<Piece> targetPiece = _getPieceByVector4DFullTurn({from.x(), ny, nz, from.w()}, mover);
        if (targetPiece != nullptr and targetPiece->color() == mover) {
          break;
        }
        moveablePositions.emplace_back(getBoard(from.w(), 2 * nz + parity), Position2D(from.x(), ny));
        if (targetPiece != nullptr) {
          break;
        }
      }
    }

    for (int sx : {-1, +1}) for (int sw : {-1, +1}) {
      for (int d = 1; d < dim(); d += 1) {
        int nx = from.x() + sx * d;
        int nw = from.w() + sw * d;
        if (nx < 0 || nx >= dim()) break;
        if (!boardExists(nw, 2 * from.z() + parity)) break;
        std::shared_ptr<Piece> targetPiece = _getPieceByVector4DFullTurn({nx, from.y(), from.z(), nw}, mover);
        if (targetPiece != nullptr and targetPiece->color() == mover) {
          break;
        }
        moveablePositions.emplace_back(getBoard(nw, 2 * from.z() + parity), Position2D(nx, from.y()));
        if (targetPiece != nullptr) {
          break;
        }
      }
    }

    for (int sy : {-1, +1}) for (int sw : {-1, +1}) {
      for (int d = 1; d < dim(); d += 1) {
        int ny = from.y() + sy * d;
        int nw = from.w() + sw * d;
        if (ny < 0 || ny >= dim()) break;
        if (!boardExists(nw, 2 * from.z() + parity)) break;
        std::shared_ptr<Piece> targetPiece = _getPieceByVector4DFullTurn({from.x(), ny, from.z(), nw}, mover);
        if (targetPiece != nullptr and targetPiece->color() == mover) {
          break;
        }
        moveablePositions.emplace_back(getBoard(nw, 2 * from.z() + parity), Position2D(from.x(), ny));
        if (targetPiece != nullptr) {
          break;
        }
      }
    }

    for (int sw : {-1, +1}) {
      for (int d = 1; d <= selected.board->fullTurnNumber(); d += 1) {
        int nz = from.z() - d;
        int nw = from.w() + sw * d;
        if (!boardExists(nw, 2 * nz + parity)) break;
        std::shared_ptr<Piece> targetPiece = _getPieceByVector4DFullTurn({from.x(), from.y(), nz, nw}, mover);
        if (targetPiece != nullptr and targetPiece->color() == mover) {
          break;
        }
        moveablePositions.emplace_back(getBoard(nw, 2 * nz + parity), Position2D(from.x(), from.y()));
        if (targetPiece != nullptr) {
          break;
        }
      }
    }
  }

  if (piece->type() == PieceType::Queen) {
    for (int mask = 1; mask < (1 << 4); mask += 1) {
      #define ONBIT(n) ((mask) & (1 << (n)))
      int maxD = INT_MAX;
      if (ONBIT(0) || ONBIT(1)) {
        maxD = std::min(maxD, dim());
      }
      if (ONBIT(2)) {
        maxD = std::min(maxD, selected.board->fullTurnNumber() + 1);
      }
      if (ONBIT(3)) {
        maxD = std::min(maxD, maxTimeLineId() - minTimeLineId() + 1);
      }
      #undef ONBIT
      // Value-returning helper: the range-init temporary stays alive for the whole loop.
      auto signs = [mask](int bit) {
        return (mask & bit) ? std::vector<int>{-1, +1} : std::vector<int>{0};
      };
      for (int s0 : signs(1))
      for (int s1 : signs(2))
      for (int s2 : signs(4))
      for (int s3 : signs(8)) {
        for (int d = 1; d < maxD; d += 1) {
          int nx = from.x() + s0 * d;
          int ny = from.y() + s1 * d;
          int nz = from.z() + s2 * d;
          int nw = from.w() + s3 * d;

          if (nx < 0 || nx >= dim() || ny < 0 || ny >= dim()) break;
          if (!boardExists(nw, 2 * nz + parity)) break;
          std::shared_ptr<Piece> targetPiece = _getPieceByVector4DFullTurn({nx, ny, nz, nw}, mover);
          if (targetPiece != nullptr && targetPiece->color() == mover) break;

          std::shared_ptr<Board> targetBoard = getBoard(nw, 2 * nz + parity);
          moveablePositions.emplace_back(targetBoard, Position2D(nx, ny));
          if (targetPiece != nullptr) break;
        }
      }
    }
  }

  if (piece->type() == PieceType::King) {
    for (int dx = -1; dx <= +1; dx += 1)
    for (int dy = -1; dy <= +1; dy += 1)
    for (int dz = -1; dz <= 0; dz += 1)
    for (int dw = -1; dw <= +1; dw += 1) {
      if (dx == 0 && dy == 0 && dz == 0 && dw == 0) continue;
      Vector4D to = Vector4D(from.x() + dx, from.y() + dy, from.z() + dz, from.w() + dw);
      if (to.x() >= 0 && to.x() < dim() && to.y() >= 0 && to.y() < dim()) {
        if (boardExists(to.w(), 2 * to.z() + parity)) {
          std::shared_ptr<Piece> targetPiece = _getPieceByVector4DFullTurn(to, mover);
          if (targetPiece and targetPiece->color() == mover)
            continue;
          moveablePositions.emplace_back(getBoard(to.w(), 2 * to.z() + parity), Position2D(to.x(), to.y()));
        }
      }
    }
    // Castling (2D, same board): king and rook unmoved, everything between them empty, and neither the king's
    // square, the square it crosses nor the square it lands on attacked on this board.
    if (_rule.castling and piece->unmoved()) {
      const std::shared_ptr<Board>& b = selected.board;
      const int y = from.y();
      for (int dir : {-1, +1}) {
        int fx = from.x() + dir;
        while (fx >= 0 && fx < dim() && b->getPiece(Position2D(fx, y)) == nullptr) fx += dir;
        if (fx < 0 || fx >= dim() || std::abs(fx - from.x()) < 3) continue;
        std::shared_ptr<Piece> rook = b->getPiece(Position2D(fx, y));
        if (rook->type() != PieceType::Rook or rook->color() != mover or not rook->unmoved()) continue;
        const PieceColor enemy = opposite(mover);
        if (_attacked2D(b, Position2D(from.x(), y), enemy) or _attacked2D(b, Position2D(from.x() + dir, y), enemy)
            or _attacked2D(b, Position2D(from.x() + 2 * dir, y), enemy)) continue;
        moveablePositions.emplace_back(b, Position2D(from.x() + 2 * dir, y));
      }
    }
  }


  if (piece->type() == PieceType::Pawn) {
    const int d = mover == PieceColor::PIECEWHITE ? 1 : -1; // forward on the rank axis AND on the timeline axis
    const int tlId = selected.board->timeLineId();
    const int h = selected.board->halfTurnNumber();
    const int x = from.x();
    const int y = from.y();
    const std::shared_ptr<Board>& b = selected.board;
    const int ny = y + d;
    if (ny >= 0 && ny < dim()) {
      if (b->getPiece(Position2D(x, ny)) == nullptr) {
        moveablePositions.emplace_back(b, Position2D(x, ny));
        const int ny2 = y + 2 * d;
        if (_rule.pawnCanMakeTwoMoveOnFirstTurn and piece->unmoved() and ny2 >= 0 and ny2 < dim()
            and b->getPiece(Position2D(x, ny2)) == nullptr) {
          moveablePositions.emplace_back(b, Position2D(x, ny2));
        }
      }
      for (int dx : {-1, +1}) {
        const int nx = x + dx;
        if (nx < 0 || nx >= dim()) continue;
        std::shared_ptr<Piece> target = b->getPiece(Position2D(nx, ny));
        if (target != nullptr) {
          if (target->color() != mover) moveablePositions.emplace_back(b, Position2D(nx, ny));
          continue;
        }
        // En passant: an enemy pawn right beside us made the double step in the last half-turn of this timeline.
        const int startY = y + 2 * d;
        std::shared_ptr<Piece> beside = b->getPiece(Position2D(nx, y));
        if (beside == nullptr or beside->type() != PieceType::Pawn or beside->color() == mover) continue;
        if (startY < 0 || startY >= dim() or b->getPiece(Position2D(nx, startY)) != nullptr) continue;
        if (not boardExists(tlId, h - 1)) continue;
        std::shared_ptr<Board> previous = getBoard(tlId, h - 1);
        std::shared_ptr<Piece> before = previous->getPiece(Position2D(nx, startY));
        if (before != nullptr and before->type() == PieceType::Pawn and before->color() != mover and before->unmoved()
            and previous->getPiece(Position2D(nx, ny)) == nullptr and previous->getPiece(Position2D(nx, y)) == nullptr) {
          moveablePositions.emplace_back(b, Position2D(nx, ny));
        }
      }
    }
    // Timeline axis: one step "forward" (White towards higher IDs, Black towards lower) onto the same square.
    const int nid = tlId + d;
    if (boardExists(nid, h)) {
      std::shared_ptr<Board> side = getBoard(nid, h);
      if (side->getPiece(Position2D(x, y)) == nullptr) {
        moveablePositions.emplace_back(side, Position2D(x, y));
        if (_rule.pawnCanMakeTwoMoveOnFirstTurn and piece->unmoved() and boardExists(nid + d, h)) {
          std::shared_ptr<Board> side2 = getBoard(nid + d, h);
          if (side2->getPiece(Position2D(x, y)) == nullptr) moveablePositions.emplace_back(side2, Position2D(x, y));
        }
      }
    }
    // Timeline-axis capture: one step forward in the timeline and one full turn back or ahead in time.
    for (int dh : {-2, +2}) {
      if (!boardExists(nid, h + dh)) continue;
      std::shared_ptr<Board> diag = getBoard(nid, h + dh);
      std::shared_ptr<Piece> target = diag->getPiece(Position2D(x, y));
      if (target != nullptr and target->color() != mover) moveablePositions.emplace_back(diag, Position2D(x, y));
    }
  }
  return moveablePositions;
}

std::vector<SelectedPosition> IGame::getMoveablePositions(SelectedPosition selected) const {
  return _movesFor(_currentTurnColor, selected);
}

std::shared_ptr<Piece> makePiece(PieceType type, PieceColor color) {
  switch (type) {
    case PieceType::King: return std::make_shared<King>(color);
    case PieceType::Queen: return std::make_shared<Queen>(color);
    case PieceType::Rook: return std::make_shared<Rook>(color);
    case PieceType::Bishop: return std::make_shared<Bishop>(color);
    case PieceType::Knight: return std::make_shared<Knight>(color);
    case PieceType::Pawn: return std::make_shared<Pawn>(color);
  }
  return nullptr;
}

// Is `pos` attacked on this single 2D board by a piece of colour `by`? (Used for castling, which only looks at the
// board the king stands on.) The square itself may be empty or occupied.
bool IGame::_attacked2D(const std::shared_ptr<Board>& board, Position2D pos, PieceColor by) const {
  const int n = board->dim();
  auto at = [&](int x, int y) -> std::shared_ptr<Piece> {
    return (x < 0 || x >= n || y < 0 || y >= n) ? nullptr : board->getPiece(Position2D(x, y));
  };
  auto is = [&](const std::shared_ptr<Piece>& p, PieceType t) { return p != nullptr and p->color() == by and p->type() == t; };
  static const int knight[8][2] = {{1, 2}, {2, 1}, {-1, 2}, {-2, 1}, {1, -2}, {2, -1}, {-1, -2}, {-2, -1}};
  for (const auto& k : knight) {
    if (is(at(pos.x() + k[0], pos.y() + k[1]), PieceType::Knight)) return true;
  }
  const int pawnFrom = by == PieceColor::PIECEWHITE ? -1 : 1; // a white pawn attacks one rank up, so it stands one down
  for (int dx : {-1, 1}) {
    if (is(at(pos.x() + dx, pos.y() + pawnFrom), PieceType::Pawn)) return true;
  }
  for (int dx = -1; dx <= 1; ++dx) {
    for (int dy = -1; dy <= 1; ++dy) {
      if ((dx != 0 or dy != 0) and is(at(pos.x() + dx, pos.y() + dy), PieceType::King)) return true;
    }
  }
  for (int dx = -1; dx <= 1; ++dx) {
    for (int dy = -1; dy <= 1; ++dy) {
      if (dx == 0 and dy == 0) continue;
      const bool diagonal = dx != 0 and dy != 0;
      for (int x = pos.x() + dx, y = pos.y() + dy; x >= 0 && x < n && y >= 0 && y < n; x += dx, y += dy) {
        std::shared_ptr<Piece> p = board->getPiece(Position2D(x, y));
        if (p == nullptr) continue;
        if (p->color() == by and (p->type() == PieceType::Queen or p->type() == (diagonal ? PieceType::Bishop : PieceType::Rook)))
          return true;
        break;
      }
    }
  }
  return false;
}

void IGame::makeMove(Move move, PieceType promotion) {
  assert(_result == GameResult::Ongoing);
  assert(promotion != PieceType::King and promotion != PieceType::Pawn);
  _setupDone = true;
  std::vector<int> list;
  std::shared_ptr<Piece> piece = move.from.board->getPiece(move.from.position);
  assert(piece != nullptr);
  assert(piece->color() == _currentTurnColor);
  assert(canMakeMoveFromBoard(move.from.board));
  std::shared_ptr<Piece> moveToPiece = move.to.board->getPiece(move.to.position);
  // A legal game never lets a king be captured (canSubmit() refuses turns that leave one capturable).
  assert(moveToPiece == nullptr or moveToPiece->type() != PieceType::King);

  const bool sameBoard = move.to.board == move.from.board;
  const int dx = move.to.position.x() - move.from.position.x();
  const bool castle = sameBoard and piece->type() == PieceType::King and move.to.position.y() == move.from.position.y()
                      and std::abs(dx) == 2;
  const bool enPassant = sameBoard and piece->type() == PieceType::Pawn and dx != 0 and moveToPiece == nullptr;
  const int lastRank = _currentTurnColor == PieceColor::PIECEWHITE ? dim() - 1 : 0;
  const bool promotes = piece->type() == PieceType::Pawn and move.to.position.y() == lastRank;

  std::shared_ptr<Piece> arriving = promotes ? makePiece(promotion, _currentTurnColor) : piece->clone();
  arriving->setUnmoved(false);

  _currentTurnMoves.push_back(move);
  const int fromTimeLineId = move.from.board->timeLineId();
  // Boards are immutable once pushed to a timeline: finish building each new board before pushing it.
  std::shared_ptr<Board> newFromBoard = move.from.board->createFork(fromTimeLineId);
  newFromBoard->placePiece(move.from.position, nullptr);
  list.push_back(fromTimeLineId);
  if (sameBoard) {
    if (enPassant) newFromBoard->placePiece(Position2D(move.to.position.x(), move.from.position.y()), nullptr);
    if (castle) {
      const int dir = dx > 0 ? 1 : -1;
      int rx = move.from.position.x() + dir;
      while (newFromBoard->getPiece(Position2D(rx, move.from.position.y())) == nullptr) rx += dir;
      std::shared_ptr<Piece> rook = newFromBoard->getPiece(Position2D(rx, move.from.position.y()))->clone();
      assert(rook->type() == PieceType::Rook);
      rook->setUnmoved(false);
      newFromBoard->placePiece(Position2D(rx, move.from.position.y()), nullptr);
      newFromBoard->placePiece(Position2D(move.from.position.x() + dir, move.from.position.y()), rook);
    }
    newFromBoard->placePiece(move.to.position, arriving);
    timeLine(fromTimeLineId)->pushBack(newFromBoard);
    _undoBuffer.push_back(list);
    return;
  }
  timeLine(fromTimeLineId)->pushBack(newFromBoard);

  const int toBoardTimeLineId = move.to.board->timeLineId();
  std::shared_ptr<TimeLine> toTimeLine = move.to.board->halfTurnNumber() == timeLine(toBoardTimeLineId)->halfTurnNumber()
    ? timeLine(toBoardTimeLineId)
    : timeLine(toBoardTimeLineId)->createFork(allocateTimeLineId(_currentTurnColor), move.to.board->halfTurnNumber());

  if (toTimeLine->ID() != toBoardTimeLineId) {
    _addTimeLine(toTimeLine);
  }

  list.push_back(toTimeLine->ID());

  std::shared_ptr<Board> newToBoard = timeLine(toBoardTimeLineId)->getBoardByHalfTurn(move.to.board->halfTurnNumber())->createFork(toTimeLine->ID());
  newToBoard->placePiece(move.to.position, arriving);
  toTimeLine->pushBack(newToBoard);
  _undoBuffer.push_back(list);
}

int IGame::allocateTimeLineId(PieceColor mover) const {
  return mover == PieceColor::PIECEWHITE ? maxTimeLineId() + 1 : minTimeLineId() - 1;
}

IGame::IGame(const IGame& other)
  : _N(other._N),
    _presentHalfTurn(other._presentHalfTurn),
    _currentTurnMoves(other._currentTurnMoves),
    _currentTurnColor(other._currentTurnColor),
    _undoBuffer(other._undoBuffer),
    _rule(other._rule),
    _result(other._result),
    _turnSearchBudget(other._turnSearchBudget),
    _origMin(other._origMin),
    _origMax(other._origMax),
    _setupDone(other._setupDone) {
  for (const auto& [id, tl] : other._timeLines) {
    _timeLines.emplace(id, std::make_shared<TimeLine>(*tl)); // copies the board vector; Boards are shared (immutable)
  }
}

std::unique_ptr<IGame> IGame::clone(void) const {
  return std::unique_ptr<IGame>(new IGame(*this));
}

std::vector<Move> IGame::allPseudoLegalMoves(void) const {
  std::vector<Move> result;
  for (const auto& board : getMoveableBoards()) {
    for (int x = 0; x < board->dim(); ++x) {
      for (int y = 0; y < board->dim(); ++y) {
        auto piece = board->getPiece({x, y});
        if (!piece || piece->color() != _currentTurnColor) continue;
        SelectedPosition from(board, Position2D(x, y));
        for (const auto& to : getMoveablePositions(from)) {
          result.push_back(Move{from, to});
        }
      }
    }
  }
  return result;
}

std::vector<Threat> IGame::_threatsAgainst(PieceColor victim, bool firstOnly) const {
  std::vector<Threat> threats;
  const PieceColor enemy = opposite(victim);
  for (const auto& [id, line] : _timeLines) {
    if (line->halfTurnNumber() % 2 != int(enemy)) continue; // the enemy can only move on boards ending on its turn
    const std::shared_ptr<Board> board = line->back();
    for (int x = 0; x < board->dim(); ++x) {
      for (int y = 0; y < board->dim(); ++y) {
        auto piece = board->getPiece({x, y});
        if (!piece || piece->color() != enemy) continue;
        SelectedPosition from(board, Position2D(x, y));
        for (const auto& to : _movesFor(enemy, from)) {
          auto target = to.board->getPiece(to.position);
          if (target != nullptr and target->type() == PieceType::King and target->color() == victim) {
            threats.push_back(Threat{from, to});
            if (firstOnly) return threats;
          }
        }
      }
    }
  }
  return threats;
}

std::vector<Threat> IGame::threatsAgainst(PieceColor victim) const {
  return _threatsAgainst(victim, false);
}

bool IGame::canSubmit(void) const {
  if (_result != GameResult::Ongoing or _currentTurnMoves.empty()) return false;
  if (!mandatoryBoards().empty()) return false;
  return _threatsAgainst(_currentTurnColor, true).empty();
}

void IGame::_passMandatoryBoards(std::map<const Board*, std::shared_ptr<Board>>& passedFrom) {
  for (const auto& board : mandatoryBoards()) {
    std::shared_ptr<Board> passed = board->createFork(board->timeLineId());
    timeLine(board->timeLineId())->pushBack(passed);
    passedFrom[passed.get()] = board;
  }
}

std::vector<Threat> IGame::checkingAttacks(void) const {
  IGame simulation(*this);
  std::map<const Board*, std::shared_ptr<Board>> passedFrom;
  simulation._passMandatoryBoards(passedFrom);
  std::vector<Threat> threats = simulation._threatsAgainst(_currentTurnColor, false);
  auto real = [&](SelectedPosition& p) {
    auto it = passedFrom.find(p.board.get());
    if (it != passedFrom.end()) p.board = it->second;
  };
  for (auto& t : threats) {
    real(t.attacker);
    real(t.king);
  }
  return threats;
}

TurnSearch IGame::_search(int& nodes, int budget) {
  if (!_currentTurnMoves.empty() and canSubmit()) return TurnSearch::Found;
  // Pieces that can move, mandatory boards first: they are the ones every legal turn needs. Moves are generated
  // lazily per piece, since a legal turn is usually found among the first few.
  std::vector<SelectedPosition> sources;
  const auto mandatory = mandatoryBoards();
  auto collect = [&](const std::shared_ptr<Board>& board) {
    for (int x = 0; x < board->dim(); ++x) {
      for (int y = 0; y < board->dim(); ++y) {
        auto piece = board->getPiece({x, y});
        if (piece and piece->color() == _currentTurnColor) sources.emplace_back(board, Position2D(x, y));
      }
    }
  };
  for (const auto& board : mandatory) collect(board);
  for (const auto& board : getMoveableBoards()) {
    if (std::find(mandatory.begin(), mandatory.end(), board) == mandatory.end()) collect(board);
  }
  bool unknown = false;
  for (const SelectedPosition& from : sources) {
    for (const SelectedPosition& to : _movesFor(_currentTurnColor, from)) {
      if (nodes >= budget) return TurnSearch::Unknown;
      ++nodes;
      makeMove(Move{from, to});
      const TurnSearch r = _search(nodes, budget);
      undo();
      if (r == TurnSearch::Found) return r;
      if (r == TurnSearch::Unknown) unknown = true;
    }
  }
  return unknown ? TurnSearch::Unknown : TurnSearch::None;
}

TurnSearch IGame::findLegalTurn(int nodeBudget) const {
  IGame search(*this);
  search._result = GameResult::Ongoing;
  int nodes = 0;
  return search._search(nodes, nodeBudget);
}

void IGame::_evaluateResult(void) {
  assert(_currentTurnMoves.empty());
  const bool check = !checkingAttacks().empty();
  switch (findLegalTurn(_turnSearchBudget)) {
    case TurnSearch::Found:
    case TurnSearch::Unknown:
      _result = GameResult::Ongoing;
      break;
    case TurnSearch::None:
      if (!check) _result = GameResult::Draw;
      else _result = _currentTurnColor == PieceColor::PIECEWHITE ? GameResult::BlackWins : GameResult::WhiteWins;
      break;
  }
}

void IGame::submitTurn(void) {
  assert(canSubmit());
  _presentHalfTurn = bufferHalfTurn();
  _currentTurnMoves.clear();
  _currentTurnColor = opposite(_currentTurnColor);
  _undoBuffer.clear();
  _evaluateResult();
}

const std::string NameOfGame<StandardGame>::value = "Standard";
StandardGame::StandardGame(void) : IGame(Constant::BOARD_SIZE) {
  _addTimeLine(std::make_shared<TimeLine>(dim()));
  std::shared_ptr<Board> board = std::make_shared<Board>(dim(), 0);
  for (int i = 0; i < dim(); i += 1) {
    board->placePiece({i, 1}, std::make_shared<Pawn>(PieceColor::PIECEWHITE));
    board->placePiece({i, 6}, std::make_shared<Pawn>(PieceColor::PIECEBLACK));
  }
  board->placePiece({0, 0}, std::make_shared<Rook>(PieceColor::PIECEWHITE));
  board->placePiece({1, 0}, std::make_shared<Knight>(PieceColor::PIECEWHITE));
  board->placePiece({2, 0}, std::make_shared<Bishop>(PieceColor::PIECEWHITE));
  board->placePiece({3, 0}, std::make_shared<King>(PieceColor::PIECEWHITE));
  board->placePiece({4, 0}, std::make_shared<Queen>(PieceColor::PIECEWHITE));
  board->placePiece({5, 0}, std::make_shared<Bishop>(PieceColor::PIECEWHITE));
  board->placePiece({6, 0}, std::make_shared<Knight>(PieceColor::PIECEWHITE));
  board->placePiece({7, 0}, std::make_shared<Rook>(PieceColor::PIECEWHITE));

  board->placePiece({0, 7}, std::make_shared<Rook>(PieceColor::PIECEBLACK));
  board->placePiece({1, 7}, std::make_shared<Knight>(PieceColor::PIECEBLACK));
  board->placePiece({2, 7}, std::make_shared<Bishop>(PieceColor::PIECEBLACK));
  board->placePiece({3, 7}, std::make_shared<King>(PieceColor::PIECEBLACK));
  board->placePiece({4, 7}, std::make_shared<Queen>(PieceColor::PIECEBLACK));
  board->placePiece({5, 7}, std::make_shared<Bishop>(PieceColor::PIECEBLACK));
  board->placePiece({6, 7}, std::make_shared<Knight>(PieceColor::PIECEBLACK));
  board->placePiece({7, 7}, std::make_shared<Rook>(PieceColor::PIECEBLACK));
  _timeLines.at(0)->pushBack(board);
}

const std::string NameOfGame<CustomGameEmitBishop>::value = "Simplify - No Bishop";
CustomGameEmitBishop::CustomGameEmitBishop(void) : IGame(Constant::BOARD_SIZE_EMIT_BISHOP) {
  _rule.pawnCanMakeTwoMoveOnFirstTurn = false;
  _addTimeLine(std::make_shared<TimeLine>(dim()));
  std::shared_ptr<Board> board = std::make_shared<Board>(dim(), 0);
  for (int i = 0; i < dim(); i += 1) {
    board->placePiece({i, 1}, std::make_shared<Pawn>(PieceColor::PIECEWHITE));
    board->placePiece({i, 4}, std::make_shared<Pawn>(PieceColor::PIECEBLACK));
  }
  board->placePiece({0, 0}, std::make_shared<Rook>(PieceColor::PIECEWHITE));
  board->placePiece({1, 0}, std::make_shared<Bishop>(PieceColor::PIECEWHITE));
  board->placePiece({2, 0}, std::make_shared<Queen>(PieceColor::PIECEWHITE));
  board->placePiece({3, 0}, std::make_shared<King>(PieceColor::PIECEWHITE));
  board->placePiece({4, 0}, std::make_shared<Bishop>(PieceColor::PIECEWHITE));
  board->placePiece({5, 0}, std::make_shared<Rook>(PieceColor::PIECEWHITE));

  board->placePiece({0, 5}, std::make_shared<Rook>(PieceColor::PIECEBLACK));
  board->placePiece({1, 5}, std::make_shared<Bishop>(PieceColor::PIECEBLACK));
  board->placePiece({2, 5}, std::make_shared<Queen>(PieceColor::PIECEBLACK));
  board->placePiece({3, 5}, std::make_shared<King>(PieceColor::PIECEBLACK));
  board->placePiece({4, 5}, std::make_shared<Bishop>(PieceColor::PIECEBLACK));
  board->placePiece({5, 5}, std::make_shared<Rook>(PieceColor::PIECEBLACK));
  _timeLines.at(0)->pushBack(board);
}

const std::string NameOfGame<CustomGameEmitKnight>::value = "Simplify - No Knight";
CustomGameEmitKnight::CustomGameEmitKnight(void) : IGame(Constant::BOARD_SIZE_EMIT_KNIGHT) {
  _rule.pawnCanMakeTwoMoveOnFirstTurn = false;
  _addTimeLine(std::make_shared<TimeLine>(dim()));
  std::shared_ptr<Board> board = std::make_shared<Board>(dim(), 0);
  for (int i = 0; i < dim(); i += 1) {
    board->placePiece({i, 1}, std::make_shared<Pawn>(PieceColor::PIECEWHITE));
    board->placePiece({i, 4}, std::make_shared<Pawn>(PieceColor::PIECEBLACK));
  }
  board->placePiece({0, 0}, std::make_shared<Rook>(PieceColor::PIECEWHITE));
  board->placePiece({1, 0}, std::make_shared<Bishop>(PieceColor::PIECEWHITE));
  board->placePiece({2, 0}, std::make_shared<Queen>(PieceColor::PIECEWHITE));
  board->placePiece({3, 0}, std::make_shared<King>(PieceColor::PIECEWHITE));
  board->placePiece({4, 0}, std::make_shared<Bishop>(PieceColor::PIECEWHITE));
  board->placePiece({5, 0}, std::make_shared<Rook>(PieceColor::PIECEWHITE));

  board->placePiece({0, 5}, std::make_shared<Rook>(PieceColor::PIECEBLACK));
  board->placePiece({1, 5}, std::make_shared<Bishop>(PieceColor::PIECEBLACK));
  board->placePiece({2, 5}, std::make_shared<Queen>(PieceColor::PIECEBLACK));
  board->placePiece({3, 5}, std::make_shared<King>(PieceColor::PIECEBLACK));
  board->placePiece({4, 5}, std::make_shared<Bishop>(PieceColor::PIECEBLACK));
  board->placePiece({5, 5}, std::make_shared<Rook>(PieceColor::PIECEBLACK));
  _timeLines.at(0)->pushBack(board);
}

const std::string NameOfGame<CustomGameEmitQueen>::value = "Simplify - No Queen";
CustomGameEmitQueen::CustomGameEmitQueen(void) : IGame(Constant::BOARD_SIZE_EMIT_QUEEN) {
  _rule.pawnCanMakeTwoMoveOnFirstTurn = false;
  _addTimeLine(std::make_shared<TimeLine>(dim()));
  std::shared_ptr<Board> board = std::make_shared<Board>(dim(), 0);
  for (int i = 0; i < dim(); i += 1) {
    board->placePiece({i, 1}, std::make_shared<Pawn>(PieceColor::PIECEWHITE));
    board->placePiece({i, 5}, std::make_shared<Pawn>(PieceColor::PIECEBLACK));
  }
  board->placePiece({0, 0}, std::make_shared<Rook>(PieceColor::PIECEWHITE));
  board->placePiece({1, 0}, std::make_shared<Knight>(PieceColor::PIECEWHITE));
  board->placePiece({2, 0}, std::make_shared<Bishop>(PieceColor::PIECEWHITE));
  board->placePiece({3, 0}, std::make_shared<King>(PieceColor::PIECEWHITE));
  board->placePiece({4, 0}, std::make_shared<Bishop>(PieceColor::PIECEWHITE));
  board->placePiece({5, 0}, std::make_shared<Knight>(PieceColor::PIECEWHITE));
  board->placePiece({6, 0}, std::make_shared<Rook>(PieceColor::PIECEWHITE));

  board->placePiece({0, 6}, std::make_shared<Rook>(PieceColor::PIECEBLACK));
  board->placePiece({1, 6}, std::make_shared<Knight>(PieceColor::PIECEBLACK));
  board->placePiece({2, 6}, std::make_shared<Bishop>(PieceColor::PIECEBLACK));
  board->placePiece({3, 6}, std::make_shared<King>(PieceColor::PIECEBLACK));
  board->placePiece({4, 6}, std::make_shared<Bishop>(PieceColor::PIECEBLACK));
  board->placePiece({5, 6}, std::make_shared<Knight>(PieceColor::PIECEBLACK));
  board->placePiece({6, 6}, std::make_shared<Rook>(PieceColor::PIECEBLACK));
  _timeLines.at(0)->pushBack(board);
}

const std::string NameOfGame<CustomGameEmitRook>::value = "Simplify - No Rook";
CustomGameEmitRook::CustomGameEmitRook(void) : IGame(Constant::BOARD_SIZE_EMIT_ROOK) {
  _rule.pawnCanMakeTwoMoveOnFirstTurn = false;
  _addTimeLine(std::make_shared<TimeLine>(dim()));
  std::shared_ptr<Board> board = std::make_shared<Board>(dim(), 0);
  for (int i = 0; i < dim(); i += 1) {
    board->placePiece({i, 1}, std::make_shared<Pawn>(PieceColor::PIECEWHITE));
    board->placePiece({i, 4}, std::make_shared<Pawn>(PieceColor::PIECEBLACK));
  }
  board->placePiece({0, 0}, std::make_shared<Knight>(PieceColor::PIECEWHITE));
  board->placePiece({1, 0}, std::make_shared<Bishop>(PieceColor::PIECEWHITE));
  board->placePiece({2, 0}, std::make_shared<Queen>(PieceColor::PIECEWHITE));
  board->placePiece({3, 0}, std::make_shared<King>(PieceColor::PIECEWHITE));
  board->placePiece({4, 0}, std::make_shared<Bishop>(PieceColor::PIECEWHITE));
  board->placePiece({5, 0}, std::make_shared<Knight>(PieceColor::PIECEWHITE));

  board->placePiece({0, 5}, std::make_shared<Knight>(PieceColor::PIECEBLACK));
  board->placePiece({1, 5}, std::make_shared<Bishop>(PieceColor::PIECEBLACK));
  board->placePiece({2, 5}, std::make_shared<Queen>(PieceColor::PIECEBLACK));
  board->placePiece({3, 5}, std::make_shared<King>(PieceColor::PIECEBLACK));
  board->placePiece({4, 5}, std::make_shared<Bishop>(PieceColor::PIECEBLACK));
  board->placePiece({5, 5}, std::make_shared<Knight>(PieceColor::PIECEBLACK));
  _timeLines.at(0)->pushBack(board);
}

const std::string NameOfGame<CustomGameKVB>::value = "Simplify - Knight vs Bishop";
CustomGameKVB::CustomGameKVB(void) : IGame(Constant::BOARD_SIZE_K_VS_B) {
  _rule.pawnCanMakeTwoMoveOnFirstTurn = false;
  _addTimeLine(std::make_shared<TimeLine>(dim()));
  std::shared_ptr<Board> board = std::make_shared<Board>(dim(), 0);
  for (int i = 0; i < dim(); i += 1) {
    board->placePiece({i, 1}, std::make_shared<Pawn>(PieceColor::PIECEWHITE));
    board->placePiece({i, 4}, std::make_shared<Pawn>(PieceColor::PIECEBLACK));
  }
  board->placePiece({0, 0}, std::make_shared<Rook>(PieceColor::PIECEWHITE));
  board->placePiece({1, 0}, std::make_shared<Bishop>(PieceColor::PIECEWHITE));
  board->placePiece({2, 0}, std::make_shared<Queen>(PieceColor::PIECEWHITE));
  board->placePiece({3, 0}, std::make_shared<King>(PieceColor::PIECEWHITE));
  board->placePiece({4, 0}, std::make_shared<Bishop>(PieceColor::PIECEWHITE));
  board->placePiece({5, 0}, std::make_shared<Rook>(PieceColor::PIECEWHITE));

  board->placePiece({0, 5}, std::make_shared<Rook>(PieceColor::PIECEBLACK));
  board->placePiece({1, 5}, std::make_shared<Knight>(PieceColor::PIECEBLACK));
  board->placePiece({2, 5}, std::make_shared<Queen>(PieceColor::PIECEBLACK));
  board->placePiece({3, 5}, std::make_shared<King>(PieceColor::PIECEBLACK));
  board->placePiece({4, 5}, std::make_shared<Knight>(PieceColor::PIECEBLACK));
  board->placePiece({5, 5}, std::make_shared<Rook>(PieceColor::PIECEBLACK));
  _timeLines.at(0)->pushBack(board);
}

const std::string NameOfGame<MiscGameTimeLineInvasion>::value = "Misc - Time Line Invasion";
MiscGameTimeLineInvasion::MiscGameTimeLineInvasion(void) : IGame(Constant::BOARD_SIZE_TIME_LINE_INVASION) {
  _rule.pawnCanMakeTwoMoveOnFirstTurn = false;
  _rule.castling = false;
  _addTimeLine(std::make_shared<TimeLine>(dim(), 0));
  _addTimeLine(std::make_shared<TimeLine>(dim(), 1));
  std::shared_ptr<Board> board0 = std::make_shared<Board>(dim(), 0);
  std::shared_ptr<Board> board1 = std::make_shared<Board>(dim(), 1);

  board0->placePiece({0, dim() - 1}, std::make_shared<Knight>(PieceColor::PIECEBLACK));
  board0->placePiece({1, dim() - 1}, std::make_shared<Bishop>(PieceColor::PIECEBLACK));
  board0->placePiece({2, dim() - 1}, std::make_shared<King>(PieceColor::PIECEBLACK));
  board0->placePiece({3, dim() - 1}, std::make_shared<Rook>(PieceColor::PIECEBLACK));
  board0->placePiece({4, dim() - 1}, std::make_shared<Bishop>(PieceColor::PIECEBLACK));
  for (int i = 0; i < dim(); i += 1) {
    board0->placePiece({i, dim() - 2}, std::make_shared<Pawn>(PieceColor::PIECEBLACK));
    board0->placePiece({i, 0}, std::make_shared<Pawn>(PieceColor::PIECEWHITE));
  }

  board1->placePiece({0, 0}, std::make_shared<Knight>(PieceColor::PIECEWHITE));
  board1->placePiece({1, 0}, std::make_shared<Bishop>(PieceColor::PIECEWHITE));
  board1->placePiece({2, 0}, std::make_shared<King>(PieceColor::PIECEWHITE));
  board1->placePiece({3, 0}, std::make_shared<Rook>(PieceColor::PIECEWHITE));
  board1->placePiece({4, 0}, std::make_shared<Bishop>(PieceColor::PIECEWHITE));
  for (int i = 0; i < dim(); i += 1) {
    board1->placePiece({i, 1}, std::make_shared<Pawn>(PieceColor::PIECEWHITE));
    board1->placePiece({i, dim() - 1}, std::make_shared<Pawn>(PieceColor::PIECEBLACK));
  }
  _timeLines.at(0)->pushBack(board0);
  _timeLines.at(1)->pushBack(board1);
}

const std::string NameOfGame<MiscGameTimeLineBattle>::value = "Misc - Time Line Battle";
MiscGameTimeLineBattle::MiscGameTimeLineBattle(void) : IGame(Constant::BOARD_SIZE_TIME_LINE_BATTLE) {
  _rule.pawnCanMakeTwoMoveOnFirstTurn = false;
  _rule.castling = false;
  _addTimeLine(std::make_shared<TimeLine>(dim(), 0));
  _addTimeLine(std::make_shared<TimeLine>(dim(), 1));
  _addTimeLine(std::make_shared<TimeLine>(dim(), 2));
  std::shared_ptr<Board> board0 = std::make_shared<Board>(dim(), 0);
  std::shared_ptr<Board> board1 = std::make_shared<Board>(dim(), 1);
  std::shared_ptr<Board> board2 = std::make_shared<Board>(dim(), 2);

  board0->placePiece({0, dim() - 1}, std::make_shared<Rook>(PieceColor::PIECEBLACK));
  board0->placePiece({1, dim() - 1}, std::make_shared<Rook>(PieceColor::PIECEBLACK));
  board0->placePiece({2, dim() - 1}, std::make_shared<King>(PieceColor::PIECEBLACK));
  board0->placePiece({3, dim() - 1}, std::make_shared<Rook>(PieceColor::PIECEBLACK));
  board0->placePiece({4, dim() - 1}, std::make_shared<Rook>(PieceColor::PIECEBLACK));

  board0->placePiece({0, dim() - 2}, std::make_shared<Bishop>(PieceColor::PIECEBLACK));
  board0->placePiece({1, dim() - 2}, std::make_shared<Bishop>(PieceColor::PIECEBLACK));
  board0->placePiece({2, dim() - 2}, std::make_shared<Queen>(PieceColor::PIECEBLACK));
  board0->placePiece({3, dim() - 2}, std::make_shared<Bishop>(PieceColor::PIECEBLACK));
  board0->placePiece({4, dim() - 2}, std::make_shared<Bishop>(PieceColor::PIECEBLACK));

  for (int i = 0; i < dim(); i += 1) {
    board0->placePiece({i, dim() - 3}, std::make_shared<Pawn>(PieceColor::PIECEBLACK));
    board0->placePiece({i, 0}, std::make_shared<Pawn>(PieceColor::PIECEWHITE));
  }

  for (int i = 0; i < dim(); i += 1) {
    board1->placePiece({i, 0}, std::make_shared<Knight>(PieceColor::PIECEWHITE));
    board1->placePiece({i, 1}, std::make_shared<Pawn>(PieceColor::PIECEWHITE));

    board1->placePiece({i, dim() - 1}, std::make_shared<Knight>(PieceColor::PIECEBLACK));
    board1->placePiece({i, dim() - 2}, std::make_shared<Pawn>(PieceColor::PIECEBLACK));
  }

  board2->placePiece({0, 0}, std::make_shared<Rook>(PieceColor::PIECEWHITE));
  board2->placePiece({1, 0}, std::make_shared<Rook>(PieceColor::PIECEWHITE));
  board2->placePiece({2, 0}, std::make_shared<King>(PieceColor::PIECEWHITE));
  board2->placePiece({3, 0}, std::make_shared<Rook>(PieceColor::PIECEWHITE));
  board2->placePiece({4, 0}, std::make_shared<Rook>(PieceColor::PIECEWHITE));

  board2->placePiece({0, 1}, std::make_shared<Bishop>(PieceColor::PIECEWHITE));
  board2->placePiece({1, 1}, std::make_shared<Bishop>(PieceColor::PIECEWHITE));
  board2->placePiece({2, 1}, std::make_shared<Queen>(PieceColor::PIECEWHITE));
  board2->placePiece({3, 1}, std::make_shared<Bishop>(PieceColor::PIECEWHITE));
  board2->placePiece({4, 1}, std::make_shared<Bishop>(PieceColor::PIECEWHITE));

  for (int i = 0; i < dim(); i += 1) {
    board2->placePiece({i, 2}, std::make_shared<Pawn>(PieceColor::PIECEWHITE));
    board2->placePiece({i, dim() - 1}, std::make_shared<Pawn>(PieceColor::PIECEBLACK));
  }

  _timeLines.at(0)->pushBack(board0);
  _timeLines.at(1)->pushBack(board1);
  _timeLines.at(2)->pushBack(board2);
}

const std::string NameOfGame<MiscGameTimeLineFragment>::value = "Misc - Time Line Fragment";
MiscGameTimeLineFragment::MiscGameTimeLineFragment(void) : IGame(Constant::BOARD_SIZE_TIME_LINE_FRAGMENT) {
  _rule.pawnCanMakeTwoMoveOnFirstTurn = false;
  _rule.castling = false;
  _addTimeLine(std::make_shared<TimeLine>(dim(), 0, 0));
  _addTimeLine(std::make_shared<TimeLine>(dim(), 1));
  std::shared_ptr<Board> board0 = std::make_shared<Board>(dim(), 0, 1);
  std::shared_ptr<Board> board1 = std::make_shared<Board>(dim(), 1, 0);

  board0->placePiece({0, dim() - 1}, std::make_shared<King>(PieceColor::PIECEBLACK));
  board0->placePiece({1, dim() - 1}, std::make_shared<Pawn>(PieceColor::PIECEBLACK));
  board0->placePiece({2, dim() - 1}, std::make_shared<Pawn>(PieceColor::PIECEBLACK));
  board0->placePiece({3, dim() - 1}, std::make_shared<Pawn>(PieceColor::PIECEBLACK));
  board0->placePiece({0, 0}, std::make_shared<Knight>(PieceColor::PIECEWHITE));
  board0->placePiece({1, 0}, std::make_shared<Bishop>(PieceColor::PIECEWHITE));
  board0->placePiece({2, 0}, std::make_shared<Rook>(PieceColor::PIECEWHITE));
  board0->placePiece({3, 0}, std::make_shared<Knight>(PieceColor::PIECEWHITE));

  board1->placePiece({0, 0}, std::make_shared<King>(PieceColor::PIECEWHITE));
  board1->placePiece({1, 0}, std::make_shared<Pawn>(PieceColor::PIECEWHITE));
  board1->placePiece({2, 0}, std::make_shared<Pawn>(PieceColor::PIECEWHITE));
  board1->placePiece({3, 0}, std::make_shared<Pawn>(PieceColor::PIECEWHITE));
  board1->placePiece({0, dim() - 1}, std::make_shared<Knight>(PieceColor::PIECEBLACK));
  board1->placePiece({1, dim() - 1}, std::make_shared<Bishop>(PieceColor::PIECEBLACK));
  board1->placePiece({2, dim() - 1}, std::make_shared<Rook>(PieceColor::PIECEBLACK));
  board1->placePiece({3, dim() - 1}, std::make_shared<Knight>(PieceColor::PIECEBLACK));

  _timeLines.at(0)->pushBack(board0);
  _timeLines.at(1)->pushBack(board1);
}

const int Constant::BOARD_SIZE = 8;
const int Constant::BOARD_SIZE_EMIT_BISHOP = 6;
const int Constant::BOARD_SIZE_EMIT_KNIGHT = 6;
const int Constant::BOARD_SIZE_EMIT_QUEEN = 7;
const int Constant::BOARD_SIZE_EMIT_ROOK = 6;
const int Constant::BOARD_SIZE_K_VS_B = 6;
const int Constant::BOARD_SIZE_TIME_LINE_INVASION = 5;
const int Constant::BOARD_SIZE_TIME_LINE_BATTLE = 5;
const int Constant::BOARD_SIZE_TIME_LINE_FRAGMENT = 4;

} // namespace Chess
