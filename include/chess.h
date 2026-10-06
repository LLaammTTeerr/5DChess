#pragma once

#include <string>
#include <array>
#include <memory>
#include <vector>
#include <optional>
#include <queue>
#include <map>
#include <cassert>
#include <functional>
#include <cstdint>
#include <iostream>
#include <climits>
#include <algorithm>
#include <compare>
#include <functional>

namespace Chess {

using u8 = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;

class Vector4D {
public:
  Vector4D(int x, int y, int z, int w);

  inline int x(void) const {
    return _data[0];
  }

  inline int y(void) const {
    return _data[1];
  }

  inline int z(void) const {
    return _data[2];
  }

  inline int w(void) const {
    return _data[3];
  }
private:
  std::array<int, 4> _data;
};

enum class PieceColor : uint8_t {
  PIECEWHITE = 0,
  PIECEBLACK = 1,
};

inline constexpr PieceColor opposite(PieceColor c) {
  return c == PieceColor::PIECEWHITE ? PieceColor::PIECEBLACK : PieceColor::PIECEWHITE;
}

enum class PieceType : uint8_t { King, Queen, Rook, Bishop, Knight, Pawn };

/**
 * Plain-value engine types (no pointers, so they can be hashed, compared, written to notation / puzzles / save files and
 * sent to an AI). They live in Chess::Core next to the older shared_ptr based Move / SelectedPosition below, which they
 * will replace; the conversions are SelectedPosition::coord() and IGame::selected().
 */
namespace Core {

/**
 * One square of one board: file x (0 = a-file), rank y (0 = White's back rank), half-turn t of the board (0 = White's
 * first turn, odd = Black to move on it) and timeline id l (may be negative).
 * Ordered by (x, y, t, l); the order has no meaning beyond making Coord usable as a map key.
 */
struct Coord {
  int8_t x = 0, y = 0;
  int16_t t = 0, l = 0;
  constexpr bool operator==(const Coord&) const = default;
  constexpr auto operator<=>(const Coord&) const = default;
};

/** A move: source, destination, and what a pawn reaching the last rank becomes (otherwise unused, Queen by default). */
struct Move {
  Coord from, to;
  PieceType promotion = PieceType::Queen;
  constexpr bool operator==(const Move&) const = default;
};

/** A move as it was played: `promotes` tells whether a pawn reached the last rank (then `move.promotion` is what it became). */
struct PlayedMove {
  Move move;
  bool promotes = false;
  constexpr bool operator==(const PlayedMove&) const = default;
};

/** The moves of one submitted turn and the present (half-turn) the turn started at. */
struct PlayedTurn {
  int presentHalfTurn = 0;
  std::vector<PlayedMove> moves;
  bool operator==(const PlayedTurn&) const = default;
};

} // namespace Core

class Position2D;
struct Piece;
class Board;
class TimeLine;
class Multiverse;
class Move;
class RuleEngine;

class Position2D {
public:
  /**
   * Construct a Position2D object with the given x and y coordinates.
   * @param x The x coordinate.
   * @param y The y coordinate.
   * This constructor initializes the Position2D object with the specified coordinates.
   * It is used to represent a position on a chess board or any 2D grid.
   * @note The coordinates are typically zero-indexed, meaning (0, 0)
   * represents the top-left corner of the board.
   */
  Position2D(int x, int y) : _x(x), _y(y) {}

  /**
   * Get the x coordinate of the position.
   * @return The x coordinate.
   */
  inline int x(void) const { return _x; }

  /**
   * Get the y coordinate of the position.
   * @return The y coordinate.
   */
  inline int y(void) const { return _y; }

