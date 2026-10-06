// Tests of the AI opponent engine (include/ai, docs/AI.md): legality of what it returns, mate finding, safety, determinism,
// the step budget, and a self-play smoke test.
#include <doctest/doctest.h>

#include "ai/Eval.h"
#include "ai/Play.h"
#include "ai/Search.h"
#include "engine/Notation.h"
#include "engine/Position.h"
#include "test_support.h"

#include <string>

using namespace Chess;
using namespace test;

namespace {

std::shared_ptr<IGame> fromText(const std::string& text) { return Core::parsePosition(text).makeGame(); }

// An 8x8 position with one board per row text (timeline 0, T1w); `toMove` is "white" or "black".
std::string singleBoard(const char* rows, const char* toMove = "white") {
  return std::string("5dchess-position 1\nsize: 8\nrules: double-step castling\nto-move: ") + toMove + "\nL0 T1" +
         (std::string(toMove) == "white" ? "w" : "b") + ": " + rows + "\n";
}

ai::Search::Status runToEnd(ai::Search& s, int budget = 300, int maxSteps = 1000000) {
  auto st = ai::Search::Status::Running;
  for (int i = 0; i < maxSteps and st == ai::Search::Status::Running; ++i) st = s.step(budget);
  return st;
}

// The turn applied to a clone: legal (submittable)?
bool legal(const IGame& game, const std::vector<Core::Move>& turn) {
  auto copy = game.clone();
  for (const Core::Move& m : turn) copy->makeMove(m);
  return copy->canSubmit();
}

// Plays the AI's turn on `game` and returns the result after the opponent's legal-turn proof.
GameResult play(IGame& game, ai::Options options) {
  REQUIRE(ai::playTurn(game, options));
  return game.result();
}

} // namespace

TEST_CASE("the AI always returns a legal turn: fuzz positions of every catalog mode, every level") {
  int decisions = 0, noTurn = 0;
  for (const std::string& mode : allModeIds()) {
    for (unsigned seed : {1u, 2u}) {
      std::mt19937 rng(seed * 7919u);
      auto game = newGame(mode);
      for (int turn = 0; turn < 7 and game->result() == GameResult::Ongoing; ++turn) {
        // Easy at every position; Normal / Hard (with a small budget so that Debug + ASan stays fast) on some.
        ai::Options opt{ai::Level::Easy, seed};
        if (turn == 2) opt = {ai::Level::Normal, seed, 3000};
        if (turn == 4) opt = {ai::Level::Hard, seed, 3000};
        ai::Search search(*game, opt);
        REQUIRE(runToEnd(search) == ai::Search::Status::Done);
        ++decisions;
        if (!search.hasTurn()) {
          // Reported "no legal turn": the engine's own search must agree (when it can decide cheaply).
          ++noTurn;
          CHECK(game->findLegalTurn(200000) != TurnSearch::Status::Found);
          break;
        }
        const auto turn_ = search.bestTurn();
        REQUIRE_FALSE(turn_.empty());
        CHECK(legal(*game, turn_));
        // Continue the game with the AI's turn (some turns with a random one, so that positions differ).
        if (turn % 3 == 2) {
          if (!buildRandomTurn(*game, rng, [](IGame&, Move&) {})) break;
        } else {
          for (const Core::Move& m : turn_) game->makeMove(m);
        }
        game->submitTurn();
        game->resolveResult(20000);
      }
    }
  }
  CHECK(decisions > 60);
  INFO("decisions without a legal turn: " << noTurn);
}

TEST_CASE("the AI finds a mate in one on a single board (every level that searches the reply)") {
  // Black king h8 boxed in by g7 / h7; White rook a1 gives Ra8#.
  for (ai::Level level : {ai::Level::Normal, ai::Level::Hard}) {
    auto game = fromText(singleBoard("7k/6pp/8/8/8/8/8/R6K"));
    ai::Search search(*game, {level, 3});
    REQUIRE(runToEnd(search) == ai::Search::Status::Done);
    REQUIRE(search.hasTurn());
    CHECK(search.progress().mateFound);
    CHECK(search.progress().bestScore >= ai::MateScore - 10);
    REQUIRE(search.bestTurn().size() == 1);
    CHECK(search.bestTurn()[0].to.y == 7);
    CHECK(play(*game, {level, 3}) == GameResult::WhiteWins);
  }
}

