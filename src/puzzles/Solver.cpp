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

struct Enumerator {
  std::unique_ptr<IGame> g;
  const TurnVisitor& visit;
  long long budget;
  bool prune;
  long long nodes = 0;
  bool stopped = false, exhausted = false;
  size_t bound = 0;
  Turn cur;
  int forks = 0;
  std::set<std::string> seen;

  Enumerator(const IGame& game, const TurnVisitor& v, long long maxNodes, bool pruneDead)
      : g(game.clone()), visit(v), budget(maxNodes), prune(pruneDead) {}

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

  // The same set of moves in another order is the same turn, unless two of them create a timeline (the ids are handed out
  // in creation order): then the order is part of the identity.
  std::string key() const {
    std::vector<std::string> parts;
    for (const auto& m : cur) parts.push_back(Chess::toNotation(m, true));
    if (forks < 2) std::sort(parts.begin(), parts.end());
    std::string s;
    for (const auto& p : parts) s += p + " ";
    return s;
  }

  void dfs() {
    if (!cur.empty() && g->canSubmit() && seen.insert(key()).second) {
      if (!visit(cur, *g)) {
        stopped = true;
        return;
      }
    }
    if (cur.size() >= bound) return;
    const PieceColor mover = g->getCurrentTurnColor();
    for (const auto& move : candidates()) {
      if (nodes++ >= budget) {
        exhausted = true;
        return;
      }
      const int lines = g->timeLineCount();
      g->makeMove(move);
      const bool forked = g->timeLineCount() > lines;
      // A king the opponent can capture stays capturable whatever else is played (docs/SEARCH.md, F2): such a state
      // can never become a legal turn.
      if (!prune || g->threatsAgainst(mover).empty()) {
        cur.push_back(move);
        forks += forked;
        dfs();
        forks -= forked;
        cur.pop_back();
      }
      g->undo();
      if (stopped || exhausted) return;
    }
  }
};

GameResult winFor(PieceColor c) { return c == PieceColor::PIECEWHITE ? GameResult::WhiteWins : GameResult::BlackWins; }

bool crossesBoards(const Turn& turn) {
  return std::any_of(turn.begin(), turn.end(), [](const Chess::Core::Move& m) { return m.from.l != m.to.l || m.from.t != m.to.t; });
}

std::string turnText(const Turn& turn) {
  std::string s;
  for (const auto& m : turn) s += (s.empty() ? "" : " ") + Chess::toNotation(m, m.promotion != Chess::PieceType::Queen);
  return s;
}

} // namespace

bool forEachTurn(const IGame& game, const TurnVisitor& visit, long long maxNodes, bool prune) {
  Enumerator e(game, visit, maxNodes, prune);
  e.bound = e.g->getMoveableBoards().size();
  e.dfs();
  return !e.exhausted;
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

DefenceProver::DefenceProver(const IGame& game) : _game(game.clone()) {
  const bool complete = forEachTurn(*_game, [&](const Turn& turn, const IGame&) {
    _defences.push_back(turn);
    return true;
  });
  if (!complete) throw std::runtime_error("too many defences to list");
  if (_defences.empty()) _status = Status::Refuted; // nothing to prove: the game is over already (never a mate in 2)
}

DefenceProver::Status DefenceProver::step(double milliseconds) {
  const auto start = std::chrono::steady_clock::now();
  while (_status == Status::Running) {
    const Turn& defence = _defences[_next];
    const auto after = submitted(*_game, defence);
    if (!after || after->result() != GameResult::Ongoing || !findMate(*after)) {
      _refutation = defence;
      _status = Status::Refuted;
      break;
    }
    if (++_next == _defences.size()) {
      _status = Status::Proven;
      break;
    }
    if (std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count() >= milliseconds) break;
  }
  return _status;
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