  inline bool operator == (const Position2D& other) const {
    return _x == other._x and _y == other._y;
  }
private:
  int _x;
  int _y;
};

/**
 * A piece as a plain value (no identity, no board pointer): what it is, whose it is, and whether it has never moved
 * (needed for castling by king and rook and for the pawn's double step; cleared when the piece moves, kept by a fork).
 */
struct Piece {
  PieceType type = PieceType::Pawn;
  PieceColor color = PieceColor::PIECEWHITE;
  bool unmoved = true;
  constexpr bool operator==(const Piece&) const = default;
};

/** "king", "queen", ... (the keys of the piece textures: "white_" / "black_" + name). */
const std::string& pieceName(PieceType type);
/** 'K', 'Q', 'R', 'B', 'N', 'P'. */
char pieceSymbol(PieceType type);

/**
 * One square of a board in one byte: 0 is empty; otherwise bits 0-2 hold PieceType + 1, bit 3 the colour, bit 4 "unmoved".
 * This is the storage form of Board and of the turn search; use Board::at() for a Piece.
 */
struct Cell {
  uint8_t v = 0;
  constexpr bool empty() const { return v == 0; }
  constexpr PieceType type() const { return PieceType((v & 7) - 1); }
  constexpr PieceColor color() const { return PieceColor((v >> 3) & 1); }
  constexpr bool unmoved() const { return (v & 16) != 0; }
  static constexpr Cell make(PieceType type, PieceColor color, bool unmoved) {
    return Cell{uint8_t((int(type) + 1) | (int(color) << 3) | (unmoved ? 16 : 0))};
  }
  static constexpr Cell of(const Piece& p) { return make(p.type, p.color, p.unmoved); }
  constexpr Piece piece() const { return Piece{type(), color(), unmoved()}; }
};

/**
 * COMPATIBILITY SHIM for the UI (Board::getPiece): a nullable view of a piece with the old pointer-style accessors.
 * New code uses Board::at() and the fields of Piece. To be removed with the UI's last use of getPiece().
 */
class PieceRef {
public:
  PieceRef(std::nullptr_t = nullptr) {}
  PieceRef(const Piece& piece) : _piece(piece) {}
  const PieceRef* operator->() const { return this; }
  explicit operator bool() const { return _piece.has_value(); }
  friend bool operator==(const PieceRef& r, std::nullptr_t) { return !r._piece; }
  PieceColor color() const { return _piece->color; }
  PieceType type() const { return _piece->type; }
  bool unmoved() const { return _piece->unmoved; }
  const std::string& name() const { return pieceName(_piece->type); }
  char symbol() const { return pieceSymbol(_piece->type); }
private:
  std::optional<Piece> _piece;
};

/**
 * A snapshot of one N x N position, N <= MAX_DIM, stored as a fixed array of one-byte cells (index y * N + x), so forking
 * a board is a plain copy.
 * @note Boards are IMMUTABLE once they have been pushed onto a TimeLine: place() / clear() are only for building a board.
 * This is what makes it safe to share Board objects between IGame::clone()d games (structural sharing).
 */
class Board {
public:
  static constexpr int MAX_DIM = 8;

  Board(int N, int timeLineId, int halfTurnNumber = 0);

  /** The dimension N of the board. */
  inline int dim(void) const { return _N; }

  /** The piece on a square, if any. */
  inline std::optional<Piece> at(Position2D position) const {
    const Cell c = cell(position.x(), position.y());
    return c.empty() ? std::nullopt : std::optional<Piece>(c.piece());
  }
  inline Cell cell(int x, int y) const {
    assert(x >= 0 && x < _N && y >= 0 && y < _N);
    return _cells[size_t(y * _N + x)];
  }
  /** Row-major cells (y * N + x), the first N * N entries are valid. */
  inline const std::array<Cell, MAX_DIM * MAX_DIM>& cells(void) const { return _cells; }

  /** Put a piece on a square (replacing what is there) / empty a square. Only while the board is being built. */
  void place(Position2D position, const Piece& piece);
  void clear(Position2D position);

  /** COMPATIBILITY SHIM for the UI, see PieceRef. */
  inline PieceRef getPiece(Position2D position) const {
    const auto piece = at(position);
    return piece ? PieceRef(*piece) : PieceRef();
  }

  /** The ID of the timeline this board belongs to (immutable; may be negative). */
  inline int timeLineId(void) const { return _timeLineId; }
  inline int fullTurnNumber(void) const { return _halfTurnNumber / 2; }
  inline int halfTurnNumber(void) const { return _halfTurnNumber; }

  /** A copy of this board one half-turn later, belonging to timeline `timeLineId` (still to be edited, then pushed). */
  std::shared_ptr<Board> createFork(int timeLineId) const;
private:
  int _N;
  int _halfTurnNumber;
  int _timeLineId;
  std::array<Cell, MAX_DIM * MAX_DIM> _cells{};
};

class TimeLine {
public:
  static constexpr int NO_PARENT = INT_MIN;

  TimeLine(int N, int IDX = 0, int forkAt = -1, int parentId = NO_PARENT);

