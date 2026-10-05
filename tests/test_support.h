#pragma once

#include "chess.h"

#include <memory>
#include <random>
#include <string>
#include <vector>

namespace test {

using namespace Chess;

// A game with an empty position: every timeline starts with one empty board at half-turn 0.
// Lets tests place arbitrary pieces through IGame's protected state.
class Sandbox : public IGame {
public:
  explicit Sandbox(int n, int timeLines = 1) : IGame(n) {
    for (int id = 0; id < timeLines; ++id) {
      _timeLines.push_back(std::make_shared<TimeLine>(n, id));
      _timeLines[id]->pushBack(std::make_shared<Board>(n, _timeLines[id]));
    }
  }

  // Timeline i holds boardCounts[i] empty boards (half-turns 0..count-1); the present is set explicitly.
  Sandbox(int n, const std::vector<int>& boardCounts, int presentHalfTurn) : IGame(n) {
    for (int id = 0; id < static_cast<int>(boardCounts.size()); ++id) {
      _timeLines.push_back(std::make_shared<TimeLine>(n, id));
      for (int h = 0; h < boardCounts[id]; ++h) {
        _timeLines[id]->pushBack(std::make_shared<Board>(n, _timeLines[id], h));
      }
    }
    _presentHalfTurn = presentHalfTurn;
  }

  std::shared_ptr<Board> boardAt(int timeLineID, int halfTurn) const {
    return _timeLines[timeLineID]->getBoardByHalfTurn(halfTurn);
  }

  std::shared_ptr<Board> tip(int timeLineID) const { return _timeLines[timeLineID]->back(); }

  void place(int timeLineID, int x, int y, std::shared_ptr<Piece> piece) {
    tip(timeLineID)->placePiece({x, y}, std::move(piece));
  }
};

template <class P>
std::shared_ptr<Piece> make(PieceColor color) { return std::make_shared<P>(color); }

inline std::vector<SelectedPosition> movesAt(const IGame& game, std::shared_ptr<Board> board, int x, int y) {
  return game.getMoveablePositions(SelectedPosition(board, Position2D(x, y)));
}

inline bool contains(const std::vector<SelectedPosition>& moves, const std::shared_ptr<Board>& board, int x, int y) {
  for (const auto& m : moves) {
    if (m.board == board && m.position == Position2D(x, y)) return true;
  }
  return false;
}

inline std::string key(const SelectedPosition& p) {
  return std::to_string(p.board->getTimeLine()->ID()) + ":" + std::to_string(p.board->halfTurnNumber()) + ":" +
         std::to_string(p.position.x()) + "," + std::to_string(p.position.y());
}

// Full textual dump of every board in every timeline plus turn bookkeeping.
inline std::string snapshot(const IGame& game) {
  std::string out = "present=" + std::to_string(game.presentHalfTurn()) +
                    " color=" + std::to_string(int(game.getCurrentTurnColor())) +
                    " end=" + std::to_string(game.gameEnd()) + " undoable=" + std::to_string(game.undoable()) + "\n";
  for (const auto& timeLine : game.getTimeLines()) {
    out += "T" + std::to_string(timeLine->ID()) + " fork=" + std::to_string(timeLine->forkAt()) + "\n";
    for (const auto& board : timeLine->getBoards()) {
      out += " h" + std::to_string(board->halfTurnNumber()) + " ";
      for (int y = 0; y < board->dim(); ++y) {
        for (int x = 0; x < board->dim(); ++x) {
          auto piece = board->getPiece({x, y});
          char c = piece ? piece->symbol() : '.';
          if (piece && piece->color() == PieceColor::PIECEBLACK) c = char(c - 'A' + 'a');
          out += c;
        }
        out += '/';
      }
      out += "\n";
    }
  }
  return out;
}

// Every legal (from, to) pair available to the side to move.
inline std::vector<Move> allLegalMoves(const IGame& game) {
  std::vector<Move> result;
  for (const auto& board : game.getMoveableBoards()) {
    for (int x = 0; x < board->dim(); ++x) {
      for (int y = 0; y < board->dim(); ++y) {
        auto piece = board->getPiece({x, y});
        if (!piece || piece->color() != game.getCurrentTurnColor()) continue;
        SelectedPosition from(board, Position2D(x, y));
        for (const auto& to : game.getMoveablePositions(from)) {
          result.push_back(Move{from, to});
        }
      }
    }
  }
  return result;
}

} // namespace test
