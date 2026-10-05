// Tests of TurnSearch (the legal-turn search behind checkmate / stalemate). The reference here is a plain exhaustive
// depth-first search over the engine's own public move API: no pruning, no reordering, the engine's canSubmit() as the
// only judge. Every position the fast search answers is compared with it.
#include <doctest/doctest.h>

#include "test_support.h"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <random>

using namespace Chess;
using namespace test;

namespace {

enum class Truth { Found, None, Unknown };

struct Brute {
  long long nodes = 0;
  long long cap = 0;
};

Truth bruteForce(IGame& game, Brute& b) {
  if (game.undoable() and game.canSubmit()) return Truth::Found;
  bool unknown = false;
  for (const Move& m : game.allPseudoLegalMoves()) {
    auto target = m.to.board->getPiece(m.to.position);
    if (target != nullptr and target->type() == PieceType::King) continue; // makeMove forbids it
    auto piece = m.from.board->getPiece(m.from.position);
    const int lastRank = piece->color() == PieceColor::PIECEWHITE ? game.dim() - 1 : 0;
    const bool promotes = piece->type() == PieceType::Pawn and m.to.position.y() == lastRank;
    for (PieceType promo : {PieceType::Queen, PieceType::Rook, PieceType::Bishop, PieceType::Knight}) {
      if (!promotes and promo != PieceType::Queen) break;
      if (++b.nodes > b.cap) return Truth::Unknown;
      game.makeMove(m, promo);
      const Truth t = bruteForce(game, b);
      game.undo();
      if (t == Truth::Found) return t;
      if (t == Truth::Unknown) unknown = true;
    }
  }
  return unknown ? Truth::Unknown : Truth::None;
}

bool hasKing(const Board& board, int n, PieceColor color) {
  for (int y = 0; y < n; ++y)
    for (int x = 0; x < n; ++x) {
      auto p = board.getPiece({x, y});
      if (p and p->type() == PieceType::King and p->color() == color) return true;
    }
  return false;
}

// A small random multiverse: up to four original timelines of a few boards each, with kings and a few other pieces on
// every board, and timelines "created" by either player (Black's have negative IDs). Every half-turn is shifted by a
// random amount, which together with the board counts decides who is to move (the side whose turn the present is), so
// both colours move. The boards are sparse on purpose: many positions have exactly one legal turn or very few, the case
// where a wrong reduction would lose the only answer. With `sparse` false the boards are somewhat fuller.
std::unique_ptr<Sandbox> randomPosition(std::mt19937& rng, int n, int lines, bool created, bool sparse = true) {
  auto uni = [&](int lo, int hi) { return test::randInt(rng, lo, hi); };
  const int shift = uni(0, 1);
  std::vector<int> counts;
  for (int i = 0; i < lines; ++i) counts.push_back(uni(2, 3) + shift);
  auto game = std::make_unique<Sandbox>(n, counts, 0);
  if (created) {
    // Timelines made by Black (negative ID) and/or by White; up to two of each.
    for (int k = uni(0, lines > 2 ? 1 : 2); k > 0; --k) game->addCreatedTimeLine(game->minTimeLineId() - 1, uni(2, 3) + shift);
    for (int k = uni(0, lines > 2 ? 1 : 2); k > 0; --k) game->addCreatedTimeLine(game->maxTimeLineId() + 1, uni(2, 3) + shift);
  }
  const PieceColor mover = PieceColor(game->bufferHalfTurn() % 2);
  game->setTurnColor(mover);
  static const PieceType kinds[] = {PieceType::Queen, PieceType::Rook, PieceType::Bishop, PieceType::Knight, PieceType::Pawn};
  for (int id : game->timeLineIds()) {
    for (const auto& board : game->timeLine(id)->getBoards()) {
      std::vector<int> free;
      for (int i = 0; i < n * n; ++i) free.push_back(i);
      auto put = [&](PieceType t, PieceColor c) {
        if (free.empty()) return;
        const int k = uni(0, int(free.size()) - 1);
        const int sq = free[std::size_t(k)];
        free.erase(free.begin() + k);
        const int x = sq % n, y = sq / n;
        if (t == PieceType::Pawn and (y == 0 or y == n - 1)) return;
        auto piece = makePiece(t, c);
        if (uni(0, 3) == 0) piece->setUnmoved(false);
        board->placePiece({x, y}, piece);
      };
      // Past boards often lack a king (every king of a past board threatens through time, and the positions would
      // otherwise nearly always be checkmate); the tips always have both.
      const bool tip = board == game->timeLine(id)->back();
      if (tip or uni(0, 1)) put(PieceType::King, PieceColor::PIECEWHITE);
      // Most of the time the enemy king is not next to the mover's (adjacent kings are a check by themselves).
      if (uni(0, 3) != 0 and (tip or uni(0, 1))) {
        for (int tries = 0; tries < 6; ++tries) {
          const int sq = free[std::size_t(uni(0, int(free.size()) - 1))];
          bool adjacent = false;
          for (int y = 0; y < n; ++y)
            for (int x = 0; x < n; ++x)
              if (board->getPiece({x, y}) and std::abs(x - sq % n) <= 1 and std::abs(y - sq / n) <= 1) adjacent = true;
          if (!adjacent) { free.erase(std::find(free.begin(), free.end(), sq)); board->placePiece({sq % n, sq / n}, makePiece(PieceType::King, PieceColor::PIECEBLACK)); break; }
        }
      }
      if (tip and !hasKing(*board, n, PieceColor::PIECEBLACK)) put(PieceType::King, PieceColor::PIECEBLACK);
      // Half the boards hold just the two kings; the rest a piece or two.
      const bool bare = sparse and uni(0, 1) == 0;
      for (int i = bare ? 0 : uni(0, sparse ? 1 : 2); i > 0; --i) put(kinds[uni(0, 4)], mover);
      for (int i = bare or uni(0, 2) != 0 ? 0 : 1; i > 0; --i) put(kinds[uni(0, 4)], opposite(mover));
    }
  }
  return game;
}

// Replays a found turn on a fresh copy and checks that it is a legal turn.
void checkTurn(const IGame& original, const TurnSearch& search) {
  auto copy = original.clone();
  for (const auto& step : search.turn()) {
    // The steps refer to boards of `original`, which the clone shares.
    copy->makeMove(step.move, step.promotion);
  }
  CHECK(copy->canSubmit());
}

} // namespace