  /**
   * Get the ID of the timeline.
   * @return The ID of the timeline as an integer.
   */
  inline int ID(void) const {
    return _ID;
  }

  /**
   * Get the dimension of the timeline.
   * @return The dimension of the timeline as an integer.
   * This method returns the size of the board (N).
   */
  inline int dim(void) const {
    return _N;
  }

  /**
   * Get the fork point of the timeline.
   * @return The fork point of the timeline as an integer.
   */
  inline int forkAt(void) const {
    return _forkAt;
  }

  /**
   * Get the full turn number of the timeline.
   * @return The full turn number of the timeline as an integer.
   */
  inline int fullTurnNumber(void) const {
    assert(_history.size() > 0);
    return _history.back()->fullTurnNumber();
  }

  /**
   * Get the half turn number of the timeline.
   * @return The half turn number of the timeline as an integer.
   */
  inline int halfTurnNumber(void) const {
    assert(_history.size() > 0);
    return _history.back()->halfTurnNumber();
  }

  /**
   * Get the ID of the timeline this one was forked from, or NO_PARENT if it is an original timeline.
   * (An ID rather than a pointer, so a TimeLine never points into another game.)
   */
  inline int parentId(void) const {
    return _parentId;
  }

  inline bool hasParent(void) const {
    return _parentId != NO_PARENT;
  }

  inline int size(void) const {
    return _history.size();
  }

  /**
   * Push a new board state onto the timeline.
   * @param board The board state to be added to the timeline; it must be fully built and is immutable from now on.
   */
  void pushBack(std::shared_ptr<Board> board);

  inline void popBack(void) {
    assert(!_history.empty());
    _history.pop_back();
  }

  inline std::shared_ptr<Board> back(void) {
    assert(!_history.empty());
    return _history.back();
  }

  inline std::shared_ptr<Board> getBoardByHalfTurn(int halfTurn) const {
    int pos = halfTurn - _forkAt - 1;
    assert(pos >= 0 && pos < static_cast<int>(_history.size()));
    return _history[pos];
  }

  std::shared_ptr<TimeLine> createFork(int newID, int forkAt) {
    std::shared_ptr<TimeLine> forkedTimeLine = std::make_shared<TimeLine>(_N, newID);
    forkedTimeLine->_forkAt = forkAt;
    forkedTimeLine->_parentId = _ID;
    return forkedTimeLine;
  }
private:
  int _N;
  int _ID;
  int _forkAt;
  std::vector<std::shared_ptr<Board>> _history;
  int _parentId = NO_PARENT;
public:
  std::vector<std::shared_ptr<Board>> getBoards() const { return _history; }
};

struct SelectedPosition {
  std::shared_ptr<Board> board; // Board where the position is selected
  Position2D position;  // 2D position on the board (e.g., chess coordinates as float for rendering)

  SelectedPosition() : board(nullptr), position(Position2D(-1, -1)) {} // Default constructor
  SelectedPosition(std::shared_ptr<Board> b, Position2D pos) : board(b), position(pos) {}

  inline Vector4D toVector4D(void) const {
    return Vector4D(position.x(), position.y(), board->fullTurnNumber(), board->timeLineId());
  }

  /** The same square as a pointer-free value. */
  inline Core::Coord coord(void) const {
    return Core::Coord{static_cast<int8_t>(position.x()), static_cast<int8_t>(position.y()),
                       static_cast<int16_t>(board->halfTurnNumber()), static_cast<int16_t>(board->timeLineId())};
  }
};

// Represents a move in the game, including the source and destination positions.
// Castling is a king move two files sideways, en passant a pawn's diagonal step onto the empty square behind the
// captured pawn: both are recognised by makeMove from the geometry, so a Move needs no extra fields.
class Move {
// public for easy access in TurnState
public:
  SelectedPosition from;
  SelectedPosition to;
};

/** A piece of `attacker.board` that could capture the king on `king.board` (both positions are on real boards). */
struct Threat {
  SelectedPosition attacker;
  SelectedPosition king;
};

enum class GameResult { Ongoing, WhiteWins, BlackWins, Draw };

class IGame;

/**
 * Resumable search for a legal turn of the side to move (continuing from the moves already pending, if any).
 *
 * A turn is legal when every mandatory board has been moved on (or made irrelevant by a jump into the past), at least one
 * move was made, and the opponent cannot capture any of the mover's kings (IGame::canSubmit). The search proves either
 * that such a turn exists (Found, turn() lists its moves) or that none does (None: checkmate or stalemate). It is
 * exhaustive, but it works on a private compact copy of the position, prunes every position in which a king can already
 * be captured (captures stay possible whatever else is played), plays moves of independent boards in one canonical
 * order, and ignores moves of optional boards that cannot affect the outcome (see src/chess.cpp for the argument), so
 * the mates seen in play are proven within a few thousand nodes.
 *
 * It never recurses across step() calls: all state lives in an explicit stack, so step() can be called with a small
 * budget once per frame. The IGame it was created from may change afterwards; the search keeps its own snapshot.
 */
class TurnSearch {
public:
  enum class Status {
    Found,   // a legal turn exists: turn()
    None,    // proven: no legal turn exists
    Running, // not decided yet: call step() again
  };

