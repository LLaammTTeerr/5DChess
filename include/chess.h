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

namespace Chess {

using u8 = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;

class Vector4D {
public:
  Vector4D(int x, int y, int z, int w);

  inline int x(void) const;
  inline int y(void) const;
  inline int z(void) const;
  inline int w(void) const;

  inline int operator * (const Vector4D& other) const;

  inline int operator - (const Vector4D& other) const;
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
  virtual const std::string& name(void) = 0;

  /**
   * Get the symbol of the piece.
   * @return The symbol of the piece as a character.
   * This method returns a single character that represents the piece, such as 'K' for king, 'Q' for queen, etc.
   */
  virtual const char& symbol(void) = 0;

  /**
   * Get the board this piece is on.
   * @return A shared pointer to the Board object this piece is on.
   * This method returns the board that this piece is currently placed on.
   * If the piece is not on any board, it returns a null pointer.
   */
  inline std::shared_ptr<Board> getBoard() const { return _board; }

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
protected:
  PieceColor _color;
  std::shared_ptr<Board> _board;
  Position2D _position;
};

class King : public Piece {
public:
  King(PieceColor color, std::shared_ptr<Board> board, Position2D position);

  inline const std::string& name(void) override {
    static const std::string name = "king";
    return name;
  }

  inline const char& symbol(void) override {
    static const char symbol = 'K';
    return symbol;
  }
};

class Queen : public Piece {
public:
  Queen(PieceColor color, std::shared_ptr<Board> board, Position2D position);

  inline const std::string& name(void) override {
    static const std::string name = "queen";
    return name;
  }

  inline const char& symbol(void) override {
    static const char symbol = 'Q';
    return symbol;
  }
};

class Rook : public Piece {
public:
  Rook(PieceColor color, std::shared_ptr<Board> board, Position2D position);

  inline const std::string& name(void) override {
    static const std::string name = "rook";
    return name;
  }

  inline const char& symbol(void) override {
    static const char symbol = 'R';
    return symbol;
  }
};

class Bishop : public Piece {
public:
  Bishop(PieceColor color, std::shared_ptr<Board> board, Position2D position);

  inline const std::string& name(void) override {
    static const std::string name = "bishop";
    return name;
  }

  inline const char& symbol(void) override {
    static const char symbol = 'B';
    return symbol;
  }
};

class Knight : public Piece {
public:
  Knight(PieceColor color, std::shared_ptr<Board> board, Position2D position);

  inline const std::string& name(void) override {
    static const std::string name = "knight";
    return name;
  }

  inline const char& symbol(void) override {
    static const char symbol = 'N';
    return symbol;
  }
};

class Pawn : public Piece {
public:
  Pawn(PieceColor color, std::shared_ptr<Board> board = nullptr, Position2D position = Position2D(-1, -1));

  inline const std::string& name(void) override {
    static const std::string name = "pawn";
    return name;
  }

  inline const char& symbol(void) override {
    static const char symbol = 'P';
    return symbol;
  }
};

class Board {
public:
  Board(int N, std::shared_ptr<TimeLine> timeLine);

  /**
   * Get the dimension of the board.
   * @return The dimension of the board as an integer.
   * This method returns the size of the board (N).
   */
  inline int dim(void) const;

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
  std::shared_ptr<const Piece> getPiece(Position2D position) const;

  std::shared_ptr<TimeLine> getTimeLine() const;

private:
  int _N;
  int _fullTurnNumber;
  int _halfTurnNumber;
  std::shared_ptr<Board> _previousBoard;
  std::vector<std::vector<std::shared_ptr<Piece>>> _pieces;
  std::shared_ptr<TimeLine> _timeLine; // The timeline this board belongs to
};

class TimeLine {
public:
  TimeLine(int N, int ID = 0);

  /**
   * Get the ID of the timeline.
   * @return The ID of the timeline as an integer.
   */
  inline int ID(void) const;

  /**
   * Get the dimension of the timeline.
   * @return The dimension of the timeline as an integer.
   * This method returns the size of the board (N).
   */
  inline int dim(void) const;

  /**
   * Get the fork point of the timeline.
   * @return The fork point of the timeline as an integer.
   */
  inline int forkAt(void) const;