TEST_CASE("TurnSearch agrees with an exhaustive search on random small multiverses") {
  int compared = 0, found = 0, none = 0, skipped = 0, pendingCases = 0, bySideBlack = 0, fewTurns = 0;
  for (unsigned seed = 1; seed <= 260; ++seed) {
    std::mt19937 rng(seed * 7919u);
    const int n = seed % 3 == 0 ? 5 : 4;
    static const int lineChoices[] = {1, 1, 2, 2, 3, 4};
    auto game = randomPosition(rng, n, lineChoices[rng() % 6], rng() % 2 == 0, seed % 5 != 0);
    // On about half the seeds a pseudo-legal move is already pending (the search must continue the turn).
    bool pendingMove = false;
    if (rng() % 2 == 0) {
      auto moves = game->allPseudoLegalMoves();
      for (int tries = 0; tries < 8 and !moves.empty() and !pendingMove; ++tries) {
        const Move m = moves[rng() % moves.size()];
        auto target = m.to.board->getPiece(m.to.position);
        if (target != nullptr and target->type() == PieceType::King) continue;
        game->makeMove(m);
        pendingMove = true;
      }
    }
    CAPTURE(seed);
    Brute b;
    b.cap = 500;
    auto brute = game->clone();
    const Truth truth = bruteForce(*brute, b);
    if (truth == Truth::Unknown) { ++skipped; continue; }
    ++compared;
    for (int config = 0; config < 8; ++config) {
      CAPTURE(config); // 0: everything on, 1: no reductions, 2..5: one reduction off, 6 and 7: very early restarts
      TurnSearch::Options opt;
      opt.reductions = config != 1;
      opt.commutation = config != 2;
      opt.irrelevant = config != 3;
      opt.forwardCheck = config != 4;
      opt.cheapFirst = config != 5;
      if (config == 6) opt.restartLimit = 50;
      if (config == 7) opt.restartLimit = 5;
      TurnSearch search(*game, opt);
      TurnSearch::Status st = TurnSearch::Status::Running;
      for (int i = 0; i < 2000 and st == TurnSearch::Status::Running; ++i) st = search.step(10000);
      REQUIRE(st != TurnSearch::Status::Running);
      CHECK((st == TurnSearch::Status::Found) == (truth == Truth::Found));
      if (st == TurnSearch::Status::Found) checkTurn(*game, search);
    }
    (truth == Truth::Found ? found : none)++;
    pendingCases += pendingMove;
    bySideBlack += game->getCurrentTurnColor() == PieceColor::PIECEBLACK;
    if (truth == Truth::Found) {
      // "Few legal turns": count the moves of the first level only (a cheap proxy for how tight the position is).
      if (game->allPseudoLegalMoves().size() <= 6) ++fewTurns;
    }
  }
  // The random positions must actually exercise both answers, both colours, and pending turns.
  CHECK(compared > 150);
  CHECK(found > 40);
  CHECK(none > 40);
  CHECK(pendingCases > 30);
  CHECK(bySideBlack > 30);
  CHECK(compared - bySideBlack > 30);
  CHECK(fewTurns > 10);
  MESSAGE("compared " << compared << " (" << found << " with a legal turn, " << none << " without; " << bySideBlack
          << " with Black to move, " << pendingCases << " with a pending move, " << fewTurns
          << " Found with <= 6 first moves), skipped " << skipped);
}

