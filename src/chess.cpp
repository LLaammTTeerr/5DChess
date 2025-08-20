#include "chess.h"
#include <stdexcept>
#include <iostream>
#include <climits>

// using namespace Chess;
namespace Chess {
Vector4D::Vector4D(int x, int y, int z, int w) : _data({x, y, z, w}) {}

const int Constant::BOARD_SIZE = 8;

Piece::Piece(PieceColor color, std::shared_ptr<Board> board, Position2D position)
    : _color(color), _board(board), _position(position) {}

Board::Board(int N, std::shared_ptr<TimeLine> timeLine) : _N(N), _halfTurnNumber(0), _previousBoard(nullptr), _timeLine(timeLine) {
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

std::shared_ptr<Board> Board::createFork(std::shared_ptr<TimeLine> timeLine) {
  std::shared_ptr<Board> forkedBoard = std::make_shared<Board>(_N, timeLine);
  for (int x = 0; x < _N; ++x) {
    for (int y = 0; y < _N; ++y) {
      std::shared_ptr<Piece> piece = _pieces[x][y];
      if (piece != nullptr) {
        forkedBoard->placePiece(Position2D(x, y), std::shared_ptr<Piece>(piece->clone()));
      }
    }
  }
  forkedBoard->_previousBoard = shared_from_this();
  forkedBoard->_halfTurnNumber += 1;
  std::cerr << _halfTurnNumber << " -> " << forkedBoard->_halfTurnNumber << std::endl;
  return forkedBoard;
}

std::shared_ptr<TimeLine> Board::getTimeLine() const {
  return _timeLine;
}

TimeLine::TimeLine(int N, int IDX) : _N(N), _ID(IDX), _forkAt(-1), _parent(nullptr) {}

void TimeLine::pushBack(std::shared_ptr<Board> board) {
  _history.push_back(board);
}

Game::Game(int N, std::function<std::shared_ptr<Board>(std::shared_ptr<TimeLine>)> boardBuilder) : _N(N), _presentHalfTurn(0), _currentTurnColor(PieceColor::PIECEWHITE), _nextHalfTurn(INT_MAX) {
  _timeLines.push_back(std::make_shared<TimeLine>(N));
  std::shared_ptr<Board> board = boardBuilder(_timeLines.back());
  _timeLines.back()->pushBack(std::move(board));
}

std::vector<std::shared_ptr<Board>> Game::getMoveableBoards(void) const {
  std::vector<std::shared_ptr<Board>> moveableBoards;
  for (std::shared_ptr<TimeLine> timeLine: _timeLines) {
    if (timeLine->fullTurnNumber() == presentFullTurn() && timeLine->halfTurnNumber() == _presentHalfTurn) {
      moveableBoards.push_back(timeLine->back());
    }
  }
  return moveableBoards;
}

bool Game::canMakeMoveFromBoard(std::shared_ptr<Board> board) const {
  return board
    and board->getTimeLine()->fullTurnNumber() == presentFullTurn()
    and board->getTimeLine()->halfTurnNumber() == _presentHalfTurn;
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

std::vector<SelectedPosition> Game::getMoveablePositions(SelectedPosition selected) const {
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
      if (_getPieceByVector4DFullTurn({nx, from.y(), from.z(), from.w()}) != nullptr) {
        break;
      }
      moveablePositions.emplace_back(selected.board, Position2D(nx, from.y()));
    }

    for (int nx = from.x() - 1; nx >= 0; nx -= 1) {
      if (_getPieceByVector4DFullTurn({nx, from.y(), from.z(), from.w()}) != nullptr) {
        break;
      }
      moveablePositions.emplace_back(selected.board, Position2D(nx, from.y()));
    }

    for (int ny = from.y() + 1; ny < dim(); ny += 1) {
      if (_getPieceByVector4DFullTurn({from.x(), ny, from.z(), from.w()}) != nullptr) {
        break;
      }
      moveablePositions.emplace_back(selected.board, Position2D(from.x(), ny));
    }

    for (int ny = from.y() - 1; ny >= 0; ny -= 1) {
      if (_getPieceByVector4DFullTurn({from.x(), ny, from.z(), from.w()}) != nullptr) {
        break;
      }
      moveablePositions.emplace_back(selected.board, Position2D(from.x(), ny));
    }

    for (int nz = from.z() - 1; nz >= 0; nz -= 1) {
      if (not boardExists(from.w(), 2 * nz + parity)) {
        break;
      }
      if (_getPieceByVector4DFullTurn({from.x(), from.y(), nz, from.w()}) != nullptr) {
        break;
      }
      moveablePositions.emplace_back(getBoard(from.w(), 2 * nz + parity), Position2D(from.x(), from.y()));
    }

    for (int nw = from.w() - 1; nw >= 0; nw -= 1) {
      if (not boardExists(nw, from.z())) {
        break;
      }
      if (_getPieceByVector4DFullTurn({from.x(), from.y(), from.z(), nw}) != nullptr) {
        break;
      }
      moveablePositions.emplace_back(getBoard(nw, from.z()), Position2D(from.x(), from.y()));
    }

    for (int nw = from.w() + 1; nw < _timeLines.size(); nw += 1) {
      if (not boardExists(nw, from.z())) {
        break;
      }
      if (_getPieceByVector4DFullTurn({from.x(), from.y(), from.z(), nw}) != nullptr) {
        break;
      }
      moveablePositions.emplace_back(getBoard(nw, from.z()), Position2D(from.x(), from.y()));
    }
  }

  if (piece->name() == "knight") {
    std::vector<Vector4D> knightMoves = genKnightMoves(from);

    for (const Vector4D& move : knightMoves) {
      if (move.x() >= 0 && move.x() < dim() && move.y() >= 0 && move.y() < dim()) {
        if (boardExists(move.w(), 2 * move.z() + parity)) {
          moveablePositions.emplace_back(getBoard(move.w(), 2 * move.z() + parity), Position2D(move.x(), move.y()));
        }
      }
    }
  }

  if (piece->name() == "bishop") {
    for (int d = 1; d < dim(); d += 1) {
      for (int sx : {-1, +1}) for (int sy : {-1, +1}) {
        int nx = from.x() + sx * d;
        int ny = from.y() + sy * d;
        if (nx < 0 || nx >= dim() || ny < 0 || ny >= dim()) continue;

        moveablePositions.emplace_back(selected.board, Position2D(nx, ny));
      }
    }

    for (int d = 1; d < dim(); d += 1) {
      for (int sx : {-1, +1}) {
        int nx = from.x() + sx * d;
        int nz = from.z() - d;
        if (nz < 0) continue;
        if (nx < 0 || nx >= dim()) continue;

        moveablePositions.emplace_back(getBoard(from.w(), 2 * nz + parity), Position2D(nx, from.y()));
      }
    }

    for (int d = 1; d < dim(); d += 1) {
      for (int sy : {-1, +1}) {
        int ny = from.y() + sy * d;
        int nz = from.z() - d;
        if (nz < 0) continue;
        if (ny < 0 || ny >= dim()) continue;

        moveablePositions.emplace_back(getBoard(from.w(), 2 * nz + parity), Position2D(from.x(), ny));
      }
    }

    for (int d = 1; d < dim(); d += 1) {
      for (int sx : {-1, +1}) for (int sw : {-1, +1}) {
        int nx = from.x() + sx * d;
        int nw = from.w() + sw * d;
        if (nx < 0 || nx >= dim()) continue;
        if (!boardExists(nw, 2 * from.z() + parity)) continue;

        moveablePositions.emplace_back(getBoard(nw, 2 * from.z() + 1), Position2D(nx, from.y()));
      }
    }

    for (int d = 1; d < dim(); d += 1) {
      for (int sy : {-1, +1}) for (int sw : {-1, +1}) {
        int ny = from.y() + sy * d;
        int nw = from.w() + sw * d;
        if (ny < 0 || ny >= dim()) continue;
        if (!boardExists(nw, 2 * from.z() + parity)) continue;

        moveablePositions.emplace_back(getBoard(nw, 2 * from.z() + 1), Position2D(from.x(), ny));
      }
    }

    for (int d = 1; d < selected.board->halfTurnNumber(); d += 1) {
      for (int sw : {-1, +1}) {
        int nz = from.z() - d;
        int nw = from.w() + sw * d;
        if (!boardExists(nw, 2 * nz + parity)) continue;

        moveablePositions.emplace_back(getBoard(nw, 2 * nz + 1), Position2D(from.x(), from.y()));
      }
    }
  }

  if (piece->name() == "queen") {

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
          moveablePositions.emplace_back(getBoard(to.w(), 2 * to.z() + 1), Position2D(to.x(), to.y()));
        }
      }
    }
  }

  if (piece->name() == "pawn") {
    if (piece->color() == PieceColor::PIECEWHITE) {
      if (piece->getPosition().y() == 1) {
        moveablePositions.emplace_back(selected.board, Position2D(from.x(), from.y() + 2));
      }
      if (piece->getPosition().y() < dim() - 1) {
        moveablePositions.emplace_back(selected.board, Position2D(from.x(), from.y() + 1));
        //TODO: PROMOTE
      }
    }
    if (piece->color() == PieceColor::PIECEBLACK) {
      if (piece->getPosition().y() == 6) {
        moveablePositions.emplace_back(selected.board, Position2D(from.x(), from.y() - 2));
      }
      if (piece->getPosition().y() > 0) {
        moveablePositions.emplace_back(selected.board, Position2D(from.x(), from.y() - 1));
        //TODO: PROMOTE
      }
    }
  }

  return moveablePositions;
}

