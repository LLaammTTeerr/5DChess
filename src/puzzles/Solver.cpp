#include "puzzles/Solver.h"
#include <algorithm>
#include <chrono>
#include <set>
#include <sstream>
#include <stdexcept>
#include "engine/Notation.h"
#include "engine/Position.h"

namespace puzzles {

using Chess::GameResult;
using Chess::IGame;
using Chess::PieceColor;

namespace {

GameResult winFor(PieceColor c) { return c == PieceColor::PIECEWHITE ? GameResult::WhiteWins : GameResult::BlackWins; }

bool crossesBoards(const Turn& turn) {
  return std::any_of(turn.begin(), turn.end(), [](const Chess::Core::Move& m) { return m.from.l != m.to.l || m.from.t != m.to.t; });
}

std::string turnText(const Turn& turn) {
  std::string s;
  for (const auto& m : turn) s += (s.empty() ? "" : " ") + Chess::toNotation(m, m.promotion != Chess::PieceType::Queen);
  return s;
}

using Clock = std::chrono::steady_clock;
double millisecondsSince(Clock::time_point start) { return std::chrono::duration<double, std::milli>(Clock::now() - start).count(); }

} // namespace

// ---------------------------------------------------------------------------------------------------------------------
// Turn enumeration: an explicit-stack depth-first search over the moves of the mover, so it can stop after any node.

struct TurnEnumerator::Impl {
  struct Frame {
    std::vector<Chess::Core::Move> candidates;
    size_t next = 0;
    bool visited = false; // the turn made so far has been looked at, the candidates are computed
    bool made = false;    // entered through a move (undone when the frame is left)
    bool forked = false;
  };

  std::unique_ptr<IGame> g;
  bool prune;
  long long maxNodes, nodes = 0;
  size_t bound = 0;
  Turn cur;
  int forks = 0;
  std::vector<Frame> stack;
  std::set<std::string> seen;

  Impl(const IGame& game, bool pruneDead, long long limit) : g(game.clone()), prune(pruneDead), maxNodes(limit) {
    bound = g->getMoveableBoards().size();
    stack.emplace_back();
  }

  std::vector<Chess::Core::Move> candidates() const {
    std::vector<Chess::Core::Move> out;
    const PieceColor mover = g->getCurrentTurnColor();
    for (const auto& board : g->getMoveableBoards()) {
      const int n = board->dim();
      for (int y = 0; y < n; ++y)
        for (int x = 0; x < n; ++x) {
          const auto piece = board->at(Chess::Position2D(x, y));
          if (!piece || piece->color != mover) continue;
          const Chess::Core::Coord from{static_cast<int8_t>(x), static_cast<int8_t>(y), static_cast<int16_t>(board->halfTurnNumber()),
                                        static_cast<int16_t>(board->timeLineId())};
          const auto moves = g->legalMovesFrom(from);
          out.insert(out.end(), moves.begin(), moves.end());
        }
    }
    return out;
  }

  // A turn is the position it leads to: the same set of moves in another order (or two routes to the same boards) is one turn.
  std::string fingerprint() const { return Chess::Core::writePosition(Chess::Core::Position::fromGame(*g)); }

