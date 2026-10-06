#include "ai/Eval.h"

#include <algorithm>
#include <cstdlib>

namespace Chess::ai {

int pieceValue(PieceType type) {
  switch (type) {
    case PieceType::Pawn: return 100;
    case PieceType::Knight: return 300;
    case PieceType::Bishop: return 320;
    case PieceType::Rook: return 500;
    case PieceType::Queen: return 900;
    case PieceType::King: return 0;
  }
  return 0;
}

int timelinesCreatedBy(const IGame& game, PieceColor color) {
  int count = 0;
  for (const auto& line : game.getTimeLines()) {
    if (!line->hasParent()) continue;
    // The first board of a created timeline has the half-turn forkAt + 1 and it is the opponent's turn there.
    const int firstHalf = line->forkAt() + 1;
    if (PieceColor(((firstHalf % 2) + 2) % 2) == opposite(color)) ++count;
  }
  return count;
}

namespace {

int placement(const Cell cell, int x, int y, int n, const EvalWeights& w) {
  const bool white = cell.color() == PieceColor::PIECEWHITE;
  const int rank = white ? y : n - 1 - y; // 0 = own back rank
  switch (cell.type()) {
    case PieceType::Pawn: return w.pawnAdvance * rank;
    case PieceType::Knight:
    case PieceType::Bishop:
    case PieceType::Queen: {
      // Distance from the centre in half-squares: 0 in the middle, n-1 in a corner.
      const int dx = std::abs(2 * x - (n - 1)), dy = std::abs(2 * y - (n - 1));
      const int steps = std::max(0, (n - 1 - std::max(dx, dy)) / 2); // 0 on the rim, up to n/2 - 1 in the middle
      int v = w.centre * std::min(steps, 3);
      if (cell.type() != PieceType::Queen and rank > 0) v += w.developed;
      return v;
    }
    default: return 0;
  }
}

} // namespace

int evaluate(const IGame& game, PieceColor perspective, const EvalWeights& w) {
  int score = 0; // White's point of view first
  const int n = game.dim();
  for (const auto& line : game.getTimeLines()) {
    const std::shared_ptr<Board> board = line->back();
    for (int y = 0; y < n; ++y) {
      for (int x = 0; x < n; ++x) {
        const Cell c = board->cell(x, y);
        if (c.empty()) continue;
        const int v = pieceValue(c.type()) + placement(c, x, y, n, w);
        score += c.color() == PieceColor::PIECEWHITE ? v : -v;
      }
    }
  }
  score -= w.timelineCreated * (timelinesCreatedBy(game, PieceColor::PIECEWHITE) - timelinesCreatedBy(game, PieceColor::PIECEBLACK));
  return perspective == PieceColor::PIECEWHITE ? score : -score;
}

} // namespace Chess::ai
