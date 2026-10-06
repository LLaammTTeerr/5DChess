#include "ai/Play.h"

namespace Chess::ai {

bool playTurn(IGame& game, Options options, long long* nodesUsed, int stepBudget, long long resultNodes) {
  Search search(game, options);
  while (search.step(stepBudget) == Search::Status::Running) {}
  if (nodesUsed) *nodesUsed = search.progress().nodes;
  if (!search.hasTurn()) return false;
  for (const Core::Move& m : search.bestTurn()) game.makeMove(m);
  game.submitTurn();
  game.resolveResult(resultNodes);
  return true;
}

} // namespace Chess::ai
