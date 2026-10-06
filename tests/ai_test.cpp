// Tests of the AI opponent engine (include/ai, docs/AI.md): legality of what it returns, mate finding, safety, determinism,
// the node cap, the fallback, the step budget, and a self-play property test.
#include <doctest/doctest.h>

#include "ai/Eval.h"
#include "ai/Play.h"
#include "ai/Search.h"
#include "ai/TurnGen.h"
#include "engine/Notation.h"
#include "engine/Position.h"
#include "test_support.h"

#include <algorithm>
#include <fstream>
#include <sstream>
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

std::shared_ptr<IGame> fixture(const char* name) {
  std::ifstream in(std::string(FDCHESS_AI_POSITIONS_DIR) + "/" + name);
  REQUIRE(in.good());
  std::stringstream text;
  text << in.rdbuf();
  return fromText(text.str());
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
      CHECK(now - last <= 40 + 200); // a step may finish the slice it is in (one move's cost, or a proof slice: 32 nodes charged x the position's weight)
      CHECK(search.progress().fraction < 1.0);
      CHECK_FALSE(search.hasTurn()); // nothing is offered before Done
      CHECK(search.bestTurn().empty());
      last = now;
      REQUIRE(++steps < 100000);
    }
    CHECK(search.progress().nodes - last <= 40 + 200);
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

  // Two boards to move on, one move already made: the result is exactly the missing move, on the other board.
  const std::string text =
      "5dchess-position 1\nsize: 8\nrules: double-step castling\nto-move: white\n"
      "L0 T1w: 4k3/8/8/8/8/8/4P3/4K3\n"
      "L1 T1w: 4k3/8/8/8/8/8/3P4/4K3\n";
  for (ai::Level level : {ai::Level::Easy, ai::Level::Normal, ai::Level::Hard}) {
    auto two = fromText(text);
    REQUIRE(two->mandatoryBoards().size() == 2);
    const Core::Move first = parseMove("(L0T1)e2>(L0T1)e3", PieceColor::PIECEWHITE);
    two->makeMove(first);
    REQUIRE_FALSE(two->canSubmit());
    ai::Search more(*two, {level, 2, 3000});
    REQUIRE(runToEnd(more) == ai::Search::Status::Done);
    REQUIRE(more.hasTurn());
    CHECK_FALSE(more.progress().usedFallback);
    REQUIRE(more.bestTurn().size() == 1);
    CHECK(more.bestTurn()[0].from.l == 1);
    CHECK(legal(*two, more.bestTurn()));
    CHECK(two->pendingMoves().size() == 1); // untouched
  }
}

TEST_CASE("self-play against random turns: every AI turn is legal, the AI is never mated and ends ahead on material") {
  // Fixed seeds, short games, small budgets. The random side plays any legal turn; a search with even a little depth must stay
  // ahead of it on material overall and must never be mated.
  const char* modes[] = {"standard", "timeline-battle", "knight-vs-bishop"};
  int game = 0, total = 0;
  for (const char* mode : modes) {
    for (ai::Level level : {ai::Level::Normal, ai::Level::Hard}) {
      std::mt19937 rng(1000u + unsigned(game));
      auto g = newGame(mode);
      const bool aiWhite = game % 2 == 0;
      const PieceColor ai = aiWhite ? PieceColor::PIECEWHITE : PieceColor::PIECEBLACK;
      for (int turn = 0; turn < 10 and g->result() == GameResult::Ongoing; ++turn) {
        if (g->getCurrentTurnColor() == ai) {
          if (!ai::playTurn(*g, {level, std::uint64_t(game * 100 + turn + 1), 4000}, nullptr, 500, 20000)) break;
        } else {
          if (!buildRandomTurn(*g, rng, [](IGame&, Move&) {})) break;
          g->submitTurn();
          g->resolveResult(20000);
        }
      }
      INFO(std::string(mode) << ", AI plays " << (aiWhite ? "white" : "black") << ", level " << int(level));
      CHECK(g->result() != (aiWhite ? GameResult::BlackWins : GameResult::WhiteWins));
      const int e = ai::evaluate(*g, ai);
      CHECK(e > -300); // not behind by more than a minor piece in any single game (about 2000 cp = a queen of lead is typical)
      total += e;
      ++game;
    }
  }
  CHECK(total > 1000); // over six games (measured: about 6000): an average lead of well over a pawn per game
}

