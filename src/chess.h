#pragma once

#include <string>
#include <array>
#include <memory>
#include <vector>
#include <optional>
#include <queue>
#include <cassert>

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

class Move {
public:
  /**
   * Construct a Move object with the given parameters.
   * @param from The starting position of the move.
   * @param to The target position of the move.
   * @param fromTimeLine The timeline from which the move is made.
   * @param toTimeLine The timeline to which the move is made.
   * @param targetTurn The turn number at which the move is targeted.
   * @param piece The piece being moved.
   */
  Move(
    const Position2D& from,
    const Position2D& to,
    std::shared_ptr<TimeLine> fromTimeLine,
    std::shared_ptr<TimeLine> toTimeLine,
    int targetTurn,
    const std::shared_ptr<Piece>& piece
  ) : _from(from), _to(to), _fromTimeLine(fromTimeLine), _toTimeLine(toTimeLine),
      _targetTurn(targetTurn), _piece(piece) {}

  /**
   * Get the starting position of the move.
   * @return The starting position as a Position2D object.
   * This method returns the position from which the piece is being moved.
   */
  inline Position2D from() const { return _from; }

  /**
   * Get the target position of the move.
   * @return The target position as a Position2D object.
   * This method returns the position to which the piece is being moved.
   */
  inline Position2D to() const { return _to; }

  /**
   * Get the timeline from which the move is made.
   * @return A shared pointer to the TimeLine object representing the timeline.
   * This method returns the timeline from which the piece is being moved.
   */
  inline std::shared_ptr<TimeLine> fromTimeLine() const { return _fromTimeLine; }

  /**
   * Get the timeline to which the move is made.
   * @return A shared pointer to the TimeLine object representing the target timeline.
   * This method returns the timeline to which the piece is being moved.
   */
  inline std::shared_ptr<TimeLine> toTimeLine() const { return _toTimeLine; }

  /**
   * Get the target turn number of the move.
   * @return The target turn number as an integer.
   * This method returns the turn number at which the move is targeted.
   */
  inline int targetTurn() const { return _targetTurn; }

  /**
   * Get the piece being moved.
   * @return The Piece object representing the piece being moved.
   * This method returns the piece that is being moved from the starting position to the target position.
   */
  inline std::shared_ptr<Piece> piece() const { return _piece; }
private:
  Position2D _from;
  Position2D _to;
  std::shared_ptr<TimeLine> _fromTimeLine;
  std::shared_ptr<TimeLine> _toTimeLine;
  int _targetTurn;
  std::shared_ptr<Piece> _piece;
};

enum class Color : int {
  WHITE = 0,
  BLACK = 1,
};

class Piece {
public:
  Piece(Color color) : _color(color) {}

  inline Color color() const {
    return _color;
  }

  virtual const std::string& name(void);
  virtual const char& symbol(void);
private:
  Color _color;
};

class King : public Piece {
public:
  King(Color color) : Piece(color) {}

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
  Queen(Color color) : Piece(color) {}

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
  Rook(Color color) : Piece(color) {}

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
  Bishop(Color color) : Piece(color) {}

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
  Knight(Color color) : Piece(color) {}

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
  Pawn(Color color) : Piece(color) {}

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
  inline void placePiece(const Position2D& position, const std::shared_ptr<Piece>& piece) {
    _pieces[position.x()][position.y()] = piece;
  }

  inline int turnNumber(void) const {
    return _turnNumber;
  }

  inline void setTurnNumber(int turnNumber) {
    _turnNumber = turnNumber;
  }

  inline void setPreviousBoard(std::shared_ptr<Board> previousBoard) {
    _previousBoard = previousBoard;
  }
private:
  std::vector<std::vector<std::shared_ptr<Piece>>> _pieces;
  int _N;
  int _turnNumber;
  std::shared_ptr<Board> _previousBoard;
};

class TimeLine {
public:
  TimeLine(int N, int id) : _N(N), _timeLineNumber(id), _forkAt(-1), _parent(nullptr) {}
  TimeLine(int N, int id, int forkAt, std::shared_ptr<TimeLine> parent)
    : _N(N), _timeLineNumber(id), _forkAt(forkAt), _parent(parent) {}

  inline int id(void) const {
    return _timeLineNumber;
  }

  inline int size(void) const {
    return _history.size();
  }

  inline std::shared_ptr<Board> latestBoard(void) const {
    return _history.back();
  }

  inline void pushBoard(const Board& board) {
    _history.push_back(std::make_shared<Board>(board));
  }

  inline void pushBoard(void) {
    Board current = *latestBoard();
    current.setTurnNumber(current.turnNumber() + 1);
    current.setPreviousBoard(latestBoard());
    _history.push_back(std::make_shared<Board>(current));
  }

  inline std::shared_ptr<Board> boardAtTurn(int turn) const {
    int offset = _history.front()->turnNumber();
    return _history[turn - offset];
  }
private:
  int _N;
  int _timeLineNumber;
  int _forkAt;
  std::vector<std::shared_ptr<Board>> _history;
  std::shared_ptr<TimeLine> _parent;
};

class Multiverse {
public:
  Multiverse(int N) : _N(N), _present(0) {
    auto initialBoard = std::make_shared<Board>();
    initialBoard->setTurnNumber(0);
    auto initialTimeLine = std::make_shared<TimeLine>(0);
    initialTimeLine->pushBoard(*initialBoard);
    _timeLines.push_back(initialTimeLine);
  }

  std::shared_ptr<TimeLine> createBranch(
    std::shared_ptr<TimeLine> parent,
    int forkTurn
  );

  std::vector<Move> validMoves(Position2D position, std::shared_ptr<TimeLine> timeLine) const;

  inline int present(void) const {
    return _present;
  }

  inline Color turnColor(void) const {
    return Color(_present % 2);
  }

  std::vector<std::shared_ptr<TimeLine>> timeLineToMove(void) {
    std::vector<std::shared_ptr<TimeLine>> result;
    for (const auto& timeLine : _timeLines) {
      if (timeLine->id() == _present) {
        result.push_back(timeLine);
      }
    }
    assert(!result.empty() && "No timeline to move in the present");
    return result;
  }

  void addMoveToQueue(const Move& move);
  bool canCommitMove(void) const {
    return true; // Placeholder for actual logic to determine if a move can be committed
  }

  void undoMove(void) {
    // Placeholder for actual logic to undo a move
    if (!_moveQueue.empty()) {
      _moveQueue.pop();
    }
  }
  void commitMove(void) {
    // Placeholder for actual logic to commit a move
    while (!_moveQueue.empty()) {
      _moveQueue.pop();
    }
  }
private:
  int _N;
  int _present;
  std::queue<Move> _moveQueue;
  std::vector<std::shared_ptr<TimeLine>> _timeLines;
};