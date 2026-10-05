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

// ---------------------------------------------------------------------------------------------------------------
// Move generation. One generic implementation, written against a small "Access" interface, so that the game
// (RealAccess: shared Board objects) and the turn search (a compact private position) generate exactly the same moves.
//
//   int dim() const;                                 board size N
//   bool exists(int tl, int half) const;             does the board (timeline, half-turn) exist?
//   Cell at(int tl, int half, int x, int y) const;   contents of a square of an existing board
//   bool castlingEnabled() const / doubleStepEnabled() const
//
// Coordinates: x = file, y = rank, tl = timeline ID, half = half-turn (z in full turns is half / 2).
// Movement vectors are those of 5d-chess-js (src/piece.js movePos / moveVecs), in (dx, dy, dz, dw) with dz in FULL
// turns (a move of dz changes the half-turn by 2*dz, so that it lands on a board of the mover's colour) and dw the
// change of the timeline ID.
// ---------------------------------------------------------------------------------------------------------------
namespace {

struct Cell {
  uint8_t v = 0; // 0 = empty; bits 0-2: PieceType + 1, bit 3: colour, bit 4: unmoved
  inline bool empty() const { return v == 0; }
  inline PieceType type() const { return PieceType((v & 7) - 1); }
  inline PieceColor color() const { return PieceColor((v >> 3) & 1); }
  inline bool unmoved() const { return (v & 16) != 0; }
  static inline Cell make(PieceType type, PieceColor color, bool unmoved) {
    Cell c;
    c.v = uint8_t((int(type) + 1) | (int(color) << 3) | (unmoved ? 16 : 0));
    return c;
  }
  static inline Cell of(const std::shared_ptr<Piece>& piece) {
    return piece ? make(piece->type(), piece->color(), piece->unmoved()) : Cell();
  }
};

using Vec = std::array<int, 4>; // dx, dy, dz (full turns), dw (timelines)

enum class Gait { Step, Ray };

// Every non-zero vector of {-1,0,1}^4 with at least `minNonZero` and at most `maxNonZero` non-zero components.
std::vector<Vec> unitVectors(int minNonZero, int maxNonZero) {
  std::vector<Vec> out;
  for (int a = -1; a <= 1; ++a) for (int b = -1; b <= 1; ++b) for (int c = -1; c <= 1; ++c) for (int d = -1; d <= 1; ++d) {
    const int nz = (a != 0) + (b != 0) + (c != 0) + (d != 0);
    if (nz >= minNonZero && nz <= maxNonZero) out.push_back(Vec{a, b, c, d});
  }
  return out;
}

// Knight: 2 along one axis and 1 along another, over all ordered pairs of distinct axes of the 4 (48 vectors).
std::vector<Vec> knightVectors() {
  std::vector<Vec> out;
  for (int long_ = 0; long_ < 4; ++long_) for (int short_ = 0; short_ < 4; ++short_) {
    if (long_ == short_) continue;
    for (int sl : {-1, 1}) for (int ss : {-1, 1}) {
      Vec v{0, 0, 0, 0};
      v[long_] = 2 * sl;
      v[short_] = ss;
      out.push_back(v);
    }
  }
  return out;
}

} // namespace

const std::vector<std::array<int, 4>>& pieceVectors(PieceType type) {
  // Rook: one axis. Bishop: exactly two axes (6 planes x 4 diagonals). Queen: any of the 80 combinations.
  // King: the same 80 combinations, one step. Knight: 48 jumps. (5d-chess-js moveVecs / movePos.)
  static const std::vector<Vec> rook = unitVectors(1, 1);
  static const std::vector<Vec> bishop = unitVectors(2, 2);
  static const std::vector<Vec> queen = unitVectors(1, 4);
  static const std::vector<Vec> knight = knightVectors();
  static const std::vector<Vec> none;
  switch (type) {
    case PieceType::Rook: return rook;
    case PieceType::Bishop: return bishop;
    case PieceType::Queen: return queen;
    case PieceType::King: return queen;
    case PieceType::Knight: return knight;
    case PieceType::Pawn: break;
  }
  return none;
}

