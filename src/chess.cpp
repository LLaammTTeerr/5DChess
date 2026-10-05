#include "chess.h"

#ifdef NDEBUG
#error "chess.cpp must be built with assertions enabled (NDEBUG undefined)"
#endif

#include <stdexcept>
#include <iostream>
#include <climits>
#include <algorithm>
#include <cstdlib>
#include <map>

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
  ++_stateVersion;
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
  std::vector<SelectedPosition> moves = _movesFor(_currentTurnColor, selected);
  // Never offer a king capture (makeMove forbids it): a legal game cannot reach a position where it is possible. The
  // internal threat generation (_threatsAgainst) uses _movesFor directly and does see king captures.
  moves.erase(std::remove_if(moves.begin(), moves.end(),
                             [](const SelectedPosition& to) {
                               auto piece = to.board->getPiece(to.position);
                               return piece != nullptr and piece->type() == PieceType::King;
                             }),
              moves.end());
  return moves;
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
    ++_stateVersion;
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
  ++_stateVersion;
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
    _origMin(other._origMin),
    _origMax(other._origMax),
    _setupDone(other._setupDone),
    _stateVersion(other._stateVersion) {
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

// ---------------------------------------------------------------------------------------------------------------
// TurnSearch
//
// The question: does the side to move ("the mover", M) have a legal turn? A turn is a sequence of moves; it is legal when
// at least one move was made, no mandatory board is left, and no king of M can be captured by the opponent ("E") on a
// board that ends on E's turn. The search is exhaustive. It is fast because of these facts about the rules.
//
// F1 (moves). All moves M can make this turn are fixed at its start: a move starts on a tip of M's colour and ends on a
//    board of M's colour. Moves create boards of E's colour only (a move extends a timeline by one board, or forks a
//    new one whose first board follows the target), so no new board of M's colour appears and none changes. The set U of
//    (piece, target) pairs therefore only SHRINKS during the turn, when a source tip has been used up (a tip is used up
//    by moving from it or onto it). Candidates are generated once, at the root.
// F2 (captures are permanent: pruning). Boards never change, boards and timelines only get added, and E's tips stay E's
//    tips. A pair (E piece on an E tip, M king on a board of E's colour) in which the piece can capture the king stays
//    such a pair when more boards exist (a sliding path stays open: new boards only exist at the ends of timelines, and a
//    board cannot become non-empty). So a position in which a king is already capturable can never be repaired: prune it.
//    The whole check is a plain pair test on the E tips x the kings (`attacks`), no move lists. The same argument gives
//    "dead" candidates: if the board a move leaves behind (its source half alone) already allows a capture, the move can
//    never be part of a legal turn, whatever else is played and wherever it lands; if its two halves together do (it
//    extends a tip), it is dead as long as that tip is still a tip.
// F3 (commutation). Two moves from different tips commute (same result in either order, each still playable after the
//    other) unless one moves onto the other's source tip, both move onto the same tip, or both fork a timeline. Of two
//    independent moves only one order is searched: moves that fork a timeline come after those that do not (a fork
//    changes the IDs of later forks), and otherwise the order of a ranking of the source boards.
// F4 (irrelevant moves). A move from an optional board (not one that has to be moved on) that neither forks a timeline nor
//    lands on a mandatory board cannot change which boards are mandatory, and only adds boards (so only adds threats): a
//    legal turn without it stays legal, UNLESS another move of the turn lands on its source or target tip (then that move
//    forks instead of extending), or the board is on a timeline that a fork of M can still activate. Such a move is only
//    tried in those cases.
// F5 (forward checking). A mandatory board stops being mandatory only if a move leaves it, a move lands on it, or a fork
//    pulls the present back before it. If none of those moves is still alive (not dead by F2) the position is hopeless.
// The proof that F3/F4 keep completeness is by taking a shortest legal turn: F4 never removes one of its moves, and
// sorting it by adjacent swaps of independent moves (F3) gives a sequence the search visits.
// ---------------------------------------------------------------------------------------------------------------
namespace {

struct Brd {
  std::array<uint8_t, 64> c; // cells, index y * n + x (Cell::v)
};

struct SLine {
  bool exists = false;
  int forkAt = 0;
  std::vector<int> boards; // arena indices; board k is half-turn forkAt + 1 + k
};

struct Cand {
  int srcBid;
  int srcTl, srcHalf;
  int8_t sx, sy;
  int dstTl, dstHalf;
  int8_t dx, dy;
  int dstBid;      // bid of the destination if it is a tip of the mover's colour at the root, else -1
  uint8_t promo;   // 0, or PieceType + 1 of the promotion piece
  bool capture, king;
  int8_t kind = 0; // 0: within a board, 1: onto a tip of another board, 2: into the past (forks a timeline)
  int8_t srcDeadF = -1, extDeadF = -1; // -1 unknown, 0 alive, 1 dead (F2), computed at the root
};

struct TipRef {
  int tl, half, arena;
  uint8_t count;
  std::array<uint8_t, 64> pos; // squares (y * n + x) of the E pieces on this board
};

struct KingPos {
  int tl, half;
  int8_t x, y;
};

struct Undo {
  int srcTl = 0, dstTl = 0, createdId = 0;
  bool sameBoard = false, created = false, targetDone = false;
  int oldMin = 0, oldMax = 0;
  size_t arenaSize = 0, tipSize = 0, kingSize = 0;
};

} // namespace

struct TurnSearch::Impl {
  Options opt;
  // Snapshot -------------------------------------------------------------------------------------------------------
  int n = 0;
  bool castling = true, doubleStep = true;
  PieceColor mover = PieceColor::PIECEWHITE, enemy = PieceColor::PIECEBLACK;
  std::vector<Brd> arena;
  std::vector<std::shared_ptr<Board>> realBoards; // arena index -> board of the game, for the root boards
  int base = 0;
  std::vector<SLine> lines;
  int minId = 0, maxId = 0, origMin = 0, origMax = 0;
  std::vector<TipRef> enemyTips;
  std::vector<KingPos> kings; // kings of the mover on every board of the enemy's colour
  int pending = 0;            // moves already made in the game's current turn
  int rootMinId = 0, rootMaxId = 0;
  bool rootInCheckFlag = false;
  // Root tips of the mover's colour ("bids") and the candidates ---------------------------------------------------
  int nBids = 0;
  std::vector<int> bidTl, bidHalf, bidRank;
  std::vector<int> bidOfLine; // by line index; -1 if the line has no tip of the mover's colour at the root
  std::vector<Cand> cands;
  std::vector<std::vector<int>> fromBid; // per bid: its candidates
  std::vector<std::vector<int>> byDst;   // per bid: candidates from OTHER boards that land on it
  std::vector<int> forkCands;            // cross-board candidates whose target is not a tip at the root
  std::vector<int8_t> nodeSrc, nodeFull; // dead flags found at the current node (1), valid in its subtree
  std::vector<int> trail;
  int phase = 1; // 1: optional boards only when they obviously matter (fast, incomplete); 2: everything (complete)
  std::vector<long long> bidFail;
  long long restartLimit = 20000, restartAt = 20000, restartCount = 0;
  size_t initCursor = 0; // candidates whose dead flags have been computed so far
  std::vector<char> consumed;
  // Search ---------------------------------------------------------------------------------------------------------
  struct Frame {
    size_t next = 0;
    int via = -1;           // candidate that led here
    bool viaCreated = false;
    std::vector<char> mand; // mandatory bids at this node
    int mandCount = 0;
    int present = 0;
    bool activationLowers = false;
    bool nextActive = false;
    size_t trailMark = 0;
  };
  std::vector<Frame> stack;
  std::vector<Undo> undos;
  std::vector<char> usedDst; // per applied move: did it use up the destination tip as well
  Status st = Status::Running;
  long long nodeCount = 0;
  int budgetLeft = 0;
  std::vector<Step> found;
  long long prunedThreat = 0, prunedForward = 0;
  size_t maxDepth = 0;

  // Access interface for generateMoves ---------------------------------------------------------------------------
  inline int dim() const { return n; }
  inline bool castlingEnabled() const { return castling; }
  inline bool doubleStepEnabled() const { return doubleStep; }
  inline const SLine* lineAt(int tl) const {
    const int i = tl - base;
    return (i < 0 || i >= int(lines.size())) ? nullptr : &lines[size_t(i)];
  }
  inline SLine* lineAt(int tl) {
    const int i = tl - base;
    return (i < 0 || i >= int(lines.size())) ? nullptr : &lines[size_t(i)];
  }
  inline int tipHalf(const SLine& l) const { return l.forkAt + int(l.boards.size()); }
  inline bool exists(int tl, int half) const {
    const SLine* l = lineAt(tl);
    if (!l || !l->exists) return false;
    const int pos = half - l->forkAt - 1;
    return pos >= 0 && pos < int(l->boards.size());
  }
  inline Cell at(int tl, int half, int x, int y) const {
    const SLine* l = lineAt(tl);
    Cell c;
    c.v = arena[size_t(l->boards[size_t(half - l->forkAt - 1)])].c[size_t(y * n + x)];
    return c;
  }
  inline bool isActive(int id) const {
    if (id >= origMin && id <= origMax) return true;
    const int whiteCreated = std::max(0, maxId - origMax);
    const int blackCreated = std::max(0, origMin - minId);
    if (id > origMax) return id - origMax <= blackCreated + 1;
    return origMin - id <= whiteCreated + 1;
  }

  Impl(const IGame& game, Options options, PieceColor moverColor, bool forQuery);

  // --- position helpers ---------------------------------------------------------------------------------------------
  // Fills `out` with the mandatory bids; returns their number and the present.
  int mandatoryBids(std::vector<char>& out, int& presentOut) const {
    out.assign(size_t(nBids), 0);
    int present = INT_MAX;
    for (int id = minId; id <= maxId; ++id) {
      const SLine* l = lineAt(id);
      if (l && l->exists && isActive(id)) present = std::min(present, tipHalf(*l));
    }
    presentOut = present;
    if (present == INT_MAX || present % 2 != int(mover)) return 0;
    int count = 0;
    for (int id = minId; id <= maxId; ++id) {
      const SLine* l = lineAt(id);
      if (!l || !l->exists || !isActive(id) || tipHalf(*l) != present) continue;
      const int bid = bidOfLine[size_t(id - base)];
      assert(bid >= 0);
      out[size_t(bid)] = 1;
      ++count;
    }
    return count;
  }

  void pushTip(int tl, int half, int arenaIdx) {
    TipRef t;
    t.tl = tl;
    t.half = half;
    t.arena = arenaIdx;
    t.count = 0;
    const Brd& b = arena[size_t(arenaIdx)];
    for (int i = 0; i < n * n; ++i) {
      Cell c;
      c.v = b.c[size_t(i)];
      if (!c.empty() and c.color() == enemy) t.pos[t.count++] = uint8_t(i);
    }
    enemyTips.push_back(t);
  }

  void pushKings(int tl, int half, int arenaIdx) {
    const Brd& b = arena[size_t(arenaIdx)];
    for (int i = 0; i < n * n; ++i) {
      Cell c;
      c.v = b.c[size_t(i)];
      if (!c.empty() and c.type() == PieceType::King and c.color() == mover)
        kings.push_back(KingPos{tl, half, int8_t(i % n), int8_t(i / n)});
    }
  }

  // Can the enemy piece `a` on (tla, ha, ax, ay) capture the king k? Mirrors generateMoves for the enemy piece.
  inline bool attacks(Cell a, int ax, int ay, int tla, int ha, const KingPos& k) const {
    const int dx = k.x - ax, dy = k.y - ay, dz = (k.half - ha) / 2, dw = k.tl - tla;
    const int adx = std::abs(dx), ady = std::abs(dy), adz = std::abs(dz), adw = std::abs(dw);
    const int m = std::max(std::max(adx, ady), std::max(adz, adw));
    if (m == 0) return false;
    switch (a.type()) {
      case PieceType::Knight: {
        int twos = 0, ones = 0;
        for (int v : {adx, ady, adz, adw}) {
          if (v == 2) ++twos;
          else if (v == 1) ++ones;
          else if (v != 0) return false;
        }
        return twos == 1 && ones == 1;
      }
      case PieceType::King:
        return m == 1;
      case PieceType::Pawn: {
        const int d = a.color() == PieceColor::PIECEWHITE ? 1 : -1;
        return (dz == 0 && dw == 0 && dy == d && adx == 1) || (dx == 0 && dy == 0 && dw == -d && adz == 1);
      }
      case PieceType::Rook:
      case PieceType::Bishop:
      case PieceType::Queen: {
        int nz = 0;
        for (int v : {adx, ady, adz, adw}) {
          if (v == 0) continue;
          if (v != m) return false;
          ++nz;
        }
        if (a.type() == PieceType::Rook && nz != 1) return false;
        if (a.type() == PieceType::Bishop && nz != 2) return false;
        const int ux = (dx > 0) - (dx < 0), uy = (dy > 0) - (dy < 0), uz = (dz > 0) - (dz < 0), uw = (dw > 0) - (dw < 0);
        for (int s = 1; s < m; ++s) {
          const int tl = tla + s * uw, half = ha + 2 * s * uz, x = ax + s * ux, y = ay + s * uy;
          if (!exists(tl, half) || !at(tl, half, x, y).empty()) return false;
        }
        return true;
      }
    }
    return false;
  }

  static inline bool compatible(int dz, int dw) {
    const int a = std::abs(dz), b = std::abs(dw);
    return a == b || a == 0 || b == 0 || (a == 1 && b == 2) || (a == 2 && b == 1);
  }

  // Can any enemy piece on an enemy tip capture any king of the mover? This is the test that makes F2 cheap: instead of
  // generating the moves of every enemy piece (what IGame::threatsAgainst does, ~100x slower) it asks, for each pair
  // (enemy piece, king), whether the displacement between them is one the piece can travel, and whether the squares in
  // between exist and are empty. `compatible` rejects most board pairs from the timeline/time displacement alone. It is a
  // full recomputation per node rather than a delta against the parent: simple, and exact even when a new board lies on
  // the path of two old ones (a delta would have to track that). A delta version is future work (docs/SEARCH.md).
  bool anyThreat() const {
    for (const TipRef& e : enemyTips) {
      for (const KingPos& k : kings) {
        if (!compatible((k.half - e.half) / 2, k.tl - e.tl)) continue;
        const Brd& b = arena[size_t(e.arena)];
        for (int i = 0; i < e.count; ++i) {
          const int sq = e.pos[size_t(i)];
          Cell a;
          a.v = b.c[size_t(sq)];
          if (attacks(a, sq % n, sq / n, e.tl, e.half, k)) return true;
        }
      }
    }
    return false;
  }

  // --- applying a move ------------------------------------------------------------------------------------------------
  inline Cell arrivingCell(const Cand& c, Cell piece) const {
    return Cell::make(c.promo ? PieceType(c.promo - 1) : piece.type(), mover, false);
  }

  // The source half of a move: the board its source timeline gets (for a move within a board, the whole move).
  void applySource(const Cand& c, Undo& u) {
    u.arenaSize = arena.size();
    u.tipSize = enemyTips.size();
    u.kingSize = kings.size();
    u.oldMin = minId;
    u.oldMax = maxId;
    u.srcTl = c.srcTl;
    u.dstTl = c.dstTl;
    u.created = false;
    u.targetDone = false;
    u.sameBoard = c.dstTl == c.srcTl && c.dstHalf == c.srcHalf;

    SLine& S = *lineAt(c.srcTl);
    Brd nb = arena[size_t(S.boards.back())];
    const int si = c.sy * n + c.sx, di = c.dy * n + c.dx;
    Cell piece;
    piece.v = nb.c[size_t(si)];
    nb.c[size_t(si)] = 0;
    if (u.sameBoard) {
      const int dxm = c.dx - c.sx;
      const bool castle = piece.type() == PieceType::King && c.dy == c.sy && std::abs(dxm) == 2;
      Cell target;
      target.v = nb.c[size_t(di)];
      const bool enPassant = piece.type() == PieceType::Pawn && dxm != 0 && target.empty();
      if (enPassant) nb.c[size_t(c.sy * n + c.dx)] = 0;
      if (castle) {
        const int dir = dxm > 0 ? 1 : -1;
        int rx = c.sx + dir;
        while (nb.c[size_t(c.sy * n + rx)] == 0) rx += dir;
        nb.c[size_t(c.sy * n + rx)] = 0;
        nb.c[size_t(c.sy * n + c.sx + dir)] = Cell::make(PieceType::Rook, mover, false).v;
      }
      nb.c[size_t(di)] = arrivingCell(c, piece).v;
    }
    arena.push_back(nb);
    S.boards.push_back(int(arena.size()) - 1);
    pushTip(c.srcTl, c.srcHalf + 1, int(arena.size()) - 1);
    pushKings(c.srcTl, c.srcHalf + 1, int(arena.size()) - 1);
  }

  // The target half of a move to another board: extends the target timeline or forks a new one. Returns whether a
  // timeline was created.
  bool applyTarget(const Cand& c, Undo& u) {
    u.targetDone = true;
    const Cell piece = at(c.srcTl, c.srcHalf, c.sx, c.sy);
    const Cell arriving = arrivingCell(c, piece);
    const int di = c.dy * n + c.dx;
    SLine& T = *lineAt(c.dstTl);
    if (tipHalf(T) == c.dstHalf) {
      Brd nt = arena[size_t(T.boards.back())];
      nt.c[size_t(di)] = arriving.v;
      arena.push_back(nt);
      T.boards.push_back(int(arena.size()) - 1);
      pushTip(c.dstTl, c.dstHalf + 1, int(arena.size()) - 1);
      pushKings(c.dstTl, c.dstHalf + 1, int(arena.size()) - 1);
      return false;
    }
    Brd nt = arena[size_t(T.boards[size_t(c.dstHalf - T.forkAt - 1)])];
    nt.c[size_t(di)] = arriving.v;
    arena.push_back(nt);
    const int id = mover == PieceColor::PIECEWHITE ? maxId + 1 : minId - 1;
    SLine* L = lineAt(id);
    assert(L && !L->exists);
    L->exists = true;
    L->forkAt = c.dstHalf;
    L->boards.assign(1, int(arena.size()) - 1);
    if (id > maxId) maxId = id;
    if (id < minId) minId = id;
    u.created = true;
    u.createdId = id;
    pushTip(id, c.dstHalf + 1, int(arena.size()) - 1);
    pushKings(id, c.dstHalf + 1, int(arena.size()) - 1);
    return true;
  }

  // Returns whether a timeline was created.
  bool apply(const Cand& c, Undo& u) {
    applySource(c, u);
    return u.sameBoard ? false : applyTarget(c, u);
  }

  void undo(const Undo& u) {
    if (u.created) {
      SLine& L = *lineAt(u.createdId);
      L.exists = false;
      L.boards.clear();
    } else if (u.targetDone) {
      lineAt(u.dstTl)->boards.pop_back();
    }
    lineAt(u.srcTl)->boards.pop_back();
    minId = u.oldMin;
    maxId = u.oldMax;
    arena.resize(u.arenaSize);
    enemyTips.resize(u.tipSize);
    kings.resize(u.kingSize);
  }

  // --- dead candidates (F2) ----------------------------------------------------------------------------------------------
  // Computed in the root position only (the position as the search started), one node each, before the search proper.
  inline bool srcDead(int ci) const {
    assert(cands[size_t(ci)].srcDeadF >= 0);
    return cands[size_t(ci)].srcDeadF == 1;
  }

  // Both halves together, in the root position: valid for a move onto a tip while that tip is still a tip, and for a fork
  // while no timeline has been created yet (a fork's ID, and so the position of its board, depends on earlier forks).
  inline bool extDead(int ci) const {
    assert(cands[size_t(ci)].extDeadF >= 0);
    return cands[size_t(ci)].extDeadF == 1;
  }

  bool computeSrcDead(Cand& c) {
    Undo u;
    applySource(c, u);
    c.srcDeadF = anyThreat() ? 1 : 0;
    undo(u);
    return c.srcDeadF == 1;
  }

  void computeExtDead(Cand& c) {
    c.extDeadF = 0;
    if (c.srcDeadF == 1) {
      c.extDeadF = 1;
      return;
    }
    if (c.dstTl == c.srcTl && c.dstHalf == c.srcHalf) return;
    Undo u;
    apply(c, u); // onto a tip: an extension; otherwise a fork, with the ID the root position gives it
    c.extDeadF = anyThreat() ? 1 : 0;
    undo(u);
  }

  // --- search ------------------------------------------------------------------------------------------------------------
  inline bool isTipNow(int tl, int half) const {
    const SLine* l = lineAt(tl);
    return l && l->exists && tipHalf(*l) == half;
  }

  // An inactive timeline of the opponent's is activated by timelines the mover creates; moving on it now can therefore
  // matter later (it changes where the present will be). All other timelines keep their activity during the turn.
  bool activatable(int tl) const {
    if (isActive(tl)) return false;
    return mover == PieceColor::PIECEWHITE ? tl < origMin : tl > origMax;
  }

  // Would the timeline the mover creates next be active? (Later ones are never more likely to be.)
  bool nextLineActive() const {
    if (mover == PieceColor::PIECEWHITE) {
      const int blackCreated = std::max(0, origMin - minId);
      return (maxId + 1) - origMax <= blackCreated + 1;
    }
    const int whiteCreated = std::max(0, maxId - origMax);
    return origMin - (minId - 1) <= whiteCreated + 1;
  }

  inline bool noTimelineCreated() const { return minId == rootMinId && maxId == rootMaxId; }

  bool createsTimeline(const Cand& c) const {
    const bool same = c.dstTl == c.srcTl && c.dstHalf == c.srcHalf;
    return !same && !isTipNow(c.dstTl, c.dstHalf);
  }

  // Is some other still-playable candidate landing on board `bid`, from a source other than `except`?
  bool landedOnByOther(int bid, int except) const {
    if (bid < 0) return false;
    for (int j : byDst[size_t(bid)]) {
      const Cand& o = cands[size_t(j)];
      if (o.srcBid != except && !consumed[size_t(o.srcBid)]) return true;
    }
    return false;
  }

  // The next candidate to try at the node of frame `f`, or -1. This is where the search avoids work it provably need not do
  // (see the F-numbers at the top): moves already known to be dead; orderings of independent moves other than the
  // canonical one (the previous move `f.via` is the only one compared: a sequence with no adjacent out-of-order independent
  // pair is enough, and every legal turn can be brought into that form); and moves of optional boards that cannot matter.
  // Candidates come sorted by source rank (mandatory boards with the fewest live moves first, so the likeliest failure is
  // met at the top), within a board by "inside the board, onto a tip, into the past", kings and captures first.
  int nextCandidate(Frame& f) {
    while (f.next < cands.size()) {
      const int ci = int(f.next++);
      const Cand& c = cands[size_t(ci)];
      if (consumed[size_t(c.srcBid)]) continue;
      if (srcDead(ci) || nodeSrc[size_t(ci)] == 1) continue;
      if (c.kind == 1 ? (!consumed[size_t(c.dstBid)] && isTipNow(c.dstTl, c.dstHalf) && (extDead(ci) || nodeFull[size_t(ci)] == 1))
                      : (c.kind == 2 && noTimelineCreated() && (extDead(ci) || nodeFull[size_t(ci)] == 1)))
        continue;
      if (opt.reductions) {
        // F3: of two independent moves, only one order.
        if (opt.commutation && f.via >= 0) {
          const Cand& p = cands[size_t(f.via)];
          const bool cCreates = createsTimeline(c);
          const bool before = cCreates != f.viaCreated ? !cCreates : bidRank[size_t(c.srcBid)] < bidRank[size_t(p.srcBid)];
          if (before) {
            const bool pSame = p.dstTl == p.srcTl && p.dstHalf == p.srcHalf;
            const bool dependent = (c.dstTl == p.srcTl && c.dstHalf == p.srcHalf) ||
                                   (!pSame && c.dstTl == p.dstTl && c.dstHalf == p.dstHalf) || (f.viaCreated && cCreates);
            if (!dependent) continue;
          }
        }
        // Phase 1 only: of the optional boards, only moves that land on a mandatory board or can pull the present back.
        if (phase == 1 && opt.cheapFirst && !f.mand[size_t(c.srcBid)]) {
          const bool onMandatory = c.kind == 1 && c.dstBid >= 0 && f.mand[size_t(c.dstBid)];
          const bool clearer = c.kind == 2 && f.nextActive && c.dstHalf + 1 < f.present;
          if (!onMandatory && !clearer && !f.activationLowers) continue;
        }
        // F4: an irrelevant move of an optional board.
        // (Not while no move has been made: a turn needs at least one move, so there it is the move itself that counts.)
        if (opt.irrelevant && !f.mand[size_t(c.srcBid)] && pending + int(stack.size()) - 1 >= 1) {
          const bool same = c.dstTl == c.srcTl && c.dstHalf == c.srcHalf;
          const bool inert = !activatable(c.srcTl) &&
                             (same || (isTipNow(c.dstTl, c.dstHalf) && !activatable(c.dstTl) &&
                                       !(c.dstBid >= 0 && f.mand[size_t(c.dstBid)])));
          if (inert && !landedOnByOther(c.srcBid, c.srcBid) && !(!same && landedOnByOther(c.dstBid, c.srcBid))) continue;
        }
      }
      return ci;
    }
    return -1;
  }

  void fillFrame(Frame& f) {
    f.mandCount = mandatoryBids(f.mand, f.present);
    f.activationLowers = false;
    f.nextActive = nextLineActive();
    for (int id = minId; id <= maxId && f.mandCount > 0; ++id) {
      const SLine* l = lineAt(id);
      if (l && l->exists && activatable(id) && tipHalf(*l) < f.present) {
        f.activationLowers = true;
        break;
      }
    }
  }

  // Dead flags found at the current node (F2 again, but with the boards of the moves made so far): they hold in the whole
  // subtree and are forgotten when the search backs up past the node. Only "dead" is remembered: a move that is fine now
  // may be dead after further moves.
  bool nodeSrcDead(int ci) {
    if (srcDead(ci) || nodeSrc[size_t(ci)] == 1) return true;
    Undo u;
    applySource(cands[size_t(ci)], u);
    const bool dead = anyThreat();
    undo(u);
    --budgetLeft;
    ++nodeCount;
    if (dead) {
      nodeSrc[size_t(ci)] = 1;
      trail.push_back(ci * 2);
    }
    return dead;
  }

  // Both halves; only meaningful for a move onto a tip (while it is one) and for a fork while no timeline was created.
  bool nodeFullDead(int ci) {
    if (extDead(ci) || nodeFull[size_t(ci)] == 1) return true;
    Undo u;
    apply(cands[size_t(ci)], u);
    const bool dead = anyThreat();
    undo(u);
    --budgetLeft;
    ++nodeCount;
    if (dead) {
      nodeFull[size_t(ci)] = 1;
      trail.push_back(ci * 2 + 1);
    }
    return dead;
  }

  // F5: every mandatory board must still have a way out.
  //
  // Only the root's kind == 2 candidates (forkCands, from the root's tips) are scanned for the fork way out, and only
  // when the fork can matter: f.activationLowers, or f.nextActive with a fork ending before the present; a candidate
  // also counts only while its source is alive and, when no timeline has been created yet, not fully dead
  // (noTimelineCreated() && nodeFullDead). That is sufficient because a fork cannot rescue a mandatory board in any
  // other way:
  //  - The mover's own inactive line cannot produce an active fork: a timeline the mover creates is inactive, so a fork
  //    out of it never becomes an active line that must be played or can lower the present (hence the f.nextActive
  //    precondition: without it no new line is activated).
  //  - The opponent's inactive line is what can matter, since activating it lowers the present; that case is exactly
  //    f.activationLowers, which keeps the node unanalysed (see the end of this function).
  //  - Parity rules out a fork whose tip equals the present: the new tip is one half-turn after the target and belongs
  //    to the opponent, so a fork helps only by ending before the present (dstHalf + 1 < present), which is the test
  //    on the candidates below.
  bool forwardCheck(const Frame& f) {
    for (int b = 0; b < nBids; ++b) {
      if (!f.mand[size_t(b)]) continue;
      bool alive = false;
      for (int ci : fromBid[size_t(b)]) {
        if (!nodeSrcDead(ci)) { alive = true; break; }
      }
      if (!alive) {
        for (int ci : byDst[size_t(b)]) {
          if (consumed[size_t(cands[size_t(ci)].srcBid)]) continue;
          if (!nodeSrcDead(ci) && !nodeFullDead(ci)) { alive = true; break; }
        }
      }
      if (!alive) {
        // A fork whose new timeline ends before the present (or any fork, if it can activate a timeline that does).
        for (int ci : forkCands) {
          const Cand& c = cands[size_t(ci)];
          if (consumed[size_t(c.srcBid)]) continue;
          if (!f.activationLowers && (!f.nextActive || c.dstHalf + 1 >= f.present)) continue;
          if (!nodeSrcDead(ci) && !(noTimelineCreated() && nodeFullDead(ci))) { alive = true; break; }
        }
      }
      if (!alive && f.activationLowers) alive = true; // not analysed: keep the node
      if (!alive) {
        ++bidFail[size_t(b)];
        return false;
      }
    }
    return true;
  }

  // Source ranking: mandatory boards first; among them the ones that failed most often so far, then the ones with the
  // fewest live moves (the likeliest to fail), then by timeline. Ranks only decide the order of the search, not what is
  // searched, so they can change at a restart.
  void rank(const std::vector<char>& mand) {
    std::vector<int> alive(size_t(nBids), 0);
    for (const Cand& c : cands) {
      if (mand[size_t(c.srcBid)] && c.srcDeadF == 0) ++alive[size_t(c.srcBid)];
    }
    std::vector<int> order(static_cast<size_t>(nBids));
    for (int i = 0; i < nBids; ++i) order[size_t(i)] = i;
    std::sort(order.begin(), order.end(), [&](int a, int b) {
      if (mand[size_t(a)] != mand[size_t(b)]) return mand[size_t(a)] > mand[size_t(b)];
      if (mand[size_t(a)]) {
        if (bidFail[size_t(a)] != bidFail[size_t(b)]) return bidFail[size_t(a)] > bidFail[size_t(b)];
        if (alive[size_t(a)] != alive[size_t(b)]) return alive[size_t(a)] < alive[size_t(b)];
      }
      return bidTl[size_t(a)] < bidTl[size_t(b)];
    });
    bidRank.assign(size_t(nBids), 0);
    for (int r = 0; r < nBids; ++r) bidRank[size_t(order[size_t(r)])] = r;
  }

  // Sort the candidates by source rank (within a board: moves inside the board, then onto a tip, then forks; kings and
  // captures first: the likeliest to work) and rebuild the per-board indexes.
  void arrange() {
    std::stable_sort(cands.begin(), cands.end(), [&](const Cand& a, const Cand& b) {
      if (bidRank[size_t(a.srcBid)] != bidRank[size_t(b.srcBid)]) return bidRank[size_t(a.srcBid)] < bidRank[size_t(b.srcBid)];
      if (a.kind != b.kind) return a.kind < b.kind;
      if (a.king != b.king) return a.king;
      return a.capture && !b.capture;
    });
    fromBid.assign(size_t(nBids), {});
    byDst.assign(size_t(nBids), {});
    forkCands.clear();
    for (size_t i = 0; i < cands.size(); ++i) {
      const Cand& c = cands[i];
      fromBid[size_t(c.srcBid)].push_back(int(i));
      if (c.kind == 1) byDst[size_t(c.dstBid)].push_back(int(i));
      if (c.kind == 2) forkCands.push_back(int(i));
    }
    nodeSrc.assign(cands.size(), -1);
    nodeFull.assign(cands.size(), -1);
  }

  // Restart with a new ranking when the search keeps failing (the usual cure for a bad early choice that depth-first
  // backtracking would only undo after exhausting everything below it). The limit doubles, so the last run is unbounded
  // in practice and the search stays complete.
  void restart() {
    while (stack.size() > 1) backUp();
    const std::vector<char> mand = stack.back().mand;
    rank(mand);
    arrange();
    stack.back().next = 0;
    restartAt = nodeCount + restartLimit;
    restartLimit *= 2;
    ++restartCount;
  }

  void pushFrame(int via, bool viaCreated) {
    Frame f;
    f.via = via;
    f.viaCreated = viaCreated;
    fillFrame(f);
    f.trailMark = trail.size();
    stack.push_back(std::move(f));
  }

  void backUp() {
    const int via = stack.back().via;
    const size_t mark = stack.back().trailMark;
    while (trail.size() > mark) {
      const int e = trail.back();
      trail.pop_back();
      (e & 1 ? nodeFull : nodeSrc)[size_t(e >> 1)] = -1;
    }
    stack.pop_back();
    undo(undos.back());
    undos.pop_back();
    const Cand& c = cands[size_t(via)];
    consumed[size_t(c.srcBid)] = 0;
    if (usedDst.back()) consumed[size_t(c.dstBid)] = 0;
    usedDst.pop_back();
  }

  Status run(int budget) {
    if (st != Status::Running) return st;
    budgetLeft = budget;
    // First compute the dead flags of all candidates in the root position (a node each).
    while (initCursor < cands.size() && budgetLeft > 0) {
      Cand& c = cands[initCursor++];
      if (c.srcDeadF < 0) computeSrcDead(c);
      computeExtDead(c);
      --budgetLeft;
      ++nodeCount;
    }
    if (initCursor < cands.size()) return Status::Running;
    while (budgetLeft > 0) {
      if (opt.reductions && nodeCount > restartAt && stack.size() > 1) restart();
      Frame& f = stack.back();
      const int ci = nextCandidate(f);
      if (ci < 0) {
        if (stack.size() == 1) {
          if (phase == 1 && opt.reductions && opt.cheapFirst) {
            phase = 2; // the cheap search found nothing: now the complete one
            stack.back().next = 0;
            continue;
          }
          st = Status::None;
          return st;
        }
        backUp();
        continue;
      }
      const Cand& c = cands[size_t(ci)];
      --budgetLeft;
      ++nodeCount;
      Undo u;
      const bool created = apply(c, u);
      if (anyThreat()) {
        undo(u);
        ++prunedThreat;
        continue;
      }
      // The move is kept for now: its source tip, and the tip it extended, are used up.
      consumed[size_t(c.srcBid)] = 1;
      char extended = 0;
      if (!created && !u.sameBoard && c.dstBid >= 0 && !consumed[size_t(c.dstBid)]) {
        consumed[size_t(c.dstBid)] = 1;
        extended = 1;
      }
      undos.push_back(u);
      usedDst.push_back(extended);
      pushFrame(ci, created);
      if (pending + int(stack.size()) - 1 >= 1 && stack.back().mandCount == 0) {
        found.clear();
        for (size_t i = 1; i < stack.size(); ++i) found.push_back(toStep(cands[size_t(stack[i].via)]));
        st = Status::Found;
        return st;
      }
      maxDepth = std::max(maxDepth, stack.size());
      if (opt.reductions && opt.forwardCheck && !forwardCheck(stack.back())) {
        backUp();
        ++prunedForward;
      }
    }
    return Status::Running;
  }

  std::shared_ptr<Board> realBoard(int tl, int half) const {
    const SLine* l = lineAt(tl);
    return realBoards[size_t(l->boards[size_t(half - l->forkAt - 1)])];
  }

  Step toStep(const Cand& c) const {
    Step s;
    s.move = Move{SelectedPosition(realBoard(c.srcTl, c.srcHalf), Position2D(c.sx, c.sy)),
                  SelectedPosition(realBoard(c.dstTl, c.dstHalf), Position2D(c.dx, c.dy))};
    s.promotion = c.promo ? PieceType(c.promo - 1) : PieceType::Queen;
    return s;
  }
};

TurnSearch::Impl::Impl(const IGame& game, Options options, PieceColor moverColor, bool forQuery) : opt(options) {
  n = game.dim();
  assert(n <= 8);
  phase = opt.reductions && opt.cheapFirst ? 1 : 2;
  restartLimit = restartAt = opt.restartLimit;
  castling = game._rule.castling;
  doubleStep = game._rule.pawnCanMakeTwoMoveOnFirstTurn;
  mover = moverColor;
  enemy = opposite(mover);
  minId = game.minTimeLineId();
  maxId = game.maxTimeLineId();
  origMin = game._origMin;
  origMax = game._origMax;
  pending = int(game._currentTurnMoves.size());
  rootMinId = minId;
  rootMaxId = maxId;

  // Root tips of the mover's colour get a "bid"; the lines vector has room for every timeline a turn can create.
  int tips = 0;
  for (const auto& kv : game._timeLines) {
    if (kv.second->halfTurnNumber() % 2 == int(mover)) ++tips;
  }
  const int pad = tips + 2;
  base = minId - pad;
  lines.resize(size_t(maxId - minId + 1 + 2 * pad));
  bidOfLine.assign(lines.size(), -1);
  for (const auto& kv : game._timeLines) {
    SLine& l = lines[size_t(kv.first - base)];
    l.exists = true;
    l.forkAt = kv.second->forkAt();
    for (const auto& board : kv.second->getBoards()) {
      Brd b;
      for (int y = 0; y < n; ++y) {
        for (int x = 0; x < n; ++x) b.c[size_t(y * n + x)] = Cell::of(board->getPiece(Position2D(x, y))).v;
      }
      arena.push_back(b);
      realBoards.push_back(board);
      l.boards.push_back(int(arena.size()) - 1);
    }
    if (kv.second->halfTurnNumber() % 2 == int(mover)) {
      bidOfLine[size_t(kv.first - base)] = nBids++;
      bidTl.push_back(kv.first);
      bidHalf.push_back(kv.second->halfTurnNumber());
    } else {
      pushTip(kv.first, kv.second->halfTurnNumber(), l.boards.back());
    }
  }
  // Kings of the mover on every board of the enemy's colour.
  for (int id = minId; id <= maxId; ++id) {
    const SLine* l = lineAt(id);
    if (!l || !l->exists) continue;
    for (size_t k = 0; k < l->boards.size(); ++k) {
      const int half = l->forkAt + 1 + int(k);
      if (half % 2 == int(enemy)) pushKings(id, half, l->boards[k]);
    }
  }
  arena.reserve(arena.size() + size_t(4 * tips + 16));

  if (forQuery) return;

  // In check? Pass the mandatory boards (copy them forward) and see whether a king can be captured.
  consumed.assign(size_t(nBids), 0);
  std::vector<char> mand0;
  int present0 = 0;
  mandatoryBids(mand0, present0);
  {
    const size_t a0 = arena.size(), t0 = enemyTips.size(), k0 = kings.size();
    for (int bid = 0; bid < nBids; ++bid) {
      if (!mand0[size_t(bid)]) continue;
      SLine& l = *lineAt(bidTl[size_t(bid)]);
      arena.push_back(arena[size_t(l.boards.back())]);
      l.boards.push_back(int(arena.size()) - 1);
      pushTip(bidTl[size_t(bid)], bidHalf[size_t(bid)] + 1, int(arena.size()) - 1);
      pushKings(bidTl[size_t(bid)], bidHalf[size_t(bid)] + 1, int(arena.size()) - 1);
    }
    rootInCheckFlag = anyThreat();
    for (int bid = 0; bid < nBids; ++bid) {
      if (mand0[size_t(bid)]) lineAt(bidTl[size_t(bid)])->boards.pop_back();
    }
    arena.resize(a0);
    enemyTips.resize(t0);
    kings.resize(k0);
  }

  if (anyThreat()) {
    st = Status::None; // a king is capturable and stays so
    return;
  }

  // Generate every candidate move of the mover from the root tips.
  std::map<std::pair<int, int>, int> bidOfBoard;
  for (int b = 0; b < nBids; ++b) bidOfBoard[{bidTl[size_t(b)], bidHalf[size_t(b)]}] = b;
  for (int bid = 0; bid < nBids; ++bid) {
    const int tl = bidTl[size_t(bid)], half = bidHalf[size_t(bid)];
    for (int y = 0; y < n; ++y) {
      for (int x = 0; x < n; ++x) {
        const Cell piece = at(tl, half, x, y);
        if (piece.empty() or piece.color() != mover) continue;
        generateMoves(*this, tl, half, x, y, [&](int ttl, int thalf, int tx, int ty, bool promotes) {
          // A legal game never lets a king be captured (makeMove asserts it): such moves are not candidates.
          if (const Cell target = at(ttl, thalf, tx, ty); !target.empty() and target.type() == PieceType::King) return;
          Cand c;
          c.srcBid = bid;
          c.srcTl = tl;
          c.srcHalf = half;
          c.sx = int8_t(x);
          c.sy = int8_t(y);
          c.dstTl = ttl;
          c.dstHalf = thalf;
          c.dx = int8_t(tx);
          c.dy = int8_t(ty);
          auto it = bidOfBoard.find({ttl, thalf});
          c.dstBid = it == bidOfBoard.end() ? -1 : it->second;
          c.king = piece.type() == PieceType::King;
          const bool same = ttl == tl && thalf == half;
          c.kind = same ? 0 : c.dstBid >= 0 ? 1 : 2;
          c.capture = !at(ttl, thalf, tx, ty).empty() || (same && piece.type() == PieceType::Pawn && tx != x);
          if (promotes) {
            for (PieceType p : {PieceType::Queen, PieceType::Rook, PieceType::Bishop, PieceType::Knight}) {
              c.promo = uint8_t(int(p) + 1);
              cands.push_back(c);
            }
          } else {
            c.promo = 0;
            cands.push_back(c);
          }
        });
      }
    }
  }
  // The moves of the mandatory boards get their dead flags now: the ranking below uses them.
  for (Cand& c : cands) {
    if (mand0[size_t(c.srcBid)]) computeSrcDead(c);
  }
  bidFail.assign(size_t(nBids), 0);
  rank(mand0);
  arrange();

  pushFrame(-1, false);
  if (pending >= 1 && stack.back().mandCount == 0) st = Status::Found; // already submittable
}

TurnSearch::TurnSearch(const IGame& game) : TurnSearch(game, Options{}) {}

TurnSearch::TurnSearch(const IGame& game, Options options)
    : _impl(std::make_unique<Impl>(game, options, game.getCurrentTurnColor(), false)) {}

TurnSearch::~TurnSearch() = default;

TurnSearch::Status TurnSearch::step(int nodeBudget) { return _impl->run(nodeBudget); }

long long TurnSearch::restarts(void) const { return _impl->restartCount; }

TurnSearch::Status TurnSearch::status(void) const { return _impl->st; }

long long TurnSearch::nodes(void) const { return _impl->nodeCount; }

const std::vector<TurnSearch::Step>& TurnSearch::turn(void) const { return _impl->found; }

bool TurnSearch::inCheck(void) const { return _impl->rootInCheckFlag; }

bool TurnSearch::kingCapturable(const IGame& game, PieceColor victim) {
  Impl impl(game, Options{}, victim, true);
  return impl.anyThreat();
}

IGame::~IGame() = default;

TurnSearch::Status IGame::findLegalTurn(int nodeBudget) const {
  TurnSearch search(*this);
  return search.step(nodeBudget);
}

bool IGame::stepResultSearch(int nodeBudget) {
  if (!_resultSearch) return false;
  switch (_resultSearch->step(nodeBudget)) {
    case TurnSearch::Status::Running:
      return true;
    case TurnSearch::Status::Found:
      break;
    case TurnSearch::Status::None:
      if (!_resultSearch->inCheck()) _result = GameResult::Draw;
      else _result = _currentTurnColor == PieceColor::PIECEWHITE ? GameResult::BlackWins : GameResult::WhiteWins;
      break;
  }
  _resultSearch.reset();
  ++_stateVersion;
  return false;
}

bool IGame::resolveResult(long long maxNodes) {
  while (resultPending() && maxNodes > 0) {
    const int budget = int(std::min<long long>(maxNodes, 100000));
    stepResultSearch(budget);
    maxNodes -= budget;
  }
  return !resultPending();
}

void IGame::submitTurn(void) {
  assert(canSubmit());
  _presentHalfTurn = bufferHalfTurn();
  _currentTurnMoves.clear();
  _currentTurnColor = opposite(_currentTurnColor);
  _undoBuffer.clear();
  _resultSearch = std::make_unique<TurnSearch>(*this);
  ++_stateVersion;
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