TEST_CASE("the AI finds a mate that needs a move on both of two boards (cross-board)") {
  // Two identical timelines, White must move on both. Rh1 on one board is check but not mate: the Black king can jump to the
  // other board's h7 (all its other neighbours are Black's own pieces). Only Rh1 on BOTH boards mates.
  const std::string text =
      "5dchess-position 1\nsize: 8\nrules: double-step castling\nto-move: white\n"
      "L0 T1w: 6rk/6p1/8/8/8/8/4K3/R7\n"
      "L1 T1w: 6rk/6p1/8/8/8/8/4K3/R7\n";
  {
    auto check = fromText(text); // sanity: the mate exists, and one rook alone does not mate
    check->makeMove(parseMove("(L0T1)a1>(L0T1)h1", PieceColor::PIECEWHITE));
    check->makeMove(parseMove("(L1T1)a1>(L1T1)h1", PieceColor::PIECEWHITE));
    REQUIRE(check->canSubmit());
    check->submitTurn();
    check->resolveResult();
    REQUIRE(check->result() == GameResult::WhiteWins);
    auto half = fromText(text);
    half->makeMove(parseMove("(L0T1)a1>(L0T1)h1", PieceColor::PIECEWHITE));
    half->makeMove(parseMove("(L1T1)e2>(L1T1)e3", PieceColor::PIECEWHITE));
    REQUIRE(half->canSubmit());
    half->submitTurn();
    half->resolveResult();
    REQUIRE(half->result() == GameResult::Ongoing);
  }
  for (ai::Level level : {ai::Level::Normal, ai::Level::Hard}) {
    auto game = fromText(text);
    REQUIRE(game->mandatoryBoards().size() == 2);
    ai::Search search(*game, {level, 5});
    REQUIRE(runToEnd(search) == ai::Search::Status::Done);
    REQUIRE(search.hasTurn());
    CHECK(search.bestTurn().size() == 2);
    CHECK(search.progress().mateFound);
    CHECK(play(*game, {level, 5}) == GameResult::WhiteWins);
  }
}

TEST_CASE("the AI finds a mate that is a jump into the past") {
  // L0 has three boards. On the tip (T2w, half-turn 2) the h-file is blocked by a black pawn on h7; on T1w it is open.
  // The White rook h1 jumps back to T1w (same square, one turn earlier): the new timeline's board has the rook attacking
  // the black king h8, whose neighbours g8, g7 (and h7 on L0's T1b board, where a king could jump to) are all its own pieces
  // or on the rook's file: mate.
  const std::string text =
      "5dchess-position 1\nsize: 8\nrules: double-step castling\nto-move: white\n"
      "L0 T1w: 6rk/6p1/8/8/8/8/8/K7\n"
      "L0 T1b: 6rk/6pp/8/8/8/8/8/K7\n"
      "L0 T2w: 6rk/6pp/8/8/8/8/8/K6R\n";
  {
    auto check = fromText(text); // sanity: the mate exists
    check->makeMove(parseMove("(L0T2)h1>(L0T1)h1", PieceColor::PIECEWHITE));
    REQUIRE(check->canSubmit());
    check->submitTurn();
    check->resolveResult();
    REQUIRE(check->result() == GameResult::WhiteWins);
  }
  for (ai::Level level : {ai::Level::Normal, ai::Level::Hard}) {
    auto game = fromText(text);
    ai::Search search(*game, {level, 9});
    REQUIRE(runToEnd(search) == ai::Search::Status::Done);
    REQUIRE(search.hasTurn());
    REQUIRE(search.bestTurn().size() == 1);
    const Core::Move m = search.bestTurn()[0];
    CHECK(m.to.t < m.from.t); // a jump
    CHECK(search.progress().mateFound);
    CHECK(play(*game, {level, 9}) == GameResult::WhiteWins);
  }
}

TEST_CASE("the AI does not hang its queen") {
  // The White queen d4 is attacked by the black pawn e5, which is protected by d6: Qxe5 loses the queen too.
  // After the AI's turn no black move may capture a white queen.
  for (ai::Level level : {ai::Level::Normal, ai::Level::Hard}) {
    for (std::uint64_t seed : {1u, 2u}) {
      auto game = fromText(singleBoard("4k3/8/3p4/4p3/3Q4/8/8/4K3"));
      REQUIRE(ai::playTurn(*game, {level, seed, 8000}));
      REQUIRE(game->result() == GameResult::Ongoing);
      bool queenCapturable = false;
      for (const Move& m : game->allPseudoLegalMoves()) {
        const Cell target = m.to.board->cell(m.to.position.x(), m.to.position.y());
        if (!target.empty() and target.type() == PieceType::Queen) queenCapturable = true;
      }
      CHECK_FALSE(queenCapturable);
    }
  }
}

TEST_CASE("the AI takes a hanging queen") {
  // A black queen stands unprotected on d5 next to the White rook's file.
  auto game = fromText(singleBoard("4k3/8/8/3q4/8/8/8/3RK3"));
  ai::Search search(*game, {ai::Level::Normal, 1});
  REQUIRE(runToEnd(search) == ai::Search::Status::Done);
  REQUIRE(search.bestTurn().size() == 1);
  CHECK(search.bestTurn()[0].to.x == 3);
  CHECK(search.bestTurn()[0].to.y == 4);
}

