#pragma once

#include <string>
#include <array>
#include <memory>
#include <vector>
#include <optional>
#include <queue>
#include <cassert>
#include <functional>
#include <cstdint>
#include <iostream>
#include <climits>

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

  inline int operator * (const Vector4D& other) const {
    return _data[0] * other._data[0] + _data[1] * other._data[1] +
           _data[2] * other._data[2] + _data[3] * other._data[3];
  }

  inline int operator - (const Vector4D& other) const {
    return _data[0] - other._data[0] + _data[1] - other._data[1] +
           _data[2] - other._data[2] + _data[3] - other._data[3];
  }
private:
  std::array<int, 4> _data;
};

enum class PieceColor : int {
  PIECEWHITE = 0,
  PIECEBLACK = 1,
};

inline constexpr PieceColor opposite(PieceColor c) {
  return c == PieceColor::PIECEWHITE ? PieceColor::PIECEBLACK : PieceColor::PIECEWHITE;
}

class Position2D;
class Piece;
class Board;
class TimeLine;
class Multiverse;
class Move;

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

class Piece {
public:
  Piece(PieceColor color, std::shared_ptr<Board> board = nullptr, Position2D position = Position2D(-1, -1));
  virtual ~Piece() = default;

  virtual std::shared_ptr<Piece> clone() const = 0;

  /**
   * Get the color of the piece.
   * @return The color of the piece as a PieceColor enum.
   * This method returns the color of the piece, which can be either white or black.
   */
  inline PieceColor color() const {
    return _color;
  }

  /**
   * Get the name of the piece.
   * @return The name of the piece as a string.
   * This method returns the name of the piece, such as "king", "queen", etc.
   */
  virtual const std::string& name(void) const = 0;

  /**
   * Get the symbol of the piece.
   * @return The symbol of the piece as a character.
   * This method returns a single character that represents the piece, such as 'K' for king, 'Q' for queen, etc.
   */
  virtual const char& symbol(void) const = 0;

  /**
   * Get the board this piece is on.
   * @return A shared pointer to the Board object this piece is on.
   * This method returns the board that this piece is currently placed on.
   * If the piece is not on any board, it returns a null pointer.
   */
  inline std::shared_ptr<Board> getBoard(void) const { return _board; }

  /**
   * Get the position of this piece on the board.
   * @return The Position2D object representing the piece's position.
   * This method returns the current position of the piece on the board.
   * The position is represented as a Position2D object, which contains x and y coordinates
   * indicating the piece's location on the board.
   * If the piece is not placed on a board, the position may be invalid (e.g., (-1, -1)).
   * @note The position is typically zero-indexed, meaning (0, 0)
   * represents the top-left corner of the board.
   */
  inline Position2D getPosition() const { return _position; }

  inline void setPosition(Position2D position) {
    _position = position;
  }

  inline void setBoard(std::shared_ptr<Board> board) {
    _board = board;
  }
protected:
  PieceColor _color;
  std::shared_ptr<Board> _board;
  Position2D _position;
};

class King : public Piece {
public:
  King(PieceColor color, std::shared_ptr<Board> board = nullptr, Position2D position = Position2D(-1, -1))
    : Piece(color, board, position) {};

  virtual std::shared_ptr<Piece> clone() const override {
    return std::make_shared<King>(*this);
  }

  inline const std::string& name(void) const override {
    static const std::string name = "king";
    return name;
  }

  inline const char& symbol(void) const override {
    static const char symbol = 'K';
    return symbol;
  }
};

class Queen : public Piece {
public:
  Queen(PieceColor color, std::shared_ptr<Board> board = nullptr, Position2D position = Position2D(-1, -1))
    : Piece(color, board, position) {};

  virtual std::shared_ptr<Piece> clone() const override {
    return std::make_shared<Queen>(*this);
  }

  inline const std::string& name(void) const override {
    static const std::string name = "queen";
    return name;
  }

  inline const char& symbol(void) const override {
    static const char symbol = 'Q';
    return symbol;
  }
};

class Rook : public Piece {
public:
  Rook(PieceColor color, std::shared_ptr<Board> board = nullptr, Position2D position = Position2D(-1, -1))
    : Piece(color, board, position) {};

  virtual std::shared_ptr<Piece> clone() const override {
    return std::make_shared<Rook>(*this);
  }

