#include "chess.h"

#ifdef NDEBUG
#error "chess.cpp must be built with assertions enabled (NDEBUG undefined)"
#endif

#include <stdexcept>
#include <iostream>
#include <climits>
#include <algorithm>

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
    if (timeLine->fullTurnNumber() == presentFullTurn() && timeLine->halfTurnNumber() == _presentHalfTurn) {
      moveableBoards.push_back(timeLine->back());
    }
  }
  return moveableBoards;
}

bool IGame::canMakeMoveFromBoard(std::shared_ptr<Board> board) const {
  return board
    and timeLine(board->timeLineId())->fullTurnNumber() == presentFullTurn()
    and board->halfTurnNumber() == _presentHalfTurn
    and timeLine(board->timeLineId())->halfTurnNumber() == _presentHalfTurn;
}

bool IGame::boardExists(int timeLineID, int halfTurn) const {
  auto it = _timeLines.find(timeLineID);
  if (it == _timeLines.end()) return false;
  int pos = halfTurn - it->second->forkAt() - 1;
  return pos >= 0 && pos < static_cast<int>(it->second->size());
}

std::shared_ptr<Piece> IGame::_getPieceByVector4DFullTurn(Vector4D position) const {
  int x = position.x();
  int y = position.y();
  int halfTurn = 2 * position.z() + int(_currentTurnColor);
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
  _nextHalfTurnBuffer.pop_back();
  // Only the last move can have ended the game, and it is the one being undone.
  _gameWinner.reset();
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

std::vector<SelectedPosition> IGame::getMoveablePositions(SelectedPosition selected) const {
  std::shared_ptr<const Piece> piece = selected.board->getPiece(selected.position);
  Vector4D from = selected.toVector4D();
  int parity = int(_currentTurnColor);
  std::vector<SelectedPosition> moveablePositions;

  if (piece == nullptr) {
    throw std::runtime_error("No piece at selected position");
  }

  if (piece->color() != _currentTurnColor) {
    throw std::runtime_error("Piece color does not match current turn color");
  }

  if (piece->name() == "rook") {
    for (int nx = from.x() + 1; nx < dim(); nx += 1) {
      std::shared_ptr<Piece> targetPiece = _getPieceByVector4DFullTurn({nx, from.y(), from.z(), from.w()});
      if (targetPiece != nullptr and targetPiece->color() == _currentTurnColor) {
        break;
      }
      moveablePositions.emplace_back(selected.board, Position2D(nx, from.y()));
      if (targetPiece != nullptr)
        break;
    }

    for (int nx = from.x() - 1; nx >= 0; nx -= 1) {
      std::shared_ptr<Piece> targetPiece = _getPieceByVector4DFullTurn({nx, from.y(), from.z(), from.w()});
      if (targetPiece != nullptr and targetPiece->color() == _currentTurnColor) {
        break;
      }
      moveablePositions.emplace_back(selected.board, Position2D(nx, from.y()));
      if (targetPiece != nullptr)
        break;
    }

    for (int ny = from.y() + 1; ny < dim(); ny += 1) {
      std::shared_ptr<Piece> targetPiece = _getPieceByVector4DFullTurn({from.x(), ny, from.z(), from.w()});
      if (targetPiece != nullptr and targetPiece->color() == _currentTurnColor) {
        break;
      }
      moveablePositions.emplace_back(selected.board, Position2D(from.x(), ny));
      if (targetPiece != nullptr)
        break;
    }

    for (int ny = from.y() - 1; ny >= 0; ny -= 1) {
      std::shared_ptr<Piece> targetPiece = _getPieceByVector4DFullTurn({from.x(), ny, from.z(), from.w()});
      if (targetPiece != nullptr and targetPiece->color() == _currentTurnColor) {
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
      std::shared_ptr<Piece> targetPiece = _getPieceByVector4DFullTurn({from.x(), from.y(), nz, from.w()});
      if (targetPiece != nullptr and targetPiece->color() == _currentTurnColor) {
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
      std::shared_ptr<Piece> targetPiece = _getPieceByVector4DFullTurn({from.x(), from.y(), from.z(), nw});
      if (targetPiece != nullptr and targetPiece->color() == _currentTurnColor) {
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
      std::shared_ptr<Piece> targetPiece = _getPieceByVector4DFullTurn({from.x(), from.y(), from.z(), nw});
      if (targetPiece != nullptr and targetPiece->color() == _currentTurnColor) {
        break;
      }
      moveablePositions.emplace_back(getBoard(nw, 2 * from.z() + parity), Position2D(from.x(), from.y()));
      if (targetPiece != nullptr)
        break;
    }
  }

  if (piece->name() == "knight") {
    std::vector<Vector4D> knightMoves = genKnightMoves(from);

    for (const Vector4D& move : knightMoves) {
      if (move.x() >= 0 && move.x() < dim() && move.y() >= 0 && move.y() < dim()) {
        if (boardExists(move.w(), 2 * move.z() + parity)
            && (_getPieceByVector4DFullTurn(move) == nullptr || _getPieceByVector4DFullTurn(move)->color() != _currentTurnColor)) {
          moveablePositions.emplace_back(getBoard(move.w(), 2 * move.z() + parity), Position2D(move.x(), move.y()));
        }
      }
    }
  }

  if (piece->name() == "bishop") {
    for (int sx : {-1, +1}) for (int sy : {-1, +1}) {
      for (int d = 1; d < dim(); d += 1) {
        int nx = from.x() + sx * d;
        int ny = from.y() + sy * d;
        if (nx < 0 || nx >= dim() || ny < 0 || ny >= dim()) break;
        std::shared_ptr<Piece> targetPiece = _getPieceByVector4DFullTurn({nx, ny, from.z(), from.w()});
        if (targetPiece != nullptr and targetPiece->color() == _currentTurnColor) {
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
        std::shared_ptr<Piece> targetPiece = _getPieceByVector4DFullTurn({nx, from.y(), nz, from.w()});
        if (targetPiece != nullptr and targetPiece->color() == _currentTurnColor) {
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
        std::shared_ptr<Piece> targetPiece = _getPieceByVector4DFullTurn({from.x(), ny, nz, from.w()});
        if (targetPiece != nullptr and targetPiece->color() == _currentTurnColor) {
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
        std::shared_ptr<Piece> targetPiece = _getPieceByVector4DFullTurn({nx, from.y(), from.z(), nw});
        if (targetPiece != nullptr and targetPiece->color() == _currentTurnColor) {
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
        std::shared_ptr<Piece> targetPiece = _getPieceByVector4DFullTurn({from.x(), ny, from.z(), nw});
        if (targetPiece != nullptr and targetPiece->color() == _currentTurnColor) {
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
        std::shared_ptr<Piece> targetPiece = _getPieceByVector4DFullTurn({from.x(), from.y(), nz, nw});
        if (targetPiece != nullptr and targetPiece->color() == _currentTurnColor) {
          break;
        }
        moveablePositions.emplace_back(getBoard(nw, 2 * nz + parity), Position2D(from.x(), from.y()));
        if (targetPiece != nullptr) {
          break;
        }
      }
    }
  }

  if (piece->name() == "queen") {
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
          std::shared_ptr<Piece> targetPiece = _getPieceByVector4DFullTurn({nx, ny, nz, nw});
          if (targetPiece != nullptr && targetPiece->color() == _currentTurnColor) break;

          std::shared_ptr<Board> targetBoard = getBoard(nw, 2 * nz + parity);
          moveablePositions.emplace_back(targetBoard, Position2D(nx, ny));
          if (targetPiece != nullptr) break;
        }
      }
    }
  }

  if (piece->name() == "king") {
    for (int dx = -1; dx <= +1; dx += 1)
    for (int dy = -1; dy <= +1; dy += 1)
    for (int dz = -1; dz <= 0; dz += 1)
    for (int dw = -1; dw <= +1; dw += 1) {
      if (dx == 0 && dy == 0 && dz == 0 && dw == 0) continue;
      Vector4D to = Vector4D(from.x() + dx, from.y() + dy, from.z() + dz, from.w() + dw);
      if (to.x() >= 0 && to.x() < dim() && to.y() >= 0 && to.y() < dim()) {
        if (boardExists(to.w(), 2 * to.z() + parity)) {
          std::shared_ptr<Piece> targetPiece = _getPieceByVector4DFullTurn(to);
          if (targetPiece and targetPiece->color() == _currentTurnColor)
            continue;
          moveablePositions.emplace_back(getBoard(to.w(), 2 * to.z() + parity), Position2D(to.x(), to.y()));
        }
      }
    }
  }

  if (piece->name() == "pawn") {
    if (piece->color() == PieceColor::PIECEWHITE) {
      if (piece->getPosition().y() < dim() - 1 and piece->getPosition().x() > 0) {
        std::shared_ptr<Piece> targetPiece = selected.board->getPiece(Position2D(from.x() - 1, from.y() + 1));
        if (targetPiece != nullptr and targetPiece->color() == PieceColor::PIECEBLACK) {
          moveablePositions.emplace_back(selected.board, Position2D(from.x() - 1, from.y() + 1));
        }
      }
      if (piece->getPosition().y() < dim() - 1 and piece->getPosition().x() < dim() - 1) {
        std::shared_ptr<Piece> targetPiece = selected.board->getPiece(Position2D(from.x() + 1, from.y() + 1));
        if (targetPiece != nullptr and targetPiece->color() == PieceColor::PIECEBLACK) {
          moveablePositions.emplace_back(selected.board, Position2D(from.x() + 1, from.y() + 1));
        }
      }
      if (piece->getPosition().y() < dim() - 1) {
        std::shared_ptr<Piece> targetPiece = selected.board->getPiece(Position2D(from.x(), from.y() + 1));
        if (targetPiece != nullptr)
          goto SKIP_PAWN_MOVE;
        moveablePositions.emplace_back(selected.board, Position2D(from.x(), from.y() + 1));
      }

      if (_rule.pawnCanMakeTwoMoveOnFirstTurn and piece->getPosition().y() == 1) {
        std::shared_ptr<Piece> targetPiece = selected.board->getPiece(Position2D(from.x(), from.y() + 2));
        if (targetPiece != nullptr)
          goto SKIP_PAWN_MOVE;
        moveablePositions.emplace_back(selected.board, Position2D(from.x(), from.y() + 2));
      }
    }
    if (piece->color() == PieceColor::PIECEBLACK) {
      if (piece->getPosition().y() > 0 and piece->getPosition().x() > 0) {
        std::shared_ptr<Piece> targetPiece = selected.board->getPiece(Position2D(from.x() - 1, from.y() - 1));
        if (targetPiece != nullptr and targetPiece->color() == PieceColor::PIECEWHITE) {
          moveablePositions.emplace_back(selected.board, Position2D(from.x() - 1, from.y() - 1));
        }
      }
      if (piece->getPosition().y() > 0 and piece->getPosition().x() < dim() - 1) {
        std::shared_ptr<Piece> targetPiece = selected.board->getPiece(Position2D(from.x() + 1, from.y() - 1));
        if (targetPiece != nullptr and targetPiece->color() == PieceColor::PIECEWHITE) {
          moveablePositions.emplace_back(selected.board, Position2D(from.x() + 1, from.y() - 1));
        }
      }
      if (piece->getPosition().y() > 0) {
        std::shared_ptr<Piece> targetPiece = selected.board->getPiece(Position2D(from.x(), from.y() - 1));
        if (targetPiece != nullptr)
          goto SKIP_PAWN_MOVE;
        moveablePositions.emplace_back(selected.board, Position2D(from.x(), from.y() - 1));
      }
      if (_rule.pawnCanMakeTwoMoveOnFirstTurn and piece->getPosition().y() == dim() - 2) {
        std::shared_ptr<Piece> targetPiece = selected.board->getPiece(Position2D(from.x(), from.y() - 2));
        if (targetPiece != nullptr)
          goto SKIP_PAWN_MOVE;
        moveablePositions.emplace_back(selected.board, Position2D(from.x(), from.y() - 2));
      }
    }
    SKIP_PAWN_MOVE:;
  }

  return moveablePositions;
}

void IGame::makeMove(Move move) {
  assert(!gameEnd());
  std::vector<int> list;
  std::shared_ptr<Piece> piece = move.from.board->getPiece(move.from.position);
  assert(piece != nullptr);
  assert(piece->color() == _currentTurnColor);
  std::shared_ptr<Piece> moveToPiece = move.to.board->getPiece(move.to.position);
  if (moveToPiece != nullptr and moveToPiece->name() == "king") {
    assert(moveToPiece->color() != _currentTurnColor);
    _gameWinner = _currentTurnColor;
  }
  _currentTurnMoves.push_back(move);
  const int fromTimeLineId = move.from.board->timeLineId();
  // Boards are immutable once pushed to a timeline: finish building each new board before pushing it.
  std::shared_ptr<Board> newFromBoard = move.from.board->createFork(fromTimeLineId);
  newFromBoard->placePiece(move.from.position, nullptr);
  if (move.to.position.y() == 0 and piece->name() == "pawn" and piece->color() == PieceColor::PIECEBLACK) {
    piece = std::make_shared<Queen>(PieceColor::PIECEBLACK);
  }
  if (move.to.position.y() == dim() - 1 and piece->name() == "pawn" and piece->color() == PieceColor::PIECEWHITE) {
    piece = std::make_shared<Queen>(PieceColor::PIECEWHITE);
  }
  list.push_back(fromTimeLineId);
  if (move.to.board == move.from.board) {
    newFromBoard->placePiece(move.to.position, piece->clone());
    timeLine(fromTimeLineId)->pushBack(newFromBoard);
    _nextHalfTurnBuffer.push_back(newFromBoard->halfTurnNumber());
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
  newToBoard->placePiece(move.to.position, piece->clone());
  toTimeLine->pushBack(newToBoard);

  // The new board of the source timeline may lie behind the destination's (e.g. when jumping to a timeline
  // that is already ahead); the present must not skip past it.
  _nextHalfTurnBuffer.push_back(std::min(newFromBoard->halfTurnNumber(), newToBoard->halfTurnNumber()));
  _undoBuffer.push_back(list);
}

int IGame::allocateTimeLineId(PieceColor /*mover*/) const {
  // NOTE: the next step (official multiverse rules) makes Black allocate minTimeLineId() - 1 and
  // White maxTimeLineId() + 1. For now both colours get max + 1 (behaviour-preserving).
  return maxTimeLineId() + 1;
}

IGame::IGame(const IGame& other)
  : _N(other._N),
    _presentHalfTurn(other._presentHalfTurn),
    _nextHalfTurnBuffer(other._nextHalfTurnBuffer),
    _currentTurnMoves(other._currentTurnMoves),
    _currentTurnColor(other._currentTurnColor),
    _undoBuffer(other._undoBuffer),
    _rule(other._rule),
    _gameWinner(other._gameWinner) {
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

void IGame::submitTurn(void) {
  assert(!_nextHalfTurnBuffer.empty());
  _currentTurnMoves.clear();
  _currentTurnColor = opposite(_currentTurnColor);
  _presentHalfTurn = *std::min_element(_nextHalfTurnBuffer.begin(), _nextHalfTurnBuffer.end());
  _nextHalfTurnBuffer.clear();
  _undoBuffer.clear();
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