  /** One move of a turn; `promotion` is what a pawn reaching the last rank becomes (otherwise unused). */
  struct Step {
    Move move;
    PieceType promotion = PieceType::Queen;
  };

  struct Options {
    /** false: only the threat pruning, no other reduction; for cross-checking. Overrides the switches below. */
    bool reductions = true;
    /** Switches of the individual reductions (test knobs, see src/chess.cpp F3, F4, F5, phase 1). */
    bool commutation = true, irrelevant = true, forwardCheck = true, cheapFirst = true;
    /** Nodes before the first restart with a new ranking (doubles after every restart); a test knob so that restart()
     *  and arrange() run on small positions too. */
    long long restartLimit = 20000;
  };

  explicit TurnSearch(const IGame& game);
  TurnSearch(const IGame& game, Options options);
  ~TurnSearch();
  TurnSearch(const TurnSearch&) = delete;
  TurnSearch& operator=(const TurnSearch&) = delete;

  /**
   * Continue the search for at most `nodeBudget` nodes (a node is one move tried, whether it is then abandoned or
   * kept). Returns the status afterwards; once it is not Running it stays that way. A node costs a few microseconds,
   * roughly independent of the position, so a budget of a few thousand fits in a frame.
   */
  Status step(int nodeBudget);
  Status status(void) const;
  long long nodes(void) const;
  /** How often the search restarted with a new ranking (see Options::restartLimit); for tests and statistics. */
  long long restarts(void) const;

  /** The moves of a legal turn, in playing order; valid when status() == Found. They refer to boards of the game. */
  const std::vector<Step>& turn(void) const;

  /** Whether the side to move is in check in the position the search started from (what IGame::inCheck() says). */
  bool inCheck(void) const;

  /** Test hook: can `victim`'s king be captured in `game` as it is (same answer as !IGame::threatsAgainst().empty())? */
  static bool kingCapturable(const IGame& game, PieceColor victim);

private:
  struct Impl;
  std::unique_ptr<Impl> _impl;
};

class RuleEngine {
public:
  bool pawnCanMakeTwoMoveOnFirstTurn = true;
  bool castling = true;
};

/**
 * Movement vectors of a piece type as (dx, dy, dz, dw): file, rank, full turns (a step of dz changes the half-turn by
 * 2 * dz, so a move always lands on a board of the mover's colour) and timeline ID. King and Knight move exactly one
 * vector; Rook, Bishop and Queen slide along one until blocked or off the existing boards. Identical to the vector
 * sets of 5d-chess-js (piece.js movePos / moveVecs); empty for pawns, which are special-cased. See docs/RULES.md.
 */
const std::vector<std::array<int, 4>>& pieceVectors(PieceType type);

class IGame {
  friend class TurnSearch;
public:
  IGame(int N) : _N(N), _presentHalfTurn(0), _currentTurnColor(PieceColor::PIECEWHITE) {}
  virtual ~IGame();

  /**
   * Cheap full-state copy for search. TimeLine objects are deep-copied (their vectors of shared_ptr<Board> are
   * copied) while the immutable Boards themselves are shared. All turn bookkeeping is copied too, so moves made on
   * the clone never affect the original and vice versa. Moves/SelectedPositions taken from either game are valid
   * in both (board identity is shared).
   * @note The clone is a plain IGame; derived games only differ in their constructors.
   */
  std::unique_ptr<IGame> clone(void) const;

  /**
   * Get the dimension of the game.
   * @return The dimension of the game as an integer.
   * This method returns the size of the board (N).
   */
  inline int dim(void) const {
    return _N;
  }

