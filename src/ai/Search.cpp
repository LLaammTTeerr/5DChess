#include "ai/Search.h"

#include "ai/Eval.h"
#include "ai/TurnGen.h"

#include <algorithm>
#include <cassert>

namespace Chess::ai {

namespace {

constexpr int Inf = 1000000;
constexpr int MaxPlies = 6;
constexpr int ProbeChunk = 32; ///< nodes of the opponent's legal-turn proof per slice
/** Iterative deepening: completed searches of 1, 2, 4, 6 turns. Even depths end with the opponent's reply, so a capture on
 *  the last turn is never judged without the recapture; depth 1 is the ordering pass and the Easy level. */
constexpr int Depths[] = {1, 2, 4, 6};

struct LevelParams {
  long long maxNodes;     ///< soft limit on the nodes of one decision (the depth-1 pass always completes)
  int maxPlies;           ///< deepest iteration: 1, 2, 4 or 6 turns
  GenParams gen[MaxPlies];///< candidate-turn beam of the inner plies (ply 0 = our turn)
  int leafCap[MaxPlies];  ///< turns considered per node at those plies
  GenParams horizon;      ///< beam of the last ply: wide, so that a mating reply is among its turns
  int horizonCap;
  int margin;             ///< centipawns: a root turn within `margin` of the best may be chosen at random
  int probeNodes;         ///< nodes granted to the "does the opponent have any legal turn" proof at every node
};

GenParams gp(int beam, int deepBeam, int maxTravel) {
  GenParams g;
  g.beam = beam;
  g.deepBeam = deepBeam;
  g.maxTravel = maxTravel;
  return g;
}

LevelParams levelParams(Level level) {
  LevelParams p{};
  switch (level) {
    case Level::Easy:
      p.maxNodes = 4000;
      p.maxPlies = 1;
      p.gen[0] = gp(5, 2, 0);
      p.leafCap[0] = 8;
      p.margin = 120;
      p.probeNodes = 600;
      break;
    case Level::Normal:
      p.maxNodes = 150000;
      p.maxPlies = 4;
      p.gen[0] = gp(16, 4, 3);
      p.gen[1] = gp(10, 3, 2);
      p.gen[2] = gp(8, 3, 2);
      p.leafCap[0] = 60;
      p.leafCap[1] = 20;
      p.leafCap[2] = 12;
      p.horizon = gp(16, 4, 2);
      p.horizonCap = 30;
      p.margin = 8;
      p.probeNodes = 3000;
      break;
    case Level::Hard:
      p.maxNodes = 900000;
      p.maxPlies = 6;
      p.gen[0] = gp(24, 5, 4);
      p.gen[1] = gp(14, 4, 3);
      p.gen[2] = gp(10, 3, 2);
      p.gen[3] = gp(8, 3, 2);
      p.gen[4] = gp(8, 3, 2);
      p.leafCap[0] = 80;
      p.leafCap[1] = 30;
      p.leafCap[2] = 14;
      p.leafCap[3] = 10;
      p.leafCap[4] = 10;
      p.horizon = gp(20, 4, 2);
      p.horizonCap = 40;
      p.margin = 0;
      p.probeNodes = 6000;
      break;
  }
  return p;
}

struct SplitMix {
  std::uint64_t s;
  std::uint64_t next() {
    std::uint64_t z = (s += 0x9e3779b97f4a7c15ull);
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ull;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebull;
    return z ^ (z >> 31);
  }
};

struct Candidate {
  std::vector<Core::Move> moves;
  int value = 0;
};

/** One level of the search: the turns of one side in one position. */
struct Ply {
  enum class Phase { Gen, Probe, Descend };
  std::unique_ptr<TurnGen> gen;    // null at the root of the iterations after the first: those walk the ordered root list
  std::unique_ptr<IGame> listGame; // list-driven root: the position after the current listed turn
  IGame* leaf = nullptr;           // the position after the current turn (not yet submitted)
  PieceColor mover = PieceColor::PIECEWHITE;
  std::size_t listIdx = 0;
  Phase phase = Phase::Gen;
  int alpha = -Inf, beta = Inf, best = -Inf;
  int leaves = 0;
  std::unique_ptr<IGame> child; // the position after the current turn was submitted
  int probed = 0;
  std::vector<Core::Move> moves; // the current turn (kept at the root only)
};

} // namespace

struct Search::Impl {
  enum class Phase { Search, Fallback, Finished };