  inline const std::string& name(void) const override {
    static const std::string name = "rook";
    return name;
  }

  inline const char& symbol(void) const override {
    static const char symbol = 'R';
    return symbol;
  }
};

class Bishop : public Piece {
public:
  Bishop(PieceColor color, std::shared_ptr<Board> board = nullptr, Position2D position = Position2D(-1, -1))
    : Piece(color, board, position) {};

  virtual std::shared_ptr<Piece> clone() const override {
    return std::make_shared<Bishop>(*this);
  }

  inline const std::string& name(void) const override {
    static const std::string name = "bishop";
    return name;
  }

  inline const char& symbol(void) const override {
    static const char symbol = 'B';
    return symbol;
  }
};

class Knight : public Piece {
public:
  Knight(PieceColor color, std::shared_ptr<Board> board = nullptr, Position2D position = Position2D(-1, -1))
    : Piece(color, board, position) {};

  virtual std::shared_ptr<Piece> clone() const override {
    return std::make_shared<Knight>(*this);
  }

  inline const std::string& name(void) const override {
    static const std::string name = "knight";
    return name;
  }

  inline const char& symbol(void) const override {
    static const char symbol = 'N';
    return symbol;
  }
};

class Pawn : public Piece {
public:
  Pawn(PieceColor color, std::shared_ptr<Board> board = nullptr, Position2D position = Position2D(-1, -1))
    : Piece(color, board, position) {};

  virtual std::shared_ptr<Piece> clone() const override {
    return std::make_shared<Pawn>(*this);
  }

  inline const std::string& name(void) const override {
    static const std::string name = "pawn";
    return name;
  }

  inline const char& symbol(void) const override {
    static const char symbol = 'P';
    return symbol;
  }
};

class Board : public std::enable_shared_from_this<Board> {
public:
  Board(int N, std::shared_ptr<TimeLine> timeLine);

  /**
   * Get the dimension of the board.
   * @return The dimension of the board as an integer.
   * This method returns the size of the board (N).
   */
  inline int dim(void) const {
    return _N;
  }

  /**
   * Place a piece on the board.
   * @param position The position on the board where the piece will be placed.
   * @param piece The piece to be placed on the board.
   * This method asserts that the position is within the bounds of the board
   * Please note that this method transfers ownership of the piece to the board.
   */
  void placePiece(Position2D position, std::shared_ptr<Piece> piece);

  /**
   * Get a piece from the board.
   * @param position The position on the board from which to retrieve the piece.
   * @return A shared pointer to the Piece object at the specified position, or nullptr if no piece is present.
   */
  std::shared_ptr<Piece> getPiece(Position2D position) const;

  /**
   * Get the timeline this board belongs to.
   * @return A shared pointer to the TimeLine object associated with this board.
   */
  std::shared_ptr<TimeLine> getTimeLine() const;

  /**
   * Get the full turn number of the board.
   * @return The full turn number as an integer.
   */
  inline int fullTurnNumber(void) const { return _halfTurnNumber / 2; }

  /**
   * Get the half turn number of the board.
   * @return The half turn number as an integer.
   */
  inline int halfTurnNumber(void) const { return _halfTurnNumber; }

  std::shared_ptr<Board> createFork(std::shared_ptr<TimeLine> timeLine);
private:
  int _N;
  int _halfTurnNumber;
  std::shared_ptr<Board> _previousBoard;
  std::vector<std::vector<std::shared_ptr<Piece>>> _pieces;
  std::shared_ptr<TimeLine> _timeLine; // The timeline this board belongs to
};

class TimeLine : public std::enable_shared_from_this<TimeLine> {
public:
  TimeLine(int N, int IDX = 0);

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
   * Get the parent timeline of the current timeline.
   * @return A shared pointer to the parent timeline, or nullptr if there is no parent.
   */
  inline std::shared_ptr<const TimeLine> parent(void) const {
    return _parent;
  }

  inline int size(void) const {
    return _history.size();
  }

  /**
   * Push a new board state onto the timeline.
   * @param board The board state to be added to the timeline.
   */
  void pushBack(std::shared_ptr<Board> board);

  inline std::shared_ptr<Board> back(void) {
    assert(!_history.empty());
    return _history.back();
  }

  inline std::shared_ptr<Board> getBoardByHalfTurn(int halfTurn) const {
    int pos = halfTurn - _forkAt - 1;
    assert(pos >= 0 && pos < _history.size());
    return _history[pos];
  }