TEST_CASE("kingCapturable agrees with threatsAgainst") {
  int capturable = 0, total = 0;
  for (unsigned seed = 1; seed <= 200; ++seed) {
    std::mt19937 rng(seed * 31);
    auto game = randomPosition(rng, 4 + int(seed % 2), 1 + int(seed % 3), seed % 3 == 0);
    for (PieceColor victim : {PieceColor::PIECEWHITE, PieceColor::PIECEBLACK}) {
      CAPTURE(seed);
      const bool expected = !game->threatsAgainst(victim).empty();
      CHECK(TurnSearch::kingCapturable(*game, victim) == expected);
      capturable += expected;
      ++total;
    }
  }
  CHECK(capturable > 20);
  CHECK(capturable < total);
}

TEST_CASE("TurnSearch gives the same answer as findLegalTurn on played games, and a found turn is legal") {
  for (unsigned seed : {1u, 2u, 3u}) {
    std::mt19937 rng(seed);
    StandardGame game;
    for (int t = 0; t < 14 and game.result() == GameResult::Ongoing; ++t) {
      TurnSearch search(game);
      TurnSearch::Status st = TurnSearch::Status::Running;
      for (int i = 0; i < 200 and st == TurnSearch::Status::Running; ++i) st = search.step(5000);
      REQUIRE(st != TurnSearch::Status::Running);
      CHECK(st == game.findLegalTurn());
      if (st == TurnSearch::Status::Found) checkTurn(game, search);
      // play a random legal turn
      bool ok = false;
      for (int attempt = 0; attempt < 20 and !ok; ++attempt) {
        for (int step = 0; step < 8; ++step) {
          if (game.canSubmit() and test::randInt(rng, 0, 2) != 0) { ok = true; break; }
          auto moves = game.allPseudoLegalMoves();
          if (moves.empty()) break;
          game.makeMove(moves[std::size_t(test::randInt(rng, 0, int(moves.size()) - 1))]);
        }
        ok = ok or game.canSubmit();
        if (!ok) while (game.undoable()) game.undo();
      }
      if (!ok) break;
      game.submitTurn();
      game.resolveResult();
    }
  }
}
