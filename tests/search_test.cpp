// Tests of TurnSearch (the legal-turn search behind checkmate / stalemate). The reference here is a plain exhaustive
// depth-first search over the engine's own public move API: no pruning, no reordering, the engine's canSubmit() as the
// only judge. Every position the fast search answers is compared with it.
#include <doctest/doctest.h>

#include "test_support.h"

#include <chrono>
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

// A small random multiverse: `lines` original timelines, a few boards each, with kings and a few other pieces on every
// board; optionally timelines "created" by the players.
std::unique_ptr<Sandbox> randomPosition(std::mt19937& rng, int n, int lines, bool created) {
  auto uni = [&](int lo, int hi) { return std::uniform_int_distribution<int>(lo, hi)(rng); };
  std::vector<int> counts;
  for (int i = 0; i < lines; ++i) counts.push_back(uni(2, 4));
  auto game = std::make_unique<Sandbox>(n, counts, 0);
  if (created) {
    // One timeline made by Black (negative ID) and/or by White.
    if (uni(0, 1)) game->addCreatedTimeLine(-1, uni(2, 4));
    if (uni(0, 1)) game->addCreatedTimeLine(lines, uni(2, 4));
  }
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
      put(PieceType::King, PieceColor::PIECEWHITE);
      put(PieceType::King, PieceColor::PIECEBLACK);
      for (int i = uni(0, 2); i > 0; --i) put(kinds[uni(0, 4)], PieceColor::PIECEWHITE);
      for (int i = uni(0, 3) == 0 ? 1 : 0; i > 0; --i) put(kinds[uni(0, 4)], PieceColor::PIECEBLACK);
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
  int compared = 0, found = 0, none = 0, skipped = 0;
  for (unsigned seed = 1; seed <= 100; ++seed) {
    std::mt19937 rng(seed);
    const int n = 4 + int(seed % 2);
    auto game = randomPosition(rng, n, 1 + int(seed % 3), seed % 2 == 0);
    CAPTURE(seed);
    Brute b;
    b.cap = 6000;
    auto brute = game->clone();
    const Truth truth = bruteForce(*brute, b);
    if (truth == Truth::Unknown) { ++skipped; continue; }
    ++compared;
    for (int config = 0; config < 6; ++config) {
      CAPTURE(config); // 0: everything on, 1: no reductions, 2..5: one reduction off
      TurnSearch::Options opt;
      opt.reductions = config != 1;
      opt.commutation = config != 2;
      opt.irrelevant = config != 3;
      opt.forwardCheck = config != 4;
      opt.cheapFirst = config != 5;
      TurnSearch search(*game, opt);
      TurnSearch::Status st = TurnSearch::Status::Running;
      for (int i = 0; i < 2000 and st == TurnSearch::Status::Running; ++i) st = search.step(10000);
      REQUIRE(st != TurnSearch::Status::Running);
      CHECK((st == TurnSearch::Status::Found) == (truth == Truth::Found));
      if (st == TurnSearch::Status::Found) checkTurn(*game, search);
    }
    (truth == Truth::Found ? found : none)++;
  }
  // The random positions must actually exercise both answers.
  CHECK(compared > 55);
  CHECK(found > 9);
  CHECK(none > 15);
  MESSAGE("compared " << compared << " (" << found << " with a legal turn, " << none << " without), skipped " << skipped);
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
          if (game.canSubmit() and std::uniform_int_distribution<int>(0, 2)(rng) != 0) { ok = true; break; }
          auto moves = game.allPseudoLegalMoves();
          if (moves.empty()) break;
          game.makeMove(moves[std::uniform_int_distribution<std::size_t>(0, moves.size() - 1)(rng)]);
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