  Status run(long long budget, Turn& out) {
    while (!stack.empty()) {
      Frame& f = stack.back();
      if (!f.visited) {
        f.visited = true;
        if (!cur.empty() && g->canSubmit() && seen.insert(fingerprint()).second) {
          out = cur;
          return Status::Turn;
        }
        if (cur.size() < bound) f.candidates = candidates();
      }
      if (f.next >= f.candidates.size()) {
        if (f.made) {
          g->undo();
          cur.pop_back();
          forks -= f.forked;
        }
        stack.pop_back();
        continue;
      }
      if (nodes >= maxNodes) return Status::Limit;
      if (budget-- <= 0) return Status::Running;
      ++nodes;
      const Chess::Core::Move move = f.candidates[f.next++];
      const PieceColor mover = g->getCurrentTurnColor();
      const int lines = g->timeLineCount();
      g->makeMove(move);
      // A king the opponent can capture stays capturable whatever else is played (docs/SEARCH.md, F2): such a state
      // can never become a legal turn.
      if (prune && !g->threatsAgainst(mover).empty()) {
        g->undo();
        continue;
      }
      Frame child;
      child.made = true;
      child.forked = g->timeLineCount() > lines;
      forks += child.forked;
      cur.push_back(move);
      stack.push_back(std::move(child)); // (invalidates `f`)
    }
    return Status::Done;
  }
};

using Status_ = TurnEnumerator::Status;

TurnEnumerator::TurnEnumerator(const IGame& game, bool prune, long long maxNodes) : _impl(new Impl(game, prune, maxNodes)) {}
TurnEnumerator::~TurnEnumerator() = default;
TurnEnumerator::Status TurnEnumerator::next(long long nodeBudget, Turn& turn) { return _impl->run(nodeBudget, turn); }
const IGame& TurnEnumerator::pending() const { return *_impl->g; }
long long TurnEnumerator::nodes() const { return _impl->nodes; }

bool forEachTurn(const IGame& game, const TurnVisitor& visit, long long maxNodes, bool prune) {
  TurnEnumerator e(game, prune, maxNodes);
  Turn turn;
  for (;;) {
    switch (e.next(1'000'000, turn)) {
      case Status_::Turn:
        if (!visit(turn, e.pending())) return true;
        break;
      case Status_::Running: break;
      case Status_::Done: return true;
      case Status_::Limit: return false;
    }
  }
}

std::unique_ptr<IGame> submitted(const IGame& game, const Turn& turn, long long resolveNodes) {
  auto g = game.clone();
  for (const auto& move : turn) {
    const auto offered = g->legalMovesFrom(move.from);
    if (std::find(offered.begin(), offered.end(), move) == offered.end()) return nullptr;
    g->makeMove(move);
  }
  if (!g->canSubmit()) return nullptr;
  g->submitTurn();
  if (!g->resolveResult(resolveNodes)) throw std::runtime_error("the legal-turn proof did not finish");
  return g;
}

bool mates(const IGame& pending, long long resolveNodes) {
  const PieceColor mover = pending.getCurrentTurnColor();
  auto g = pending.clone();
  g->submitTurn();
  if (!g->inCheck()) return false;
  if (!g->resolveResult(resolveNodes)) throw std::runtime_error("the legal-turn proof did not finish");
  return g->result() == winFor(mover);
}

std::vector<Turn> matingTurns(const IGame& game, bool* complete, size_t stopAfter) {
  std::vector<Turn> found;
  const bool done = forEachTurn(game, [&](const Turn& turn, const IGame& pending) {
    if (mates(pending)) found.push_back(turn);
    return found.size() < stopAfter;
  });
  if (complete) *complete = done;
  return found;
}

std::optional<Turn> findMate(const IGame& game) {
  auto found = matingTurns(game, nullptr, 1);
  if (found.empty()) return std::nullopt;
  return found.front();
}

using Status_ = TurnEnumerator::Status;

struct DefenceProver::Impl {
  std::unique_ptr<IGame> game;
  std::unique_ptr<TurnEnumerator> defenceEnum;
  std::vector<Turn> defences;
  bool enumerated = false;
  size_t next = 0; // the defence being checked
  Status status = Status::Running;
  Turn refutation;

  // The check of one defence: first the position after it is made and its legal-turn proof resolved, then White's turns are tried.
  std::unique_ptr<IGame> after;
  bool resolving = false;
  long long resolveNodes = 0;
  std::unique_ptr<TurnEnumerator> mateEnum;

  explicit Impl(const IGame& g) : game(g.clone()), defenceEnum(new TurnEnumerator(*game)) {}

  void refute(const Turn& defence) {
    refutation = defence;
    status = Status::Refuted;
  }

  void finishDefence() {
    after.reset();
    mateEnum.reset();
    resolving = false;
    resolveNodes = 0;
    if (++next == defences.size()) status = Status::Proven;
  }