  /**
   * Get the current full turn number.
   * @return The current full turn number as an integer.
   */
  inline int presentFullTurn(void) const {
    return _presentHalfTurn / 2;
  }

  /**
   * Get the current half turn number.
   * @return The current half turn number as an integer.
   */
  inline int presentHalfTurn(void) const {
    return _presentHalfTurn;
  }

  /**
   * The present as it stands right now, including the moves of the pending turn: the earliest end-turn
   * (half-turn of the latest board) among the ACTIVE timelines. Equals presentHalfTurn() when no move is pending.
   */
  int bufferHalfTurn(void) const;

  /**
   * End the current turn and hand the move to the opponent. Requires canSubmit() (asserted).
   * Afterwards the present and the side to move are updated and result() is Ongoing; the question whether the new side to
   * move has a legal turn at all is NOT answered here (that search can take long): submitTurn() only arms it, see
   * resultPending(). The caller drives it a little at a time, e.g. once per frame:
   *
   *   game.submitTurn();
   *   while (game.resultPending()) game.stepResultSearch(2000);   // or resolveResult()
   *   // result() is now WhiteWins / BlackWins / Draw if the side to move had no legal turn, else still Ongoing
   *
   * Moves made while the search is pending are fine; the search works on a snapshot of the position at submitTurn().
   */
  void submitTurn(void);

  /** True between submitTurn() and the end of the legal-turn search it armed. */
  inline bool resultPending(void) const { return _resultSearch != nullptr; }

  /**
   * Run the pending legal-turn search for at most `nodeBudget` nodes (see TurnSearch::step). When it proves that the side
   * to move has no legal turn, result() becomes the other side's win (if the side to move is in check: checkmate) or a
   * draw (stalemate). Returns resultPending() afterwards; does nothing if nothing is pending.
   */
  bool stepResultSearch(int nodeBudget);

  /** Convenience: steps until the search has decided or `maxNodes` nodes were used. Returns !resultPending(). */
  bool resolveResult(long long maxNodes = 100000000);

  /**
   * True if the pending turn may be submitted: at least one move was made, every mandatory board has been moved on
   * (the present now belongs to the opponent), and the opponent cannot capture any of the mover's kings on any
   * board it will be able to move on (threatsAgainst(mover) is empty). False once the game is over.
   */
  bool canSubmit(void) const;

  /**
   * Boards the mover MUST still move on before the turn can be submitted: the latest boards of the active
   * timelines that lie on the present, while the present belongs to the mover. Empty once the present has passed
   * to the opponent.
   */
  std::vector<std::shared_ptr<Board>> mandatoryBoards(void) const;

  /**
   * Threats against the side to move as the real game shows them: with every mandatory board (that has not been
   * moved on yet) passed, which pieces of the opponent could capture one of the mover's kings. This is what
   * makes "in check" (inCheck()). Attacker and king boards are real boards of this game (a passed board is
   * reported as the board it was passed from).
   */
  std::vector<Threat> checkingAttacks(void) const;
  inline bool inCheck(void) const { return !checkingAttacks().empty(); }

  /**
   * Kings of `victim` that the opponent could capture in the position as it is now, the opponent moving next, on
   * any board it can move on (every board that ends on the opponent's turn, active or not). No passing is simulated.
   */
  std::vector<Threat> threatsAgainst(PieceColor victim) const;

  /**
   * One-shot form of TurnSearch: runs a search from the current position (pending moves included) for at most
   * `nodeBudget` nodes. Running means the budget was exhausted.
   */
  TurnSearch::Status findLegalTurn(int nodeBudget = 1000000) const;

  /**
   * Ongoing / WhiteWins / BlackWins / Draw. Ongoing until a legal-turn search proves otherwise (see submitTurn,
   * stepResultSearch): checkmate (no legal turn, in check) is a win for the other side, stalemate a draw.
   */
  inline GameResult result(void) const { return _result; }

  /** Timelines that may be played on without being optional-only: see the active-timeline rule in docs/RULES.md. */
  std::vector<int> activeTimeLineIds(void) const;
  bool isTimeLineActive(int timeLineId) const;

  /**
   * Get the boards where the current player can make moves: the latest board of every timeline (active or not)
   * that ends on the current player's turn, whether or not it lies on the present. Only mandatoryBoards() have to
   * be moved on.
   * @return Boards in ascending timeline-ID order.
   */
  std::vector<std::shared_ptr<Board>> getMoveableBoards(void) const;