TEST_CASE("a quiet jump from an optional board is found without the fallback, at every level") {
  // White (to move on L0 T1w, the only mandatory board) is mated on that board: Ra1 checks Kh1, g1 is on the rook's line, no
  // move of L0 helps and none of its pieces may travel. L1 (ahead of the present, so optional) holds White's bishop a1 and rook
  // g1: Ba1 jumps to L0 and takes the rook; Rg1 jumps to g1 on L0 and blocks. Those jumps are the only legal turns, and they
  // are moves of a board the generator is not obliged to look at.
  const std::string text =
      "5dchess-position 1\nsize: 8\nrules: none\nto-move: white\n"
      "L0 T1w: 7k/8/8/8/8/8/6PP/r6K\n"
      "L1 T2w: 4k3/8/8/8/8/8/6PP/B3K1RR\n";
  {
    auto check = fromText(text);
    REQUIRE(check->mandatoryBoards().size() == 1);
    REQUIRE(check->getMoveableBoards().size() == 2);
    REQUIRE(check->findLegalTurn(100000) == TurnSearch::Status::Found);
    for (int y = 0; y < 8; ++y) {
      for (int x = 0; x < 8; ++x) {
        const Cell c = check->getBoard(0, 0)->cell(x, y);
        if (c.empty() or c.color() != PieceColor::PIECEWHITE) continue;
        for (const Core::Move& m : check->legalMovesFrom({int8_t(x), int8_t(y), 0, 0})) {
          auto copy = check->clone();
          copy->makeMove(m);
          CHECK_FALSE(copy->canSubmit()); // nothing on the mandatory board resolves the check
        }
      }
    }
  }
  for (ai::Level level : {ai::Level::Easy, ai::Level::Normal, ai::Level::Hard}) {
    auto game = fromText(text);
    ai::Search search(*game, {level, 3});
    REQUIRE(runToEnd(search) == ai::Search::Status::Done);
    REQUIRE(search.hasTurn());
    CHECK_FALSE(search.progress().usedFallback);
    CHECK(legal(*game, search.bestTurn()));
    REQUIRE(search.bestTurn().size() == 1);
    CHECK(search.bestTurn()[0].from.l == 1); // a move of the optional board
    CHECK(search.progress().depth >= 1);
  }
}

TEST_CASE("a free piece is taken with a time jump at every level (Easy included)") {
  // L0 has three boards; the present is the tip T2w. A black queen stands unprotected on c1 of T1w; it is gone from T1b on (Black's own time travel cannot bring it back). The
  // White knight a1 on the tip jumps back one turn and two files (a knight move across time) and takes it there, creating a
  // timeline: worth a queen at every level. No move on the tip itself wins anything.
  const std::string text =
      "5dchess-position 1\nsize: 8\nrules: none\nto-move: white\n"
      "L0 T1w: 4k3/8/8/8/8/8/8/N1q4K\n"
      "L0 T1b: 4k3/8/8/8/8/8/8/N6K\n"
      "L0 T2w: 4k3/8/8/8/8/8/8/N6K\n";
  const Core::Coord target{2, 0, 0, 0};
  {
    auto base = fromText(text);
    REQUIRE(base->mandatoryBoards().size() == 1);
    bool exists = false;
    for (const Core::Move& m : base->legalMovesFrom({0, 0, 2, 0})) exists = exists or m.to == target;
    REQUIRE(exists);
  }
  for (ai::Level level : {ai::Level::Easy, ai::Level::Normal, ai::Level::Hard}) {
    auto game = fromText(text);
    ai::Search search(*game, {level, 1, level == ai::Level::Easy ? 0 : 60000});
    REQUIRE(runToEnd(search) == ai::Search::Status::Done);
    REQUIRE(search.hasTurn());
    CHECK_FALSE(search.progress().usedFallback);
    REQUIRE(search.bestTurn().size() == 1);
    if (level == ai::Level::Hard) { // equally good lines exist for a deep search (take it a move later): it must see the win
      CHECK(search.progress().bestScore > 500);
      continue;
    }
    INFO("level " << int(level) << " plays to (" << int(search.bestTurn()[0].to.x) << "," << int(search.bestTurn()[0].to.y) << ") t" << search.bestTurn()[0].to.t << " l" << search.bestTurn()[0].to.l << " score " << search.progress().bestScore);
    CHECK(search.bestTurn()[0].to == target);
  }
}