  void work() {
    if (!enumerated) {
      Turn turn;
      switch (defenceEnum->next(250, turn)) {
        case Status_::Turn: defences.push_back(turn); break;
        case Status_::Running: break;
        case Status_::Done:
          enumerated = true;
          if (defences.empty()) status = Status::Refuted; // nothing to prove: the game is over already (never a mate in 2)
          break;
        case Status_::Limit: throw std::runtime_error("too many defences to list");
      }
      return;
    }
    const Turn& defence = defences[next];
    if (!after && !resolving) {
      auto g = game->clone();
      for (const auto& move : defence) {
        const auto offered = g->legalMovesFrom(move.from);
        if (std::find(offered.begin(), offered.end(), move) == offered.end()) return refute(defence);
        g->makeMove(move);
      }
      if (!g->canSubmit()) return refute(defence);
      g->submitTurn();
      after = std::move(g);
      resolving = true;
    }
    if (resolving) {
      if (after->resultPending()) {
        after->stepResultSearch(2000);
        resolveNodes += 2000;
        if (resolveNodes > 50'000'000) throw std::runtime_error("the legal-turn proof did not finish");
        return;
      }
      resolving = false;
      if (after->result() != GameResult::Ongoing) return refute(defence);
      mateEnum = std::make_unique<TurnEnumerator>(*after);
      return;
    }
    Turn turn;
    switch (mateEnum->next(250, turn)) {
      case Status_::Turn:
        if (mates(mateEnum->pending())) finishDefence();
        break;
      case Status_::Running: break;
      case Status_::Done: refute(defence); break;
      case Status_::Limit: throw std::runtime_error("too many turns to try");
    }
  }
};

DefenceProver::DefenceProver(const IGame& game) : _impl(new Impl(game)) {}
DefenceProver::~DefenceProver() = default;

DefenceProver::Status DefenceProver::step(double milliseconds) {
  const auto start = Clock::now();
  while (_impl->status == Status::Running) {
    _impl->work();
    if (_impl->status == Status::Running && millisecondsSince(start) >= milliseconds) break;
  }
  return _impl->status;
}

DefenceProver::Status DefenceProver::status() const { return _impl->status; }
bool DefenceProver::enumerated() const { return _impl->enumerated; }
size_t DefenceProver::defenceCount() const { return _impl->defences.size(); }
size_t DefenceProver::done() const { return _impl->next; }
const std::vector<Turn>& DefenceProver::defences() const { return _impl->defences; }
const Turn& DefenceProver::refutation() const { return _impl->refutation; }

double DefenceProver::fraction() const {
  if (_impl->status == Status::Proven) return 1.0;
  if (!_impl->enumerated) return 0.05;
  return 0.1 + 0.9 * static_cast<double>(_impl->next) / static_cast<double>(std::max<size_t>(1, _impl->defences.size()));
}

Report validate(const Puzzle& puzzle) {
  const auto start = std::chrono::steady_clock::now();
  Report r;
  auto error = [&r](const std::string& what) { r.errors.push_back(what); };
  const auto game = puzzle.start();
  r.timelines = game->timeLineCount();

  try {
    if (game->getCurrentTurnColor() != PieceColor::PIECEWHITE) error("White must be to move");
    if (puzzle.tier < 1 || puzzle.tier > kTiers) error("difficulty out of range");
    if (puzzle.tier <= 2 && puzzle.goal != Goal::MateIn1) error("tiers 1 and 2 are mates in 1");
    if (puzzle.tier == 2 && r.timelines < 2) error("a time-travel puzzle needs two or more timelines");
    if (puzzle.hint.empty()) error("a puzzle needs a hint");

    if (puzzle.goal == Goal::MateIn1) {
      bool complete = false;
      size_t turns = 0;
      const bool done = forEachTurn(*game, [&](const Turn& turn, const IGame& pending) {
        ++turns;
        if (mates(pending)) {
          r.winners.push_back(turn);
          r.travelWins += crossesBoards(turn);
          r.branchingWins += pending.timeLineCount() > game->timeLineCount();
        }
        return true;
      });
      complete = done;
      r.firstTurns = turns;
      r.winningFirstTurns = r.winners.size();
      if (!complete) error("the enumeration of White's turns hit its limit");
      if (turns == 0) error("White has no legal turn (the position is already mate or stalemate)");
      else if (r.winners.empty()) error("White has no mating turn");
      const auto stored = submitted(*game, puzzle.solution[0]);
      if (!stored) error("the stored solution is not a legal turn: " + turnText(puzzle.solution[0]));
      else if (stored->result() != GameResult::WhiteWins) error("the stored solution does not mate: " + turnText(puzzle.solution[0]));
      if (puzzle.tier == 2 && r.travelWins != r.winners.size())
        error("tier 2: " + std::to_string(r.winners.size() - r.travelWins) + " mating turn(s) need no move to another board");
    } else {
      size_t turns = 0, mateInOne = 0;
      std::vector<Turn> candidates;
      const bool done = forEachTurn(*game, [&](const Turn& turn, const IGame& pending) {
        ++turns;
        auto g = pending.clone();
        g->submitTurn();
        if (!g->resolveResult(50'000'000)) throw std::runtime_error("the legal-turn proof did not finish");
        if (g->result() == GameResult::WhiteWins) ++mateInOne;
        else if (g->result() == GameResult::Ongoing) candidates.push_back(turn);
        return true;
      });
      r.firstTurns = turns;
      if (!done) error("the enumeration of White's turns hit its limit");
      if (turns == 0) error("White has no legal turn (the position is already mate or stalemate)");
      if (mateInOne) error("White has a mate in 1 (" + std::to_string(mateInOne) + " turn(s)): too easy for a mate in 2");
      for (const Turn& turn : candidates) {
        const auto after = submitted(*game, turn);
        DefenceProver prover(*after);
        while (prover.step(1e9) == DefenceProver::Status::Running) {}
        if (prover.status() != DefenceProver::Status::Proven) continue;
        r.winners.push_back(turn);
        r.travelWins += crossesBoards(turn);
        auto pending = game->clone();
        for (const auto& m : turn) pending->makeMove(m);
        r.branchingWins += pending->timeLineCount() > game->timeLineCount();
      }
      r.winningFirstTurns = r.winners.size();
      if (r.winners.empty()) error("White has no forced mate in 2");

      const auto first = submitted(*game, puzzle.solution[0]);
      if (!first) {
        error("the stored first turn is not legal: " + turnText(puzzle.solution[0]));
      } else if (first->result() != GameResult::Ongoing) {
        error("the stored first turn ends the game at once");
      } else {
        DefenceProver prover(*first);
        while (prover.step(1e9) == DefenceProver::Status::Running) {}
        r.defences = prover.defenceCount();
        if (prover.status() != DefenceProver::Status::Proven)
          error("the stored first turn does not win by force; the defence " + turnText(prover.refutation()) + " escapes");
        const auto reply = submitted(*first, puzzle.solution[1]);
        if (!reply) error("the stored reply is not a legal turn for Black: " + turnText(puzzle.solution[1]));
        else if (reply->result() != GameResult::Ongoing) error("the stored reply ends the game");
        else {
          const auto last = submitted(*reply, puzzle.solution[2]);
          if (!last) error("the stored last turn is not legal: " + turnText(puzzle.solution[2]));
          else if (last->result() != GameResult::WhiteWins) error("the stored last turn does not mate: " + turnText(puzzle.solution[2]));
        }
      }
    }
  } catch (const std::exception& e) {
    error(std::string("exception: ") + e.what());
  }
  r.ok = r.errors.empty();
  r.seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
  return r;
}

std::string describe(const Puzzle& puzzle, const Report& r) {
  std::ostringstream out;
  out << (r.ok ? "ok   " : "FAIL ") << puzzle.id << "  tier " << puzzle.tier << "  " << puzzle.goalText() << "  timelines " << r.timelines
      << "  winning first turns " << r.winningFirstTurns << " of " << r.firstTurns;
  if (puzzle.goal == Goal::MateIn2) out << "  defences " << r.defences;
  out << "  travel " << r.travelWins << "  branching " << r.branchingWins << "  " << static_cast<int>(r.seconds * 1000) << " ms";
  for (const auto& e : r.errors) out << "\n       " << e;
  return out.str();
}

} // namespace puzzles