  /**
   * Get the positions where the current player can make moves.
   * @param selected The selected position to check for moveable positions.
   * @return A vector of SelectedPosition objects representing the moveable positions.
   * This method returns a vector of positions that are available for making moves based on the selected position.
   */
  std::vector<SelectedPosition> getMoveablePositions(SelectedPosition selected) const;

  /**
   * Make a (pseudo-legal) move for the current player. Legality is judged per turn, see canSubmit().
   * @param move The move to make; the source must hold a piece of the current turn's color on a moveable board.
   * @param promotion Piece a pawn becomes when it reaches the last rank (Queen, Rook, Bishop or Knight).
   * @note The game must not have ended (asserted).
   */
  void makeMove(Move move, PieceType promotion = PieceType::Queen);

  /**
   * Check if the current turn can be undone.
   * @return True if the current turn can be undone, false otherwise.
   * This method checks if there are any moves in the current turn that can be undone.
   */
  bool undoable(void) const {
    return !_currentTurnMoves.empty();
  }

  bool canMakeMoveFromBoard(std::shared_ptr<Board> board) const;

  bool boardExists(int timeLineID, int halfTurn) const;

  inline std::shared_ptr<Board> getBoard(int timeLineID, int halfTurn) const {
    return timeLine(timeLineID)->getBoardByHalfTurn(halfTurn);
  }

  // --- Value API (Chess::Core): the same operations without shared_ptr<Board> ---------------------------------------

  /** Whether the board at (c.l, c.t) exists; c.x and c.y are ignored. */
  inline bool boardExists(Core::Coord c) const { return boardExists(c.l, c.t); }

  /** The board of a timeline at a half-turn (must exist). Boards are immutable once played, hence const. */
  inline const Board& board(int timeLine, int halfTurn) const { return *getBoard(timeLine, halfTurn); }

  /** Value -> pointer form of a square (its board must exist). The other direction is SelectedPosition::coord(). */
  SelectedPosition selected(Core::Coord c) const;

  /**
   * Moves the side to move may make with the piece on `from` (same rules as getMoveablePositions: pseudo-legal, legality
   * is judged per turn by canSubmit()). A promotion yields one move per choice (Queen, Rook, Bishop, Knight).
   * Empty if there is no board, no piece of the side to move, or the board is not one it can move on.
   */
  std::vector<Core::Move> legalMovesFrom(Core::Coord from) const;

  /** makeMove(Move, PieceType) with a value move; `move.promotion` is the pawn's promotion piece. */
  void makeMove(const Core::Move& move);

  inline std::shared_ptr<Board> getNewBoard(void) const {
    assert(undoable());
    return timeLine(_undoBuffer.back().back())->back();
  }
  /**
   * Bumped by every change of the played state (makeMove, undo, submitTurn, a finished result search), so that callers
   * can cache derived answers (canSubmit(), mandatoryBoards(), checkingAttacks()) until it changes. It does not see
   * edits made to the boards behind the game's back (test sandboxes).
   */
  inline unsigned long long stateVersion(void) const { return _stateVersion; }
protected:
  int _N;
  int _presentHalfTurn;
  std::map<int, std::shared_ptr<TimeLine>> _timeLines; // keyed (and ordered) by timeline ID; IDs may be negative
  std::vector<Move> _currentTurnMoves;
  std::vector<Core::PlayedMove> _pendingMoves;           // _currentTurnMoves as values (with promotions)
  std::vector<Core::PlayedTurn> _history;                // submitted turns
  std::string _startPosition;                            // set by Core::Position::makeGame
  std::string _modeId;                                   // set by GameCatalog::create
  PieceColor _currentTurnColor;
  std::vector<std::vector<int>> _undoBuffer;
  RuleEngine _rule;
  GameResult _result = GameResult::Ongoing;
  std::unique_ptr<TurnSearch> _resultSearch; // armed by submitTurn(), never copied
  // Range of the timeline IDs present at the start of the game; timelines outside it were created by a player
  // (above: White, below: Black). Set while the game is being set up (_addTimeLine), frozen by the first move.
  int _origMin = INT_MAX;
  int _origMax = INT_MIN;
  bool _setupDone = false;
  unsigned long long _stateVersion = 0;

