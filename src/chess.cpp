#include "chess.h"
#include <cassert>
#include <optional>

Position::Position(int x, int y, int z, int t) 
  : _coordinates{x, y, z, t} {}

int Position::x(void) const {
  return _coordinates[0];
}

int Position::y(void) const {
  return _coordinates[1];
}

int Position::z(void) const {
  return _coordinates[2];
}

int Position::t(void) const {
  return _coordinates[3];
}

Piece::Piece(PieceType type, PieceColor color) 
  : _type(type), _color(color) {}

PieceType Piece::type(void) const {
  return _type;
}

PieceColor Piece::color(void) const {
  return _color;
}

template<int N>
Board<N>::Board(void) 
  : _board{} {}

template<int N>
const std::array<std::optional<Piece>, N>& Board<N>::operator [](int x) const {
  assert(x >= 0 && x < N);
  return _board[x];
}

Move::Move(Position from, Position to, Piece piece) 
  : _from(from), _to(to), _piece(piece) {}

Position Move::from(void) const {
  return _from;
}

Position Move::to(void) const {
  return _to;
}

Piece Move::piece(void) const {
  return _piece;
}

template<int N>
std::optional<Piece> Game<N>::get_piece(Position pos) const {
  return _timelines[pos.x()][pos.y()][pos.z()][pos.t()];
}

template<int N>
void Game<N>::make_move(const Move& move) {
  //TODO: Implement the logic to make a move in the game.
}


template<int N>
std::vector<Move> Game<N>::generate_valid_moves(Position pos) const {
  std::vector<Move> valid_moves;
  // Logic to generate valid moves for the piece at position `pos`
  // This is a placeholder; actual implementation will depend on game rules.
  return valid_moves;
}

template<int N>
int Game<N>::current_timeline_index(void) {
  return _presentTimelineIndex;
}

template<int N>
int Game<N>::timeline_count(void) const {
  return _timelines.size();
}

template<int N>
int Game<N>::parent_timeline_index(void) const {
  return _parent[_presentTimelineIndex];
}

template<int N>
int Game<N>::timeline_offset(void) const {
  return _timelineOffsets[_presentTimelineIndex];
}

template<int N>
std::optional<PieceColor> Game<N>::created_by(int timelineIndex) const {
  return _createdBy[timelineIndex];
}