  Options options;
  LevelParams params;
  std::unique_ptr<IGame> start; // pristine clone of the game
  PieceColor me;
  std::size_t basePending = 0;
  SplitMix rng;

  Phase phase = Phase::Search;
  std::vector<Ply> stack;
  int iteration = -1, plies = 0, depthDone = 0;
  std::vector<Candidate> rootList;  // root turns of the last completed iteration, best first (their search order next time)
  std::vector<Candidate> iterList;  // root turns of the running iteration with their values (bounds when cut off)
  std::vector<Candidate> iterCands; // those that are exact and within the margin of the best
  std::vector<Candidate> candidates;// iterCands of the last completed iteration: what the decision is made from
  int rootDone = 0;
  bool mate = false;
  bool aborted = false;         // the node limit was reached inside the running iteration
  const long long* liveBudget = nullptr; // budget of the stepPly call in progress, and its value when it began
  long long liveBase = 0;

  std::unique_ptr<TurnSearch> fallback;
  long long nodes = 0;
  std::vector<Core::Move> result;
  bool hasResult = false;

  Impl(const IGame& game, Options o)
      : options(o), params(levelParams(o.level)), start(game.clone()), me(game.getCurrentTurnColor()), rng{o.seed} {
    basePending = game.pendingMoves().size();
    if (o.maxNodes > 0) params.maxNodes = o.maxNodes;
    if (game.result() != GameResult::Ongoing) {
      phase = Phase::Finished;
      return;
    }
    startIteration();
  }

  bool hasNextIteration() const {
    return iteration + 1 < int(std::size(Depths)) and Depths[iteration + 1] <= params.maxPlies;
  }

  void startIteration() {
    ++iteration;
    plies = Depths[iteration];
    stack.clear();
    iterList.clear();
    iterCands.clear();
    rootDone = 0;
    if (iteration == 0) {
      pushPly(start->clone(), -Inf, Inf);
    } else {
      Ply root;
      root.mover = me;
      stack.push_back(std::move(root));
    }
  }

  /** Nodes used so far, including the stepPly call in progress. The node limit is only tested at logical points (a root turn
   *  is complete, an iteration ends), where this value does not depend on how the caller slices its budget. */
  long long total() const { return nodes + (liveBudget ? liveBase - *liveBudget : 0); }

  const GenParams& genParams(std::size_t depth) const {
    return depth > 0 and int(depth) + 1 == plies ? params.horizon : params.gen[depth];
  }
  int leafCap(std::size_t depth) const {
    return depth > 0 and int(depth) + 1 == plies ? params.horizonCap : params.leafCap[depth];
  }

  void pushPly(std::unique_ptr<IGame> game, int alpha, int beta) {
    Ply p;
    p.mover = game->getCurrentTurnColor();
    p.gen = std::make_unique<TurnGen>(std::move(game), genParams(stack.size()));
    p.alpha = alpha;
    p.beta = beta;
    stack.push_back(std::move(p));
  }

  int bestRoot() const {
    int b = -Inf;
    for (const Candidate& c : iterCands) b = std::max(b, c.value);
    return b;
  }

  /** The turn just generated at ply `depth` is worth `v` to its mover. Returns true when the ply is now decided (cutoff). */
  bool update(std::size_t depth, int v) {
    Ply& p = stack[depth];
    p.best = std::max(p.best, v);
    if (depth == 0) {
      ++rootDone;
      iterList.push_back({p.moves, v});
      const int floor = iterCands.empty() ? -Inf : bestRoot() - params.margin - 1;
      if (v > floor) iterCands.push_back({p.moves, v});
      if (v >= MateScore - MaxPlies - 1) mate = true;
      if (depthDone >= 1 and total() >= params.maxNodes) aborted = true; // the depth-1 pass always completes
      return mate or aborted; // a proven mate: nothing can beat it
    }
    p.alpha = std::max(p.alpha, v);
    return p.alpha >= p.beta;
  }

