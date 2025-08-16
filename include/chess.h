#pragma once

#include <string>
#include <array>
#include <memory>
#include <vector>
#include <optional>
#include <queue>
#include <cassert>
#include <functional>

namespace Chess {

using u8 = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;

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

  inline PieceColor color() const {
    return _color;
  }

  virtual const std::string& name(void) = 0;
  virtual const char& symbol(void) = 0;
  
  std::shared_ptr<Board> getBoard() const { return _board; }
  Position2D getPosition() const { return _position; }

protected:
  PieceColor _color;
  std::shared_ptr<Board> _board; // The board this piece belongs to
  Position2D _position; // The position of the piece on the board
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

  std::shared_ptr<const TimeLine> makeMove(Move move);
private:
  int _N;
  int _presentFullTurn;
  int _presentHalfTurn;
  std::vector<std::shared_ptr<TimeLine>> _timeLines;
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
  static std::shared_ptr<Board> buildStandardBoard(std::shared_ptr<TimeLine> timeLine);
};


/// Turn management and state handling
enum class TurnPhase {
    SELECT_BOARD,
    SELECT_PIECE,
    SELECT_DESTINATION,
    CONFIRM_TURN
};

struct SelectedPosition {
    std::shared_ptr<Board> board; // Board where the position is selected
    Position2D position;  // 2D position on the board (e.g., chess coordinates as float for rendering)

    SelectedPosition() : board(nullptr), position(Position2D(-1, -1)) {} // Default constructor
};

struct Move {
    SelectedPosition from;
    SelectedPosition to;
    // Additional fields can be added, e.g., piece type, but keeping simple for now
};

class TurnState {
private:
    TurnPhase currentPhase;
    std::vector<Move> currentTurnMoves;  // Accumulates multiple moves in one turn
    SelectedPosition currentSelection;   // Tracks ongoing selection (board/piece/dest)
    bool allowMultipleMoves;             // Flag to indicate if multiple moves are possible in this turn

public:
    TurnState();

    // Reset the turn state for a new turn
    void resetTurn();

    // Advance to the next phase or handle input (placeholder for integration with input handling)
    void updatePhase(TurnPhase newPhase);

    // Add a completed move to the turn (called when destination is selected)
    void addMove(const SelectedPosition& from, const SelectedPosition& to);

    // Check if the turn can include more moves (based on game rules, placeholder logic)
    bool canAddMoreMoves() const;

    // Render the current state overlays/UI using Raylib (assumes camera/board rendering is handled externally)
    // void render() const;

    // Getters
    TurnPhase getCurrentPhase() const;
    const std::vector<Move>& getCurrentTurnMoves() const;
private:
    void updateAllowedMoves(); // Update allowed moves based on game rules
};

} // namespace Chess