  std::shared_ptr<TimeLine> createFork(int newID, int forkAt) {
    std::shared_ptr<TimeLine> forkedTimeLine = std::make_shared<TimeLine>(_N, newID);
    forkedTimeLine->_forkAt = forkAt;
    forkedTimeLine->_parent = shared_from_this();
    return forkedTimeLine;
  }
private:
  int _N;
  int _ID;
  int _forkAt;
  std::vector<std::shared_ptr<Board>> _history;
  std::shared_ptr<TimeLine> _parent;
public:
  std::vector<std::shared_ptr<Board>> getBoards() const { return _history; }
};

struct SelectedPosition {
  std::shared_ptr<Board> board; // Board where the position is selected
  Position2D position;  // 2D position on the board (e.g., chess coordinates as float for rendering)

  SelectedPosition() : board(nullptr), position(Position2D(-1, -1)) {} // Default constructor
  SelectedPosition(std::shared_ptr<Board> b, Position2D pos) : board(b), position(pos) {}

  inline Vector4D toVector4D(void) const {
    return Vector4D(position.x(), position.y(), board->fullTurnNumber(), board->getTimeLine()->ID());
  }
};

// Represents a move in the game, including the source and destination positions
class Move {
// public for easy access in TurnState
public:
  SelectedPosition from;
  SelectedPosition to;
  // Additional fields can be added, e.g., piece type, but keeping simple for now
};

class IGame {
public:
  IGame(int N) : _N(N), _presentHalfTurn(0), _currentTurnColor(PieceColor::PIECEWHITE), _nextHalfTurn(INT_MAX) {}
  virtual ~IGame() = default;

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
   * Apply the turn state to the game.
   * @param turnState The TurnState object contain  // Apply the completed turn
  // One turn might consist of multiple moves
  // This method applies all moves in the current turn to the game stateing the moves to apply.
   */
  void submitTurn(void);

  /**
   * Get the board where the current player can make moves.
   * @return A vector of shared pointers to the boards where moves can be made.
   * This method returns a vector of boards that are currently available for making moves.
   * It can be used to determine which boards are active for the current turn.
   */
  std::vector<std::shared_ptr<Board>> getMoveableBoards(void) const;

  /**
   * Get the positions where the current player can make moves.
   * @param selected The selected position to check for moveable positions.
   * @return A vector of SelectedPosition objects representing the moveable positions.
   * This method returns a vector of positions that are available for making moves based on the selected position.
   */
  std::vector<SelectedPosition> getMoveablePositions(SelectedPosition selected) const;

  void makeMove(Move move);

  /**
   * Check if the current turn can be undone.
   * @return True if the current turn can be undone, false otherwise.
   * This method checks if there are any moves in the current turn that can be undone.
   */
  bool undoable(void) const {
    return !_currentTurnMoves.empty();
  }

  bool canMakeMoveFromBoard(std::shared_ptr<Board> board) const;

  bool boardExists(int timeLineID, int halfTurn) const {
    if (timeLineID >= 0 && timeLineID < _timeLines.size()) {
      int pos = halfTurn - _timeLines[timeLineID]->forkAt() - 1;
      return pos >= 0 && pos < _timeLines[timeLineID]->size();
    }
    return false;
  }

  std::shared_ptr<Board> getBoard(int timeLineID, int halfTurn) const {
    return _timeLines[timeLineID]->getBoardByHalfTurn(halfTurn);
  }
protected:
  int _N;
  int _presentHalfTurn;
  int _nextHalfTurn;
  std::vector<std::shared_ptr<TimeLine>> _timeLines;
  std::vector<Move> _currentTurnMoves;
  PieceColor _currentTurnColor;

  std::shared_ptr<Piece> _getPieceByVector4DFullTurn(Vector4D position) const {
    int x = position.x();
    int y = position.y();
    int halfTurn = 2 * position.z() + int(_currentTurnColor);
    int timeLineID = position.w();
    assert(timeLineID >= 0 && timeLineID < _timeLines.size());
    std::shared_ptr<const Board> board = _timeLines[timeLineID]->getBoardByHalfTurn(halfTurn);
    assert(board != nullptr);
    assert(x >= 0 && x < board->dim() && y >= 0 && y < board->dim());
    return board->getPiece(Position2D(x, y));
  }