  /** The window the children of ply `depth` are searched in (from their own point of view). */
  void childWindow(std::size_t depth, int& alpha, int& beta) const {
    const Ply& p = stack[depth];
    int lo;
    if (depth == 0) lo = iterCands.empty() ? -Inf : bestRoot() - params.margin - 1;
    else lo = std::max(p.alpha, p.best);
    alpha = -p.beta;
    beta = -lo;
  }

  /** Pops the top ply and hands its value to the parent (which may then be decided itself). */
  void finishPly() {
    while (true) {
      const Ply& p = stack.back();
      const int value = p.leaves == 0 ? evaluate(p.gen ? p.gen->game() : *start, p.mover, params.gen[0].weights) : p.best;
      stack.pop_back();
      if (stack.empty()) return;
      Ply& parent = stack.back();
      parent.child.reset();
      parent.phase = Ply::Phase::Gen;
      if (!update(stack.size() - 1, -value)) return;
      if (stack.size() == 1) { // the root is decided: a mate
        stack.clear();
        return;
      }
    }
  }

  /** Called after update() returned true at the top ply. */
  void decided() {
    if (stack.size() == 1) stack.clear();
    else finishPly();
  }

  /** The running iteration has searched everything (or proved a mate). */
  void endIteration() {
    if (aborted and !mate) { // out of nodes: decide with the last completed iteration
      conclude();
      return;
    }
    if (iterCands.empty()) { // no turn found by the generator at all
      if (candidates.empty()) startFallback();
      else conclude();
      return;
    }
    candidates = iterCands;
    depthDone = plies;
    rootList = iterList;
    std::stable_sort(rootList.begin(), rootList.end(), [](const Candidate& a, const Candidate& b) { return a.value > b.value; });
    if (mate or !hasNextIteration() or nodes >= params.maxNodes) conclude();
    else startIteration();
  }

  /** Verifies a turn against the pristine game; the turn is accepted only if canSubmit() holds after it. */
  bool verified(const std::vector<Core::Move>& moves) const {
    auto copy = start->clone();
    for (const Core::Move& m : moves) copy->makeMove(m);
    return copy->canSubmit();
  }

  void conclude() {
    phase = Phase::Finished;
    stack.clear();
    int best = -Inf;
    for (const Candidate& c : candidates) best = std::max(best, c.value);
    std::vector<const Candidate*> pool;
    for (const Candidate& c : candidates) {
      if (c.value >= best - (mate ? 0 : params.margin)) pool.push_back(&c);
    }
    while (!pool.empty()) {
      const std::size_t pick = pool.size() == 1 ? 0 : std::size_t(rng.next() % pool.size());
      if (verified(pool[pick]->moves)) {
        result = pool[pick]->moves;
        hasResult = true;
        return;
      }
      pool.erase(pool.begin() + std::ptrdiff_t(pick)); // cannot happen; an illegal turn is never returned
    }
    startFallback();
  }

  void startFallback() {
    phase = Phase::Fallback;
    stack.clear();
    fallback = std::make_unique<TurnSearch>(*start);
  }

  void stepFallback(long long& budget) {
    const auto before = fallback->nodes();
    const auto st = fallback->step(int(std::min<long long>(budget, 1 << 20)));
    const long long used = fallback->nodes() - before;
    nodes += used;
    budget -= std::max<long long>(used, 1);
    if (st == TurnSearch::Status::Running) return;
    if (st == TurnSearch::Status::Found) {
      std::vector<Core::Move> moves;
      for (const auto& s : fallback->turn()) moves.push_back(Core::Move{s.move.from.coord(), s.move.to.coord(), s.promotion});
      if (verified(moves)) {
        result = std::move(moves);
        hasResult = true;
      }
    }
    phase = Phase::Finished;
  }

  Status step(long long budget) {
    while (phase != Phase::Finished and budget > 0) {
      if (phase == Phase::Fallback) {
        stepFallback(budget);
        continue;
      }
      if (stack.empty()) {
        endIteration();
        continue;
      }
      const long long before = budget;
      liveBudget = &budget;
      liveBase = before;
      stepPly(budget);
      liveBudget = nullptr;
      nodes += before - budget;
    }
    return phase == Phase::Finished ? Status::Done : Status::Running;
  }

