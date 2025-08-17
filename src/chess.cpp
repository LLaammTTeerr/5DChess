#include "chess.h"

// using namespace Chess;
namespace Chess {

Vector4D::Vector4D(int x, int y, int z, int w) : _data({x, y, z, w}) {}

inline int Vector4D::x(void) const {
  return _data[0];
}

inline int Vector4D::y(void) const {
  return _data[1];
}

inline int Vector4D::z(void) const {
  return _data[2];
}

inline int Vector4D::w(void) const {
  return _data[3];
}

inline int Vector4D::operator * (const Vector4D& other) const {
  return _data[0] * other._data[0] + _data[1] * other._data[1] +
         _data[2] * other._data[2] + _data[3] * other._data[3];
}

inline int Vector4D::operator - (const Vector4D& other) const {
  return _data[0] - other._data[0] + _data[1] - other._data[1] +
         _data[2] - other._data[2] + _data[3] - other._data[3];
}

const int Constant::BOARD_SIZE = 8;

Piece::Piece(PieceColor color, std::shared_ptr<Board> board, Position2D position)
    : _color(color), _board(board), _position(position) {}

King::King(PieceColor color, std::shared_ptr<Board> board, Position2D position)
    : Piece(color, board, position) {}

Queen::Queen(PieceColor color, std::shared_ptr<Board> board, Position2D position)
    : Piece(color, board, position) {}
Rook::Rook(PieceColor color, std::shared_ptr<Board> board, Position2D position)
    : Piece(color, board, position) {}
Bishop::Bishop(PieceColor color, std::shared_ptr<Board> board, Position2D position)
    : Piece(color, board, position) {}
Knight::Knight(PieceColor color, std::shared_ptr<Board> board, Position2D position)
    : Piece(color, board, position) {}
Pawn::Pawn(PieceColor color, std::shared_ptr<Board> board, Position2D position)
    : Piece(color, board, position) {}

Board::Board(int N, std::shared_ptr<TimeLine> timeLine) : _N(N), _fullTurnNumber(0), _halfTurnNumber(0), _previousBoard(nullptr), _timeLine(timeLine) {
  _pieces.resize(N, std::vector<std::shared_ptr<Piece>>(N, nullptr));
}

int Board::dim(void) const {
  return _N;
}

void Board::placePiece(Position2D position, std::shared_ptr<Piece> piece) {
  assert(position.x() >= 0 && position.x() < _N);
  assert(position.y() >= 0 && position.y() < _N);
  _pieces[position.x()][position.y()] = std::move(piece);
}

std::shared_ptr<const Piece> Board::getPiece(Position2D position) const {
  assert(position.x() >= 0 && position.x() < _N);
  assert(position.y() >= 0 && position.y() < _N);
  return _pieces[position.x()][position.y()];
}

std::shared_ptr<TimeLine> Board::getTimeLine() const {
  return _timeLine;
}

TimeLine::TimeLine(int N, int ID) : _N(N), _ID(ID), _forkAt(-1), _parent(nullptr), _fullTurnNumber(0), _halfTurnNumber(0) {}

inline int TimeLine::ID(void) const {
  return _ID;
}

inline int TimeLine::dim(void) const {
  return _N;
}

inline int TimeLine::forkAt(void) const {
  return _forkAt;
}

inline int TimeLine::fullTurnNumber(void) const {
  return _fullTurnNumber;
}

inline int TimeLine::halfTurnNumber(void) const {
  return _halfTurnNumber;
}

inline std::shared_ptr<const TimeLine> TimeLine::parent(void) const {
  return _parent;
}

void TimeLine::pushBack(std::shared_ptr<Board> board) {
  _history.push_back(board);
}

Game::Game(int N, std::shared_ptr<Board> board) : _N(N), _presentFullTurn(0), _presentHalfTurn(0) {
  _timeLines.push_back(std::make_shared<TimeLine>(N));
  _timeLines.back()->pushBack(std::move(board));
}

Game::Game(std::function<std::shared_ptr<Board>()> boardBuilder) : _presentFullTurn(0), _presentHalfTurn(0) {
  std::shared_ptr<Board> board = boardBuilder();
  _timeLines.push_back(std::make_shared<TimeLine>(board->dim()));
  _timeLines.back()->pushBack(std::move(board));
}

inline int Game::dim(void) const {
  return _N;
}

inline int Game::presentFullTurn(void) const {
  return _presentFullTurn;
}

inline int Game::presentHalfTurn(void) const {
  return _presentHalfTurn;
}

std::vector<std::shared_ptr<Board>> Game::getMoveableBoards(void) const {
  std::vector<std::shared_ptr<Board>> moveableBoards;
  for (std::shared_ptr<TimeLine> timeLine: _timeLines) {
    if (timeLine->fullTurnNumber() == _presentFullTurn && timeLine->halfTurnNumber() == _presentHalfTurn) {
      moveableBoards.push_back(timeLine->back());
    }
  }
  return moveableBoards;
}

inline bool Game::undoable(void) const {
  return _currentTurnMoves.size() > 0;
}

std::shared_ptr<Board> BoardBuilder::buildStandardBoard(void) {
  std::shared_ptr<Board> board = std::make_shared<Board>(Constant::BOARD_SIZE, nullptr);
  for (int i = 0; i < Constant::BOARD_SIZE; i += 1) {
    board->placePiece({i, 1}, std::make_shared<Pawn>(PieceColor::PIECEWHITE, board, Position2D(i, 1)));
    board->placePiece({i, 6}, std::make_shared<Pawn>(PieceColor::PIECEBLACK, board, Position2D(i, 6)));
  }
  board->placePiece({0, 0}, std::make_shared<Rook>(PieceColor::PIECEWHITE, board, Position2D(0, 0)));
  board->placePiece({1, 0}, std::make_shared<Knight>(PieceColor::PIECEWHITE, board, Position2D(1, 0)));
  board->placePiece({2, 0}, std::make_shared<Bishop>(PieceColor::PIECEWHITE, board, Position2D(2, 0)));
  board->placePiece({3, 0}, std::make_shared<Queen>(PieceColor::PIECEWHITE, board, Position2D(3, 0)));
  board->placePiece({4, 0}, std::make_shared<King>(PieceColor::PIECEWHITE, board, Position2D(4, 0)));
  board->placePiece({5, 0}, std::make_shared<Bishop>(PieceColor::PIECEWHITE, board, Position2D(5, 0)));
  board->placePiece({6, 0}, std::make_shared<Knight>(PieceColor::PIECEWHITE, board, Position2D(6, 0)));
  board->placePiece({7, 0}, std::make_shared<Rook>(PieceColor::PIECEWHITE, board, Position2D(7, 0)));

  board->placePiece({0, 7}, std::make_shared<Rook>(PieceColor::PIECEBLACK, board, Position2D(0, 7)));
  board->placePiece({1, 7}, std::make_shared<Knight>(PieceColor::PIECEBLACK, board, Position2D(1, 7)));
  board->placePiece({2, 7}, std::make_shared<Bishop>(PieceColor::PIECEBLACK, board, Position2D(2, 7)));
  board->placePiece({3, 7}, std::make_shared<Queen>(PieceColor::PIECEBLACK, board, Position2D(3, 7)));
  board->placePiece({4, 7}, std::make_shared<King>(PieceColor::PIECEBLACK, board, Position2D(4, 7)));
  board->placePiece({5, 7}, std::make_shared<Bishop>(PieceColor::PIECEBLACK, board, Position2D(5, 7)));
  board->placePiece({6, 7}, std::make_shared<Knight>(PieceColor::PIECEBLACK, board, Position2D(6, 7)));
  board->placePiece({7, 7}, std::make_shared<Rook>(PieceColor::PIECEBLACK, board, Position2D(7, 7)));
  return board;
}

} // namespace Chess