TEST_CASE("the node limit caps the whole decision, also on big multiverses") {
  // Fixtures: 15-18 timelines with 2, 13 and 14 mandatory boards (some in check). Every level, first pass and probes and turn
  // generation included, stays within 1.5x of its cap; Easy at its real cap, the others at small caps so that Debug + ASan
  // stays fast. (A cap below the cost of one complete turn on 14 boards, about 10-40k nodes, cannot be honoured: the first
  // turn is always evaluated.)
  struct Case { ai::Level level; long long cap; };
  for (const char* name : {"big-13-boards.5dp", "big-2-boards.5dp", "big-14-boards-a.5dp", "big-14-boards-b.5dp"}) {
    for (const Case& c : {Case{ai::Level::Easy, 10000}, Case{ai::Level::Normal, 60000}, Case{ai::Level::Hard, 60000}}) {
      auto game = fixture(name);
      REQUIRE(game->timeLineCount() >= 15);
      ai::Search search(*game, {c.level, 1, c.level == ai::Level::Easy ? 0 : c.cap});
      REQUIRE(runToEnd(search, 500) == ai::Search::Status::Done);
      INFO(std::string(name) << " level " << int(c.level) << " nodes " << search.progress().nodes);
      CHECK(search.progress().nodes <= c.cap * 3 / 2);
      REQUIRE(search.hasTurn());
      CHECK(legal(*game, search.bestTurn()));
    }
  }
}


TEST_CASE("underpromotions are found: a knight promotion that mates, and one that avoids stalemate") {
  // White d7-d8=N gives check to the black king c6, whose every square is covered: mate. d8=Q is no check and leaves Black
  // without a move: stalemate. Only the knight promotion wins.
  const std::string mate =
      "5dchess-position 1\nsize: 8\nrules: none\nto-move: white\n"
      "L0 T1w: 1R6/R2P4/2k5/8/1P6/8/8/3RK3\n";
  {
    auto game = fromText(mate);
    bool knightMates = false, queenMates = false;
    for (const Core::Move& m : game->legalMovesFrom({3, 6, 0, 0})) {
      if (m.to.y != 7) continue;
      auto c = game->clone();
      c->makeMove(m);
      c->submitTurn();
      c->resolveResult();
      if (c->result() == GameResult::WhiteWins) (m.promotion == PieceType::Knight ? knightMates : queenMates) = true;
    }
    REQUIRE(knightMates);
    REQUIRE_FALSE(queenMates);
  }
  for (ai::Level level : {ai::Level::Normal, ai::Level::Hard}) {
    auto game = fromText(mate);
    ai::Search search(*game, {level, 1});
    REQUIRE(runToEnd(search) == ai::Search::Status::Done);
    REQUIRE(search.hasTurn());
    REQUIRE(search.bestTurn().size() == 1);
    CHECK(search.bestTurn()[0].promotion == PieceType::Knight);
    CHECK(search.progress().mateFound);
  }
  // Whatever the position, the generator must offer every promotion piece once (the turn dedupe keys on the promotion).
  auto game = fromText("5dchess-position 1\nsize: 8\nrules: none\nto-move: white\nL0 T1w: 4k3/P7/8/8/8/8/8/4K3\n");
  ai::GenParams gp;
  gp.beam = 100;
  gp.deepBeam = 100;
  gp.maxTravel = ai::NoLimit;
  ai::TurnGen gen(game->clone(), gp);
  long long budget = 1 << 30;
  std::vector<PieceType> promoted;
  while (gen.advance(budget) == ai::TurnGen::Result::Leaf)
    for (const auto& pm : gen.game().pendingMoves())
      if (pm.promotes) promoted.push_back(pm.move.promotion);
  for (PieceType t : {PieceType::Queen, PieceType::Rook, PieceType::Bishop, PieceType::Knight})
    CHECK(std::count(promoted.begin(), promoted.end(), t) == 1);
}

TEST_CASE("when even the rescue generator finds nothing, the fallback turn is legal, scored, and independent of step size") {
  for (ai::Level level : {ai::Level::Easy, ai::Level::Normal}) {
    auto game = fixture("fallback.5dp");
    ai::Search ref(*game, {level, 1, 2000});
    REQUIRE(runToEnd(ref, 500) == ai::Search::Status::Done);
    REQUIRE(ref.hasTurn());
    CHECK(ref.progress().usedFallback);
    CHECK(legal(*game, ref.bestTurn()));
    CHECK(ref.progress().bestScore != 0); // evaluated, not blind
    for (int step : {1, 37, 100000}) {
      ai::Search other(*game, {level, 1, 2000});
      REQUIRE(runToEnd(other, step, 100000000) == ai::Search::Status::Done);
      CHECK(other.bestTurn() == ref.bestTurn());
      CHECK(other.progress().nodes == ref.progress().nodes);
      CHECK(other.progress().usedFallback);
    }
  }
}