  /** The next turn of a list-driven root (iterations after the first): the position after it is built in p.listGame. */
  bool nextListTurn(Ply& p) {
    if (p.listIdx >= rootList.size()) return false;
    const Candidate& c = rootList[p.listIdx++];
    p.listGame = start->clone();
    for (const Core::Move& m : c.moves) p.listGame->makeMove(m);
    p.leaf = p.listGame.get();
    p.moves = c.moves;
    return true;
  }

  void stepPly(long long& budget) {
    const std::size_t depth = stack.size() - 1;
    Ply& p = stack.back();
    switch (p.phase) {
      case Ply::Phase::Descend: break; // never current: the child ply is on top
      case Ply::Phase::Gen: {
        if (p.leaves >= leafCap(depth)) {
          finishPly();
          return;
        }
        if (p.gen) {
          switch (p.gen->advance(budget)) {
            case TurnGen::Result::Pause: return;
            case TurnGen::Result::Done: finishPly(); return;
            case TurnGen::Result::Leaf: break;
          }
          p.leaf = &p.gen->game();
          if (depth == 0) {
            p.moves.clear();
            const auto& pending = p.leaf->pendingMoves();
            for (std::size_t i = basePending; i < pending.size(); ++i) p.moves.push_back(pending[i].move);
          }
        } else if (!nextListTurn(p)) {
          finishPly();
          return;
        }
        ++p.leaves;
        p.child = p.leaf->clone();
        p.child->submitTurn(); // arms the opponent's legal-turn proof
        p.probed = 0;
        p.phase = Ply::Phase::Probe;
        return;
      }
      case Ply::Phase::Probe: {
        const long long chunk = ProbeChunk; // fixed, so that the outcome does not depend on how the caller slices its budget
        p.child->stepResultSearch(int(chunk));
        budget -= chunk;
        p.probed += int(chunk);
        if (p.child->resultPending() and p.probed < params.probeNodes) return;
        const GameResult r = p.child->result();
        if (r != GameResult::Ongoing) { // the opponent has no legal turn: mate (a win for us) or stalemate
          const bool win = (r == GameResult::WhiteWins and p.mover == PieceColor::PIECEWHITE) or
                           (r == GameResult::BlackWins and p.mover == PieceColor::PIECEBLACK);
          p.child.reset();
          p.phase = Ply::Phase::Gen;
          if (update(depth, win ? MateScore - int(depth) : 0)) decided();
          return;
        }
        if (int(depth) + 1 == plies) { // the horizon: static evaluation of the position after this turn
          p.child.reset();
          p.phase = Ply::Phase::Gen;
          if (update(depth, evaluate(*p.leaf, p.mover, params.gen[0].weights))) decided();
          return;
        }
        int alpha, beta;
        childWindow(depth, alpha, beta);
        p.phase = Ply::Phase::Descend;
        std::unique_ptr<IGame> child = std::move(p.child);
        pushPly(std::move(child), alpha, beta); // invalidates p
        return;
      }
    }
  }
};

Search::Search(const IGame& game, Options options) : _impl(std::make_unique<Impl>(game, options)) {}
Search::~Search() = default;

Search::Status Search::step(int nodeBudget) { return _impl->step(std::max(nodeBudget, 1)); }

Search::Status Search::status() const { return _impl->phase == Impl::Phase::Finished ? Status::Done : Status::Running; }

bool Search::hasTurn() const { return status() == Status::Done and _impl->hasResult; }

std::vector<Core::Move> Search::bestTurn() const { return hasTurn() ? _impl->result : std::vector<Core::Move>{}; }

Progress Search::progress() const {
  Progress p;
  p.nodes = _impl->nodes;
  p.depth = _impl->depthDone;
  p.candidatesTotal = int(_impl->iterList.size());
  p.candidatesDone = _impl->rootDone;
  const auto& cs = _impl->candidates;
  p.bestScore = cs.empty() ? 0 : std::max_element(cs.begin(), cs.end(), [](const Candidate& a, const Candidate& b) { return a.value < b.value; })->value;
  p.mateFound = _impl->mate;
  p.fraction = status() == Status::Done ? 1.0 : std::min(0.99, double(_impl->nodes) / double(_impl->params.maxNodes));
  return p;
}

} // namespace Chess::ai
