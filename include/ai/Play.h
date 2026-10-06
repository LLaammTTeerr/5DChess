#pragma once

// Convenience for tests, tools and self-play: let the AI play a whole turn on a game.

#include "ai/Search.h"

namespace Chess::ai {

/**
 * Runs a Search to the end (stepping with `stepBudget`), makes its moves on `game` and submits the turn; the game's own
 * legal-turn proof is then resolved with `resultNodes` nodes. Returns false (game untouched) if the side to move has no
 * legal turn. `nodesUsed` receives the nodes the search used.
 */
bool playTurn(IGame& game, Options options, long long* nodesUsed = nullptr, int stepBudget = 500, long long resultNodes = 50000);

} // namespace Chess::ai