  void _pushBack(std::shared_ptr<TimeLine> timeLine) {
    _timeLines.push_back(timeLine);
  }
public:
  inline std::vector<std::shared_ptr<TimeLine>> getTimeLines(void) const {
    return _timeLines;
  }

  inline PieceColor getCurrentTurnColor(void) const {
    return _currentTurnColor;
  }
};

class Constant {
public:
  static const int BOARD_SIZE;
  static const int BOARD_SIZE_EMIT_BISHOP;
  static const int BOARD_SIZE_EMIT_KNIGHT;
  static const int BOARD_SIZE_EMIT_QUEEN;
};

class StandardGame : public IGame {
  StandardGame(void) : IGame(Constant::BOARD_SIZE) {
    _timeLines.push_back(std::make_shared<TimeLine>(dim()));
    std::shared_ptr<Board> board = std::make_shared<Board>(dim(), _timeLines[0]);
    for (int i = 0; i < dim(); i += 1) {
      board->placePiece({i, 1}, std::make_shared<Pawn>(PieceColor::PIECEWHITE));
      board->placePiece({i, 6}, std::make_shared<Pawn>(PieceColor::PIECEBLACK));
    }
    board->placePiece({0, 0}, std::make_shared<Rook>(PieceColor::PIECEWHITE));
    board->placePiece({1, 0}, std::make_shared<Knight>(PieceColor::PIECEWHITE));
    board->placePiece({2, 0}, std::make_shared<Bishop>(PieceColor::PIECEWHITE));
    board->placePiece({3, 0}, std::make_shared<Queen>(PieceColor::PIECEWHITE));
    board->placePiece({4, 0}, std::make_shared<King>(PieceColor::PIECEWHITE));
    board->placePiece({5, 0}, std::make_shared<Bishop>(PieceColor::PIECEWHITE));
    board->placePiece({6, 0}, std::make_shared<Knight>(PieceColor::PIECEWHITE));
    board->placePiece({7, 0}, std::make_shared<Rook>(PieceColor::PIECEWHITE));

    board->placePiece({0, 7}, std::make_shared<Rook>(PieceColor::PIECEBLACK));
    board->placePiece({1, 7}, std::make_shared<Knight>(PieceColor::PIECEBLACK));
    board->placePiece({2, 7}, std::make_shared<Bishop>(PieceColor::PIECEBLACK));
    board->placePiece({3, 7}, std::make_shared<Queen>(PieceColor::PIECEBLACK));
    board->placePiece({4, 7}, std::make_shared<King>(PieceColor::PIECEBLACK));
    board->placePiece({5, 7}, std::make_shared<Bishop>(PieceColor::PIECEBLACK));
    board->placePiece({6, 7}, std::make_shared<Knight>(PieceColor::PIECEBLACK));
    board->placePiece({7, 7}, std::make_shared<Rook>(PieceColor::PIECEBLACK));
    _timeLines[0]->pushBack(board);
  }
};

class CustomGameEmitBishop : public IGame {
public:
  CustomGameEmitBishop(void) : IGame(Constant::BOARD_SIZE_EMIT_BISHOP) {
    _timeLines.push_back(std::make_shared<TimeLine>(dim()));
    std::shared_ptr<Board> board = std::make_shared<Board>(dim(), _timeLines[0]);
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
    _timeLines[0]->pushBack(board);
  }
};

class CustomGameEmitKnight : public IGame {
public:
  CustomGameEmitKnight(void) : IGame(Constant::BOARD_SIZE_EMIT_KNIGHT) {
    _timeLines.push_back(std::make_shared<TimeLine>(dim()));
    std::shared_ptr<Board> board = std::make_shared<Board>(dim(), _timeLines[0]);
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
    _timeLines[0]->pushBack(board);
  }
};

class CustomGameEmitQueen : public IGame {
public:
  CustomGameEmitQueen(void) : IGame(Constant::BOARD_SIZE_EMIT_QUEEN) {
    _timeLines.push_back(std::make_shared<TimeLine>(dim()));
    std::shared_ptr<Board> board = std::make_shared<Board>(dim(), _timeLines[0]);
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
    _timeLines[0]->pushBack(board);
  }
};

template<class T>
std::shared_ptr<IGame> createGame(void) {
  static_assert(std::is_base_of<IGame, T>::value, "T must derive from IGame");
  return std::make_shared<T>();
}

} // namespace Chess