namespace {

// Is the square (x, y) of an existing board attacked, on that board alone, by a piece of colour `by`? (Castling
// only looks at the board the king stands on.) The square itself may be empty or occupied.
template <class A>
bool attacked2D(const A& a, int tl, int half, int x, int y, PieceColor by) {
  const int n = a.dim();
  auto at = [&](int px, int py) -> Cell { return (px < 0 || px >= n || py < 0 || py >= n) ? Cell() : a.at(tl, half, px, py); };
  auto is = [&](const Cell& c, PieceType t) { return !c.empty() and c.color() == by and c.type() == t; };
  static const int knight[8][2] = {{1, 2}, {2, 1}, {-1, 2}, {-2, 1}, {1, -2}, {2, -1}, {-1, -2}, {-2, -1}};
  for (const auto& k : knight) {
    if (is(at(x + k[0], y + k[1]), PieceType::Knight)) return true;
  }
  const int pawnFrom = by == PieceColor::PIECEWHITE ? -1 : 1; // a white pawn attacks one rank up, so it stands one down
  for (int dx : {-1, 1}) {
    if (is(at(x + dx, y + pawnFrom), PieceType::Pawn)) return true;
  }
  for (int dx = -1; dx <= 1; ++dx) {
    for (int dy = -1; dy <= 1; ++dy) {
      if ((dx != 0 or dy != 0) and is(at(x + dx, y + dy), PieceType::King)) return true;
    }
  }
  for (int dx = -1; dx <= 1; ++dx) {
    for (int dy = -1; dy <= 1; ++dy) {
      if (dx == 0 and dy == 0) continue;
      const bool diagonal = dx != 0 and dy != 0;
      for (int px = x + dx, py = y + dy; px >= 0 && px < n && py >= 0 && py < n; px += dx, py += dy) {
        const Cell c = a.at(tl, half, px, py);
        if (c.empty()) continue;
        if (c.color() == by and (c.type() == PieceType::Queen or c.type() == (diagonal ? PieceType::Bishop : PieceType::Rook)))
          return true;
        break;
      }
    }
  }
  return false;
}

// Calls emit(tl, half, x, y, promotes) for every target square the piece on (tl, half, x, y) can move to. `promotes`
// is true when a pawn reaches the last rank. Castling is the king's two-file step, en passant the pawn's diagonal step
// onto the empty square behind the captured pawn; makeMove recognises both from the geometry.
template <class A, class Emit>
void generateMoves(const A& a, int tl, int half, int x, int y, Emit&& emit) {
  const int n = a.dim();
  const Cell piece = a.at(tl, half, x, y);
  assert(!piece.empty());
  const PieceColor mover = piece.color();

  if (piece.type() != PieceType::Pawn) {
    const auto& vectors = pieceVectors(piece.type());
    const bool ray = piece.type() == PieceType::Rook or piece.type() == PieceType::Bishop or piece.type() == PieceType::Queen;
    for (const Vec& v : vectors) {
      int ptl = tl + v[3], phalf = half + 2 * v[2], px = x + v[0], py = y + v[1];
      while (px >= 0 && px < n && py >= 0 && py < n && a.exists(ptl, phalf)) {
        const Cell target = a.at(ptl, phalf, px, py);
        if (!target.empty() and target.color() == mover) break;
        emit(ptl, phalf, px, py, false);
        if (!ray or !target.empty()) break;
        ptl += v[3]; phalf += 2 * v[2]; px += v[0]; py += v[1];
      }
    }
    // Castling (2D, same board): king and rook unmoved, everything between them empty, and neither the king's
    // square, the square it crosses nor the square it lands on attacked on this board.
    if (piece.type() == PieceType::King and piece.unmoved() and a.castlingEnabled()) {
      const PieceColor enemy = opposite(mover);
      for (int dir : {-1, +1}) {
        int fx = x + dir;
        while (fx >= 0 && fx < n && a.at(tl, half, fx, y).empty()) fx += dir;
        if (fx < 0 || fx >= n || std::abs(fx - x) < 3) continue;
        const Cell rook = a.at(tl, half, fx, y);
        if (rook.type() != PieceType::Rook or rook.color() != mover or not rook.unmoved()) continue;
        if (attacked2D(a, tl, half, x, y, enemy) or attacked2D(a, tl, half, x + dir, y, enemy)
            or attacked2D(a, tl, half, x + 2 * dir, y, enemy)) continue;
        emit(tl, half, x + 2 * dir, y, false);
      }
    }
    return;
  }

  // Pawn.
  const int d = mover == PieceColor::PIECEWHITE ? 1 : -1; // forward on the rank axis (and, reversed, on the timeline axis)
  const int lastRank = mover == PieceColor::PIECEWHITE ? n - 1 : 0;
  const int ny = y + d;
  if (ny >= 0 && ny < n) {
    if (a.at(tl, half, x, ny).empty()) {
      emit(tl, half, x, ny, ny == lastRank);
      const int ny2 = y + 2 * d;
      if (a.doubleStepEnabled() and piece.unmoved() and ny2 >= 0 and ny2 < n and a.at(tl, half, x, ny2).empty()) {
        emit(tl, half, x, ny2, ny2 == lastRank);
      }
    }
    for (int dx : {-1, +1}) {
      const int nx = x + dx;
      if (nx < 0 || nx >= n) continue;
      const Cell target = a.at(tl, half, nx, ny);
      if (!target.empty()) {
        if (target.color() != mover) emit(tl, half, nx, ny, ny == lastRank);
        continue;
      }
      // En passant: an enemy pawn right beside us made the double step in the last half-turn of this timeline.
      const int startY = y + 2 * d;
      const Cell beside = a.at(tl, half, nx, y);
      if (beside.empty() or beside.type() != PieceType::Pawn or beside.color() == mover) continue;
      if (startY < 0 || startY >= n or !a.at(tl, half, nx, startY).empty()) continue;
      if (!a.exists(tl, half - 1)) continue;
      const Cell before = a.at(tl, half - 1, nx, startY);
      if (!before.empty() and before.type() == PieceType::Pawn and before.color() != mover and before.unmoved()
          and a.at(tl, half - 1, nx, ny).empty() and a.at(tl, half - 1, nx, y).empty()) {
        emit(tl, half, nx, ny, false);
      }
    }
  }
  // Timeline axis: one step "forward" onto the same square. Forward on the timeline axis is the direction of the
  // opponent's timelines (5d-chess-js: timelineMove(l, -forward)), i.e. White towards lower IDs, Black towards higher.
  const int ntl = tl - d;
  if (a.exists(ntl, half) and a.at(ntl, half, x, y).empty()) {
    emit(ntl, half, x, y, false);
    if (a.doubleStepEnabled() and piece.unmoved() and a.exists(ntl - d, half) and a.at(ntl - d, half, x, y).empty()) {
      emit(ntl - d, half, x, y, false);
    }
  }
  // Timeline-axis capture: one timeline forward and one full turn back or ahead in time (same square).
  for (int dh : {-2, +2}) {
    if (!a.exists(ntl, half + dh)) continue;
    const Cell target = a.at(ntl, half + dh, x, y);
    if (!target.empty() and target.color() != mover) emit(ntl, half + dh, x, y, false);
  }
}

// Access to the boards of a real game.
class RealAccess {
public:
  explicit RealAccess(const IGame& game) : _game(game) {}
  inline int dim() const { return _game.dim(); }
  inline bool exists(int tl, int half) const { return _game.boardExists(tl, half); }
  inline Cell at(int tl, int half, int x, int y) const {
    return Cell::of(_game.getBoard(tl, half)->getPiece(Position2D(x, y)));
  }
  inline bool castlingEnabled() const { return _game.rule().castling; }
  inline bool doubleStepEnabled() const { return _game.rule().pawnCanMakeTwoMoveOnFirstTurn; }
private:
  const IGame& _game;
};

} // namespace

std::vector<SelectedPosition> IGame::_movesFor(PieceColor mover, SelectedPosition selected) const {
  std::shared_ptr<const Piece> piece = selected.board->getPiece(selected.position);
  if (piece == nullptr) {
    throw std::runtime_error("No piece at selected position");
  }
  if (piece->color() != mover) {
    throw std::runtime_error("Piece color does not match current turn color");
  }
  std::vector<SelectedPosition> moveablePositions;
  RealAccess access(*this);
  generateMoves(access, selected.board->timeLineId(), selected.board->halfTurnNumber(), selected.position.x(),
                selected.position.y(), [&](int tl, int half, int x, int y, bool) {
                  moveablePositions.emplace_back(getBoard(tl, half), Position2D(x, y));
                });
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