  /**
   * Get the full turn number of the timeline.
   * @return The full turn number of the timeline as an integer.
   */
  inline int fullTurnNumber(void) const;

  /**
   * Get the half turn number of the timeline.
   * @return The half turn number of the timeline as an integer.
   */
  inline int halfTurnNumber(void) const;

  /**
   * Get the parent timeline of the current timeline.
   * @return A shared pointer to the parent timeline, or nullptr if there is no parent.
   */
  inline std::shared_ptr<const TimeLine> parent(void) const;

  /**
   * Push a new board state onto the timeline.
   * @param board The board state to be added to the timeline.
   */
  void pushBack(std::shared_ptr<Board> board);

  inline std::shared_ptr<Board> back(void) {
    assert(!_history.empty());
    return _history.back();
  }
private:
  int _N;
  int _ID;
  int _forkAt;
  int _fullTurnNumber;
  int _halfTurnNumber;
  std::vector<std::shared_ptr<Board>> _history;
  std::shared_ptr<TimeLine> _parent;
};

// class Move {
// public:
//   Position2D _from, _to;
//   int _fromTimeLineID, _toTimeLineID;
//   int _fromTurn, _toTurn;
//   std::shared_ptr<Piece> _piece;
// };

/// Turn management and state handling
enum class MovePhase {
  SELECT_FROM_BOARD,
  SELECT_FROM_POSITION,
  SELECT_TO_BOARD,
  SELECT_TO_POSITION,
};

struct SelectedPosition {
  std::shared_ptr<Board> board; // Board where the position is selected
  Position2D position;  // 2D position on the board (e.g., chess coordinates as float for rendering)

  SelectedPosition() : board(nullptr), position(Position2D(-1, -1)) {} // Default constructor
};

// Represents a move in the game, including the source and destination positions
class Move {
// public for easy access in TurnState
public:
  SelectedPosition from;
  SelectedPosition to;
  // Additional fields can be added, e.g., piece type, but keeping simple for now
};

class Game {
public:
  /**
   * Constructor for the Game class.
   * @param N The size of the board.
   * @param board The initial board configuration.
   * Please note that this constructor transfers ownership of the board to the game.
   */
  Game(int N, std::shared_ptr<Board> board);

  /**
   * Constructor for the Game class.
   * @param builderFunction A function that builds the initial board configuration, check out the BoardBuilder class
   */
  Game(std::function<std::shared_ptr<Board>(void)> builderFunction);

  /**
   * Get the dimension of the game.
   * @return The dimension of the game as an integer.
   * This method returns the size of the board (N).
   */
  inline int dim(void) const;

  /**
   * Get the current full turn number.
   * @return The current full turn number as an integer.
   */
  inline int presentFullTurn(void) const;

  /**
   * Get the current half turn number.
   * @return The current half turn number as an integer.
   */
  inline int presentHalfTurn(void) const;

  /**
   * Apply the turn state to the game.
   * @param turnState The TurnState object contain  // Apply the completed turn
  // One turn might consist of multiple moves
  // This method applies all moves in the current turn to the game stateing the moves to apply.
   */
  void applyTurn(void);

  /**
   * Get the board where the current player can make moves.
   * @return A vector of shared pointers to the boards where moves can be made.
   * This method returns a vector of boards that are currently available for making moves.
   * It can be used to determine which boards are active for the current turn.
   */
  std::vector<std::shared_ptr<Board>> getMoveableBoards(void) const;
  std::vector<SelectedPosition> getMoveablePositions(SelectedPosition selected) const;
  void makeMove(const Move& move);
  bool undoable(void) const;
private:
  int _N;
  int _presentFullTurn;
  int _presentHalfTurn;
  std::vector<std::shared_ptr<TimeLine>> _timeLines;
  std::vector<Move> _currentTurnMoves;
};

class Constant {
public:
  static const int BOARD_SIZE;
};

class BoardBuilder {
public:
  /**
   * Build a standard chess board with pieces in their initial positions.
   * @return A shared pointer to the constructed Board object.
   * This method initializes a standard chess board with all pieces placed in their starting positions.
   */
  static std::shared_ptr<Board> buildStandardBoard();
};



} // namespace Chess
