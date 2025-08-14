#include "chess.h"

using namespace Chess;

Board::Board(int N) : _N(N), _fullTurnNumber(0), _halfTurnNumber(0), _previousBoard(nullptr) {
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

std::shared_ptr<const TimeLine> Game::makeMove(Move move) {
  assert(move._fromTurn == _presentFullTurn);
  assert(move._toTurn <= _presentFullTurn);
  
  if (move._fromTurn == move._toTurn) {
    
  }
}

std::shared_ptr<Board> BoardBuilder::buildStandardBoard(void) {
  std::shared_ptr<Board> board = std::make_shared<Board>(Constant::BOARD_SIZE);
  for (int i = 0; i < Constant::BOARD_SIZE; i += 1) {
    board->placePiece({i, 1}, std::make_shared<Pawn>(Color::WHITE));
    board->placePiece({i, 6}, std::make_shared<Pawn>(Color::BLACK));
  }
  board->placePiece({0, 0}, std::make_shared<Rook>(Color::WHITE));
  board->placePiece({1, 0}, std::make_shared<Knight>(Color::WHITE));
  board->placePiece({2, 0}, std::make_shared<Bishop>(Color::WHITE));
  board->placePiece({3, 0}, std::make_shared<Queen>(Color::WHITE));
  board->placePiece({4, 0}, std::make_shared<King>(Color::WHITE));
  board->placePiece({5, 0}, std::make_shared<Bishop>(Color::WHITE));
  board->placePiece({6, 0}, std::make_shared<Knight>(Color::WHITE));
  board->placePiece({7, 0}, std::make_shared<Rook>(Color::WHITE));

  board->placePiece({0, 7}, std::make_shared<Rook>(Color::BLACK));
  board->placePiece({1, 7}, std::make_shared<Knight>(Color::BLACK));
  board->placePiece({2, 7}, std::make_shared<Bishop>(Color::BLACK));
  board->placePiece({3, 7}, std::make_shared<Queen>(Color::BLACK));
  board->placePiece({4, 7}, std::make_shared<King>(Color::BLACK));
  board->placePiece({5, 7}, std::make_shared<Bishop>(Color::BLACK));
  board->placePiece({6, 7}, std::make_shared<Knight>(Color::BLACK));
  board->placePiece({7, 7}, std::make_shared<Rook>(Color::BLACK));
  return board;
}