#pragma once

#include <string>
#include <array>
#include <memory>
#include <vector>
#include <optional>

class Position {
public:
  Position(int x, int y) : _x(x), _y(y) {}

  inline int x() const { return _x; }
  inline int y() const { return _y; }

  inline bool operator == (const Position& other) const {
    return _x == other._x and _y == other._y;
  }
private:
  int _x;
  int _y;
};

template<int N>
class Move {
public:
  inline Position from() const { return _from; }
  inline Position to() const { return _to; }
  inline std::shared_ptr<TimeLine<N>> fromTimeLine() const { return _fromTimeLine; }
  inline std::shared_ptr<TimeLine<N>> toTimeLine() const { return _toTimeLine; }
  inline int targetTurn() const { return _targetTurn; }
  inline std::shared_ptr<Piece> piece() const { return _piece; }
private:
  std::shared_ptr<Piece> _piece;
  Position _from;
  Position _to;
  std::shared_ptr<TimeLine<N>> _fromTimeLine;
  std::shared_ptr<TimeLine<N>> _toTimeLine;
  int _targetTurn;
};

enum class Color : int {
  WHITE = 0,
  BLACK = 1,
};

class Piece {
public:
  Piece(Color color) : _color(color) {}

  Color color() const;

  virtual const std::string& name(void);
  virtual const char& symbol(void);
private:
  Color _color;
};

template<int N>
class Board {
public:
  inline void placePiece(const Position& position, const std::optional<Piece>& piece) {
    _pieces[position.x()][position.y()] = piece;
  }

  inline int turnNumber(void) const {
    return _turnNumber;
  }

  inline void setTurnNumber(int turnNumber) {
    _turnNumber = turnNumber;
  }

  inline void setPreviousBoard(std::shared_ptr<Board<N>> previousBoard) {
    _previousBoard = previousBoard;
  }
private:
  std::array<std::array<std::optional<Piece>, N>, N> _pieces;
  int _turnNumber;
  std::shared_ptr<Board<N>> _previousBoard;
};

template<int N>
class TimeLine {
public:
  TimeLine(int id) : _timeLineNumber(id), _forkAt(-1), _parent(nullptr) {}
  TimeLine(int id, int forkAt, std::shared_ptr<TimeLine<N>> parent)
    : _timeLineNumber(id), _forkAt(forkAt), _parent(parent) {}

  inline int size(void) const {
    return _history.size();
  }

  inline std::shared_ptr<Board<N>> latestBoard(void) const {
    return _history.back();
  }

  inline void pushBoard(const Board<N>& board) {
    _history.push_back(std::make_shared<Board<N>>(board));
  }

  inline void pushBoard(void) {
    Board<N> current = *latestBoard();
    current.setTurnNumber(current.turnNumber() + 1);
    current.setPreviousBoard(latestBoard());
    _history.push_back(std::make_shared<Board<N>>(current));
  }

  inline std::shared_ptr<Board<N>> boardAtTurn(int turn) const {
    return _history[turn];
  }
private:
  int _timeLineNumber;
  int _forkAt;
  std::vector<std::shared_ptr<Board<N>>> _history;
  std::shared_ptr<TimeLine<N>> _parent;
};

template<int N>
class Multiverse {
public:
  Multiverse(void);

  std::shared_ptr<TimeLine<N>> createBranch(
    std::shared_ptr<TimeLine<N>> parent,
    int forkTurn
  );

  void addMoveToQueue(const Move<N>& move);
private:
  std::queue<Move> _moveQueue;
  std::vector<std::shared_ptr<TimeLine<N>>> _timeLines;
  int _present;
};