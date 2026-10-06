#pragma once

#include "chess.h"

#include <memory>
#include <cstdint>
#include <cstdlib>
#include <random>
#include <string>
#include <vector>

namespace test {

// Portable RNG helper: std::*_distribution output is implementation-defined (libstdc++ vs libc++ vs MSVC differ), but
// std::mt19937's raw output is fixed by the standard, so derive values from it directly to play identical games everywhere.
inline int randInt(std::mt19937& rng, int lo, int hi) { return lo + int(rng() % std::uint32_t(hi - lo + 1)); }

using namespace Chess;

// A game with an empty position: every timeline starts with one empty board at half-turn 0.
// Lets tests place arbitrary pieces through IGame's protected state.
class Sandbox : public IGame {
public:
  explicit Sandbox(int n, int timeLines = 1) : IGame(n) {
    for (int id = 0; id < timeLines; ++id) {
      _addTimeLine(std::make_shared<TimeLine>(n, id))->pushBack(std::make_shared<Board>(n, id));
    }
  }

  // Timeline i holds boardCounts[i] empty boards (half-turns 0..count-1); the present is set explicitly.
  Sandbox(int n, const std::vector<int>& boardCounts, int presentHalfTurn) : IGame(n) {
    for (int id = 0; id < static_cast<int>(boardCounts.size()); ++id) {
      auto timeLine = _addTimeLine(std::make_shared<TimeLine>(n, id));
      for (int h = 0; h < boardCounts[id]; ++h) {
        timeLine->pushBack(std::make_shared<Board>(n, id, h));
      }
    }
    _presentHalfTurn = presentHalfTurn;
  }

  // Adds a timeline as if a player had created it during the game (counts as created by White when above the
  // original IDs and by Black when below). It holds `count` empty boards (half-turns 0..count-1).
  void addCreatedTimeLine(int id, int count) {
    _setupDone = true;
    auto line = _addTimeLine(std::make_shared<TimeLine>(dim(), id));
    for (int h = 0; h < count; ++h) line->pushBack(std::make_shared<Board>(dim(), id, h));
  }

  void setTurnColor(PieceColor color) { _currentTurnColor = color; }

  std::shared_ptr<Board> boardAt(int timeLineID, int halfTurn) const {
    return timeLine(timeLineID)->getBoardByHalfTurn(halfTurn);
  }

  std::shared_ptr<Board> tip(int timeLineID) const { return timeLine(timeLineID)->back(); }

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
  return std::to_string(p.board->timeLineId()) + ":" + std::to_string(p.board->halfTurnNumber()) + ":" +
         std::to_string(p.position.x()) + "," + std::to_string(p.position.y());
}

// Full textual dump of every board in every timeline plus turn bookkeeping.
inline std::string snapshot(const IGame& game) {
  std::string out = "present=" + std::to_string(game.presentHalfTurn()) +
                    " color=" + std::to_string(int(game.getCurrentTurnColor())) +
                    " result=" + std::to_string(int(game.result())) + " undoable=" + std::to_string(game.undoable()) + "\n";
  for (const auto& timeLine : game.getTimeLines()) {
    out += "T" + std::to_string(timeLine->ID()) + " fork=" + std::to_string(timeLine->forkAt()) +
           " parent=" + std::to_string(timeLine->parentId()) + "\n";
    for (const auto& board : timeLine->getBoards()) {
      out += " h" + std::to_string(board->halfTurnNumber()) + " ";
      for (int y = 0; y < board->dim(); ++y) {
        for (int x = 0; x < board->dim(); ++x) {
          auto piece = board->getPiece({x, y});
          char c = piece ? piece->symbol() : '.';
          if (piece && piece->color() == PieceColor::PIECEBLACK) c = char(c - 'A' + 'a');
          out += c;
          if (piece && piece->unmoved()) out += '\'';
        }
        out += '/';
      }
      out += "\n";
    }
  }
  return out;
}

// Builds a random legal turn (moves on random moveable boards, undoing dead ends) and leaves it pending.
// Returns false if no legal turn was found within a few attempts. `onMove` sees every move that is made.
template <class F>
bool buildRandomTurn(IGame& game, std::mt19937& rng, F&& onMove) {
  for (int attempt = 0; attempt < 12; ++attempt) {
    for (int step = 0; step < 10; ++step) {
      if (game.canSubmit() and test::randInt(rng, 0, 2) != 0) return true;
      auto moves = game.allPseudoLegalMoves();
      if (moves.empty()) break;
      // Pawn moves and castling are favoured, so that double steps, en passant, promotion and castling occur in short games.
      std::vector<int> weight(moves.size(), 1);
      int total = 0;
      for (std::size_t i = 0; i < moves.size(); ++i) {
        auto piece = moves[i].from.board->getPiece(moves[i].from.position);
        if (piece->type() == PieceType::Pawn) {
          weight[i] = 4;
          const bool sameBoard = moves[i].to.board == moves[i].from.board;
          if (sameBoard and moves[i].to.position.x() != moves[i].from.position.x()
              and moves[i].to.board->getPiece(moves[i].to.position) == nullptr)
            weight[i] = 60; // en passant, when it is on offer
          if (moves[i].to.position.y() == (piece->color() == PieceColor::PIECEWHITE ? game.dim() - 1 : 0)) weight[i] = 40; // promotion
        }
        if (piece->type() == PieceType::King and moves[i].to.board == moves[i].from.board
            and std::abs(moves[i].to.position.x() - moves[i].from.position.x()) == 2) weight[i] = 12;
        total += weight[i];
      }
      int pick = test::randInt(rng, 0, total - 1);
      std::size_t idx = 0;
      while (pick >= weight[idx]) pick -= weight[idx++];
      Move move = moves[idx];
      onMove(game, move);
      static const PieceType promos[] = {PieceType::Queen, PieceType::Rook, PieceType::Bishop, PieceType::Knight};
      game.makeMove(move, promos[test::randInt(rng, 0, 3)]);
      if (test::randInt(rng, 0, 5) == 0) game.undo();
    }
    if (game.canSubmit()) return true;
    while (game.undoable()) game.undo();
  }
  return false;
}

} // namespace test