  std::vector<SelectedPosition> _movesFor(PieceColor mover, SelectedPosition selected) const;
  std::vector<Threat> _threatsAgainst(PieceColor victim, bool firstOnly) const;
  void _passMandatoryBoards(std::map<const Board*, std::shared_ptr<Board>>& passedFrom);
  IGame(const IGame& other);
  IGame& operator=(const IGame&) = delete;

  /** Register a timeline under its own ID (which must not be in use yet). */
  inline std::shared_ptr<TimeLine> _addTimeLine(std::shared_ptr<TimeLine> timeLine) {
    const bool inserted = _timeLines.emplace(timeLine->ID(), timeLine).second;
    assert(inserted);
    (void)inserted;
    if (!_setupDone) {
      _origMin = std::min(_origMin, timeLine->ID());
      _origMax = std::max(_origMax, timeLine->ID());
    }
    return timeLine;
  }

  /** ID for a timeline newly branched by `mover`: White gets max+1, Black min-1. */
  int allocateTimeLineId(PieceColor mover) const;
public:
  /** The timeline with the given ID (must exist). */
  inline std::shared_ptr<TimeLine> timeLine(int id) const {
    auto it = _timeLines.find(id);
    assert(it != _timeLines.end());
    return it->second;
  }

  inline bool hasTimeLine(int id) const {
    return _timeLines.find(id) != _timeLines.end();
  }

  /** All timeline IDs in ascending order. */
  std::vector<int> timeLineIds(void) const {
    std::vector<int> ids;
    ids.reserve(_timeLines.size());
    for (const auto& kv : _timeLines) ids.push_back(kv.first);
    return ids;
  }

  inline int minTimeLineId(void) const {
    assert(!_timeLines.empty());
    return _timeLines.begin()->first;
  }

  inline int maxTimeLineId(void) const {
    assert(!_timeLines.empty());
    return _timeLines.rbegin()->first;
  }

  inline int timeLineCount(void) const {
    return static_cast<int>(_timeLines.size());
  }

  /** All timelines in ascending ID order. */
  inline std::vector<std::shared_ptr<TimeLine>> getTimeLines(void) const {
    std::vector<std::shared_ptr<TimeLine>> result;
    result.reserve(_timeLines.size());
    for (const auto& kv : _timeLines) result.push_back(kv.second);
    return result;
  }

  /**
   * Every (from, to) pair, for every own piece on every moveable board, that getMoveablePositions offers
   * to the side to move. Promotions appear once (makeMove's promotion argument picks the piece).
   */
  std::vector<Move> allPseudoLegalMoves(void) const;

  void undo(void);

  // --- Move history (for records, see include/engine/Notation.h) ------------------------------------------------------

  /** The submitted turns so far, oldest first, each with the moves played in it (undone moves are not included). */
  inline const std::vector<Core::PlayedTurn>& history(void) const { return _history; }
  /** The moves of the pending (unsubmitted) turn. */
  inline const std::vector<Core::PlayedMove>& pendingMoves(void) const { return _pendingMoves; }
  /** The position this game started from as .5dp text (empty for games not built from a Position), and the catalog id of the mode if any. */
  inline const std::string& startPosition(void) const { return _startPosition; }
  inline const std::string& modeId(void) const { return _modeId; }
  inline void setModeId(std::string id) { _modeId = std::move(id); }

  inline PieceColor getCurrentTurnColor(void) const {
    return _currentTurnColor;
  }

  inline const RuleEngine& rule(void) const { return _rule; }
};

} // namespace Chess

template <>
struct std::hash<Chess::Core::Coord> {
  std::size_t operator()(const Chess::Core::Coord& c) const noexcept {
    const std::uint64_t packed = (std::uint64_t(std::uint8_t(c.x)) << 48) | (std::uint64_t(std::uint8_t(c.y)) << 32) |
                                 (std::uint64_t(std::uint16_t(c.t)) << 16) | std::uint64_t(std::uint16_t(c.l));
    return std::hash<std::uint64_t>()(packed);
  }
};

template <>
struct std::hash<Chess::Core::Move> {
  std::size_t operator()(const Chess::Core::Move& m) const noexcept {
    std::size_t h = std::hash<Chess::Core::Coord>()(m.from);
    h ^= std::hash<Chess::Core::Coord>()(m.to) + 0x9e3779b97f4a7c15ull + (h << 6) + (h >> 2);
    return h * 8 + static_cast<std::size_t>(m.promotion);
  }
};
