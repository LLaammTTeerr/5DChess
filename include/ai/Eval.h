#pragma once

// Static evaluation of a 5D chess position for the AI (see docs/AI.md, "Evaluation").
// Centipawns, positive = good for `perspective`. Pure function of the position: no search, no randomness.

#include "chess.h"

namespace Chess::ai {

/** Standard piece values in centipawns (the king has none: losing it ends the game, which the search sees as a mate). */
int pieceValue(PieceType type);

struct EvalWeights {
  int centre = 6;           ///< knights/bishops/queens: per step closer to the centre of the board (0..3 steps)
  int pawnAdvance = 5;      ///< pawns: per rank advanced
  int developed = 12;       ///< knights and bishops that left their back rank
  int timelineCreated = 12; ///< per timeline created by the side minus those created by the other side (subtracted)
  int kingCheck = 60;       ///< penalty per attack on one of our kings that must be answered (added by the search)
};

/**
 * Material + placement on the latest board of every timeline ("tip"), plus the timeline-balance term. Material of boards
 * that exist in several timelines counts in each of them: a timeline is another front on which a lead is worth more.
 * King attacks are NOT part of this function (they need the move generator; the search adds them where it can afford to).
 */
int evaluate(const IGame& game, PieceColor perspective, const EvalWeights& weights = {});

/** Pieces on the latest board of every timeline: the size of the work one evaluation or king-safety test does. The search
 *  charges its node budget in proportion to it, so that a node costs about the same time in every position. */
int positionLoad(const IGame& game);

/** Number of timelines created by `color` (a created timeline's first board belongs to the opponent's turn). */
int timelinesCreatedBy(const IGame& game, PieceColor color);

} // namespace Chess::ai
