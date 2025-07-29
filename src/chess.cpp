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

Board::Board(void) 
  : _board{} {}

std::array<std::optional<Piece>, 8>& Board::operator [](int x) {
  assert(x >= 0 && x < 8);
  return _board[x];
}

const std::array<std::optional<Piece>, 8>& Board::operator [](int x) const {
  assert(x >= 0 && x < 8);
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

std::optional<Piece> Game::get_piece(Position pos) const {
  return _timelines[pos.x()][pos.y()][pos.z()][pos.t()];
}

void Game::make_move(const Move& move) {
  Position from = move.from();
  Position to = move.to();
  Piece piece = move.piece();

  _timelines[from.x()][from.y()][from.z()][from.t()] = std::nullopt;
  _timelines[to.x()][to.y()][to.z()][to.t()] = piece;
}

std::vector<Move> Game::generate_moves(Position pos) const {
  std::vector<Move> valid_moves;
  // Logic to generate valid moves for the piece at position `pos`
  // This is a placeholder; actual implementation will depend on game rules.
  return valid_moves;
}

int Game::current_timeline_index(void) {
  return _presentTimelineIndex;
}

int Game::timeline_count(void) const {
  return _timelines.size();
}

int Game::parent_timeline_index(void) const {
  return _parent[_presentTimelineIndex];
}

int Game::timeline_offset(void) const {
  return _timelineOffsets[_presentTimelineIndex];
}

std::optional<PieceColor> Game::created_by(int timelineIndex) const {
  return _createdBy[timelineIndex];
}