void Game::makeMove(Move move) {
  std::shared_ptr<Piece> piece = move.from.board->getPiece(move.from.position);
  assert(piece != nullptr);
  assert(piece->color() == _currentTurnColor);
  _currentTurnMoves.push_back(move);
  std::shared_ptr<Board> newFromBoard = move.from.board->createFork(move.from.board->getTimeLine());
  move.from.board->getTimeLine()->pushBack(newFromBoard);
  newFromBoard->placePiece(move.from.position, nullptr);
  if (move.to.board == move.from.board) {
    newFromBoard->placePiece(move.to.position, piece);
    _nextHalfTurn = std::min(_nextHalfTurn, newFromBoard->halfTurnNumber());
    submitTurn();
    return;
  }

  std::shared_ptr<TimeLine> toTimeLine = move.to.board->halfTurnNumber() == move.from.board->halfTurnNumber()
    ? move.to.board->getTimeLine()
    : move.to.board->getTimeLine()->createFork(_timeLines.size(), move.to.board->halfTurnNumber());

  if (toTimeLine != move.to.board->getTimeLine()) {
    _timeLines.push_back(toTimeLine);
  }

  std::shared_ptr<Board> newToBoard = move.to.board->getTimeLine()->getBoardByHalfTurn(move.to.board->halfTurnNumber())->createFork(toTimeLine);
  newToBoard->placePiece(move.to.position, piece);
  toTimeLine->pushBack(newToBoard);

  _nextHalfTurn = std::min(_nextHalfTurn, newToBoard->halfTurnNumber());

  submitTurn();
}

void Game::submitTurn(void) {
  _currentTurnMoves.clear();
  _currentTurnColor = opposite(_currentTurnColor);
  _presentHalfTurn = _nextHalfTurn;
  std::cerr << "Present half turn: " << _presentHalfTurn << std::endl;
  _nextHalfTurn = INT_MAX;
}

} // namespace Chess