TEST_CASE("the search is deterministic for a fixed seed, and the seed matters only through the randomness among near-best turns") {
  auto game = newGame("standard");
  for (ai::Level level : {ai::Level::Easy, ai::Level::Normal}) {
    ai::Search a(*game, {level, 17, 20000}), b(*game, {level, 17, 20000});
    // Different step sizes must not matter either.
    REQUIRE(runToEnd(a, 50) == ai::Search::Status::Done);
    REQUIRE(runToEnd(b, 5000) == ai::Search::Status::Done);
    CHECK(a.bestTurn() == b.bestTurn());
    CHECK(a.progress().nodes == b.progress().nodes);
  }
  // Easy picks randomly among near-best turns: over several seeds more than one turn comes up.
  std::vector<std::vector<Core::Move>> seen;
  for (std::uint64_t seed = 1; seed <= 12; ++seed) {
    ai::Search s(*game, {ai::Level::Easy, seed});
    runToEnd(s);
    if (std::find(seen.begin(), seen.end(), s.bestTurn()) == seen.end()) seen.push_back(s.bestTurn());
  }
  CHECK(seen.size() > 1);
}

TEST_CASE("step() respects its node budget and never blocks: many small steps reach the same answer") {
  for (const std::string& mode : {std::string("standard"), std::string("timeline-battle")}) {
    auto game = newGame(mode);
    ai::Search search(*game, {ai::Level::Normal, 4, 30000});
    long long last = 0;
    int steps = 0;
    while (search.step(40) == ai::Search::Status::Running) {
      const long long now = search.progress().nodes;
      CHECK(now - last <= 40 + 32); // a step may finish the slice it is in (a 32-node proof slice or one move's cost)
      CHECK(search.progress().fraction < 1.0);
      CHECK_FALSE(search.hasTurn()); // nothing is offered before Done
      CHECK(search.bestTurn().empty());
      last = now;
      REQUIRE(++steps < 100000);
    }
    CHECK(search.progress().nodes - last <= 40 + 32);
    CHECK(steps > 5);
    CHECK(search.progress().fraction == 1.0);
    REQUIRE(search.hasTurn());
    CHECK(legal(*game, search.bestTurn()));
    CHECK(search.step(10) == ai::Search::Status::Done); // stays done
  }
}

TEST_CASE("a position without a legal turn is reported as such") {
  // Black to move, mated on the board by Ra8.
  auto game = fromText(singleBoard("R6k/6pp/8/8/8/8/8/K7", "black"));
  ai::Search search(*game, {ai::Level::Normal, 1});
  REQUIRE(runToEnd(search) == ai::Search::Status::Done);
  CHECK_FALSE(search.hasTurn());
  CHECK(search.bestTurn().empty());
}

TEST_CASE("the AI continues pending moves and leaves the game it was given untouched") {
  auto game = newGame("standard");
  const std::string before = snapshot(*game);
  ai::Search search(*game, {ai::Level::Easy, 2});
  runToEnd(search);
  CHECK(snapshot(*game) == before);
  REQUIRE(search.hasTurn());
  // A game with a pending move: the result continues it.
  game->makeMove(Core::Move{{4, 1, 0, 0}, {4, 3, 0, 0}});
  ai::Search more(*game, {ai::Level::Easy, 2});
  runToEnd(more);
  if (more.hasTurn()) CHECK(legal(*game, more.bestTurn()));
}

TEST_CASE("self-play smoke: Hard (small budget) scores at least as well as Easy over a few short games") {
  double hard = 0;
  const int games = 6;
  for (int g = 0; g < games; ++g) {
    const bool hardWhite = g % 2 == 0;
    auto game = newGame("standard");
    for (int turn = 0; turn < 12 and game->result() == GameResult::Ongoing; ++turn) {
      const bool whiteToMove = game->getCurrentTurnColor() == PieceColor::PIECEWHITE;
      const bool isHard = whiteToMove == hardWhite;
      ai::Options opt = isHard ? ai::Options{ai::Level::Hard, std::uint64_t(g * 100 + turn + 1), 4000}
                               : ai::Options{ai::Level::Easy, std::uint64_t(g * 100 + turn + 1)};
      if (!ai::playTurn(*game, opt, nullptr, 500, 20000)) break;
    }
    const PieceColor hardColor = hardWhite ? PieceColor::PIECEWHITE : PieceColor::PIECEBLACK;
    switch (game->result()) {
      case GameResult::WhiteWins: hard += hardWhite ? 1 : 0; break;
      case GameResult::BlackWins: hard += hardWhite ? 0 : 1; break;
      case GameResult::Draw: hard += 0.5; break;
      case GameResult::Ongoing: {
        const int e = ai::evaluate(*game, hardColor);
        hard += e > 150 ? 1 : e < -150 ? 0 : 0.5;
        break;
      }
    }
  }
  INFO("Hard scored " << hard << " / " << games);
  CHECK(hard >= games / 2.0); // loose on purpose; see tools/ai_bench for the real numbers
}
