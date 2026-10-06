#include <doctest/doctest.h>

#include "test_support.h"

#include "engine/GameCatalog.h"
#include "engine/Notation.h"
#include "engine/Position.h"

#include <random>

using namespace Chess;
using namespace Chess::Core;
using namespace test;
using VMove = Chess::Core::Move;

namespace {

// The state a record must reproduce: every board, the bookkeeping, the history and the result.
void checkSameGame(const IGame& a, const IGame& b) {
  CHECK(snapshot(a) == snapshot(b));
  CHECK(Position::fromGame(a) == Position::fromGame(b));
  CHECK(a.history() == b.history());
  CHECK(a.result() == b.result());
  CHECK(a.startPosition() == b.startPosition());
}

// Plays up to `turns` random legal turns.
void playRandom(IGame& game, unsigned seed, int turns) {
  std::mt19937 rng(seed);
  for (int t = 0; t < turns and game.result() == GameResult::Ongoing; ++t) {
    if (!buildRandomTurn(game, rng, [](const IGame&, const Chess::Move&) {})) break;
    game.submitTurn();
    game.resolveResult(50000);
  }
}

// Throws something other than ParseError: that is the bug these tests look for.
bool failsCleanly(const std::string& text) {
  try {
    loadRecord(text);
  } catch (const ParseError&) {
    return true;
  } catch (...) {
    FAIL("loadRecord threw something other than ParseError for: " << text);
  }
  return false; // it loaded
}

const std::string kOpening =
    "5dchess-record 1\n"
    "mode: standard\n"
    "T1w: (L0T1)e2>(L0T1)e4\n"
    "T1b: (L0T1)e7>(L0T1)e5\n"
    "T2w: (L0T2)b1>(L0T2)c3\n";

} // namespace

TEST_CASE("notation: a move is (L<l>T<turn>)<file><rank>>(L<l>T<turn>)<file><rank>[=piece]") {
  const VMove m{Coord{4, 1, 6, 0}, Coord{4, 3, 4, 1}};
  CHECK(toNotation(m) == "(L0T4)e2>(L1T3)e4");
  CHECK(parseMove("(L0T4)e2>(L1T3)e4", PieceColor::PIECEWHITE) == m);
  // Black's boards are the odd half-turns: the same text names turn 3 of Black's side.
  const VMove b{Coord{0, 7, 7, -1}, Coord{0, 0, 7, -2}};
  CHECK(toNotation(b) == "(L-1T4)a8>(L-2T4)a1");
  CHECK(parseMove("(L-1T4)a8>(L-2T4)a1", PieceColor::PIECEBLACK) == b);
  // promotion
  const VMove q{Coord{4, 6, 2, 0}, Coord{4, 7, 2, 0}, PieceType::Rook};
  CHECK(toNotation(q) == "(L0T2)e7>(L0T2)e8=R");
  CHECK(toNotation(VMove{q.from, q.to}, true) == "(L0T2)e7>(L0T2)e8=Q");
  CHECK(toNotation(VMove{q.from, q.to}) == "(L0T2)e7>(L0T2)e8");
  CHECK(parseMove("(L0T2)e7>(L0T2)e8=R", PieceColor::PIECEWHITE) == q);
  CHECK(parseMove("(L0T2)e7>(L0T2)e8=Q", PieceColor::PIECEWHITE).promotion == PieceType::Queen);
  // files are the engine's x: a = 0 ... h = 7
  CHECK(parseMove("(L0T1)h8>(L0T1)a1", PieceColor::PIECEWHITE) == VMove{Coord{7, 7, 0, 0}, Coord{0, 0, 0, 0}});
}

TEST_CASE("notation: every Core::Move round-trips through text") {
  std::mt19937 rng(7);
  for (int i = 0; i < 2000; ++i) {
    const PieceColor mover = randInt(rng, 0, 1) ? PieceColor::PIECEBLACK : PieceColor::PIECEWHITE;
    auto coord = [&] {
      return Coord{int8_t(randInt(rng, 0, 7)), int8_t(randInt(rng, 0, 7)),
                   int16_t(2 * randInt(rng, 0, 9999) + int(mover)), int16_t(randInt(rng, -1000, 1000))};
    };
    static const PieceType promos[] = {PieceType::Queen, PieceType::Rook, PieceType::Bishop, PieceType::Knight};
    const VMove m{coord(), coord(), promos[randInt(rng, 0, 3)]};
    CHECK(parseMove(toNotation(m), mover) == m);
  }
}

TEST_CASE("notation: malformed moves are parse errors") {
  for (const char* text :
       {"", "e2>e4", "(L0T1)e2", "(L0T1)e2>", "(L0T1)e2>(L0T1)", "(L0T1)e2>(L0T1)e", "(L0T1)i2>(L0T1)e4",
        "(L0T1)e0>(L0T1)e4", "(L0T1)e9>(L0T1)e4", "(L0T1)E2>(L0T1)e4", "(L0T1)e2>(L0T1)e4=", "(L0T1)e2>(L0T1)e4=K",
        "(L0T1)e2>(L0T1)e4=P", "(L0T1)e2>(L0T1)e4=q", "(L0T1)e2>(L0T1)e4 ", " (L0T1)e2>(L0T1)e4", "(L0T1)e2>(L0T1)e4x",
        "(LT0)e2>(L0T1)e4", "(L0T)e2>(L0T1)e4", "(L-T0)e2>(L0T1)e4", "(L--1T0)e2>(L0T1)e4", "(L0T0)e2>(L0T1)e4",
        "(L1001T1)e2>(L0T1)e4", "(L-1001T1)e2>(L0T1)e4", "(L0T10002)e2>(L0T1)e4", "(L99999999999T1)e2>(L0T1)e4",
        "(L0T1)e2<(L0T1)e4", "(L0T0]e2>(L0T1)e4", "(L0T0)e2>(L0T1)e4", "(L++1T1)e2>(L0T1)e4"}) {
    CAPTURE(text);
    CHECK_THROWS_AS(parseMove(text, PieceColor::PIECEWHITE), ParseError);
  }
}

TEST_CASE("record: format of a short game, and it replays") {
  auto game = loadRecord(kOpening);
  REQUIRE(game != nullptr);
  CHECK(game->modeId() == "standard");
  CHECK(game->history().size() == 3);
  CHECK(game->presentHalfTurn() == 3);
  CHECK(game->getCurrentTurnColor() == PieceColor::PIECEBLACK);
  // x = 4 is the engine's queen file: the pawn that moved is the one in front of the queen (docs/NOTATION.md).
  const auto& tip = *game->timeLine(0)->back();
  CHECK(tip.at({4, 3}).has_value());
  CHECK(tip.at({4, 3})->type == PieceType::Pawn);
  CHECK(tip.at({2, 2})->type == PieceType::Knight);
  CHECK(writeRecord(*game) == kOpening);
}

TEST_CASE("record: moves into the past, negative timelines and promotion") {
  // White's knight jumps back to the first board (a new timeline L1), Black answers with a move on L1 ...
  auto game = newGame("standard");
  playRandom(*game, 11, 12);
  const std::string text = writeRecord(*game);
  auto copy = loadRecord(text);
  checkSameGame(*game, *copy);
  CHECK(writeRecord(*copy) == text);

  // promotion: a pawn on its last-but-one rank, with both a push and a capture; the suffix is mandatory
  const std::string position =
      "5dchess-position 1\nsize: 4\nrules: none\nto-move: white\n"
      "L0 T1w: 1r1k/P3/4/K3\n";
  auto base = parsePosition(position).makeGame();
  const std::string record = "5dchess-record 1\nposition:\n" + writePosition(parsePosition(position)) + "end-position\n";
  auto promoted = loadRecord(record + "T1w: (L0T1)a3>(L0T1)a4=N\n");
  CHECK(promoted->timeLine(0)->back()->at({0, 3})->type == PieceType::Knight);
  CHECK(promoted->history()[0].moves[0].promotes);
  CHECK(promoted->history()[0].moves[0].move.promotion == PieceType::Knight);
  CHECK(writeRecord(*promoted) == record + "T1w: (L0T1)a3>(L0T1)a4=N\n");
  auto queen = loadRecord(record + "T1w: (L0T1)a3>(L0T1)a4=Q\n");
  CHECK(writeRecord(*queen) == record + "T1w: (L0T1)a3>(L0T1)a4=Q\n");
  CHECK(failsCleanly(record + "T1w: (L0T1)a3>(L0T1)a4\n"));    // a promotion needs its suffix
  CHECK(failsCleanly(record + "T1w: (L0T1)a1>(L0T1)b1=Q\n"));  // and a plain move must not have one
}

TEST_CASE("record: random games of every mode replay to the identical game") {
  int records = 0, withBranches = 0, decided = 0;
  for (const std::string& mode : allModeIds()) {
    for (unsigned seed : {1u, 2u, 3u}) {
      CAPTURE(mode);
      CAPTURE(seed);
      auto game = newGame(mode);
      playRandom(*game, seed * 31, 14);
      const std::string text = writeRecord(*game);
      auto copy = loadRecord(text);
      REQUIRE(copy != nullptr);
      checkSameGame(*game, *copy);
      CHECK(writeRecord(*copy) == text);
      ++records;
      withBranches += game->timeLineCount() > int(Position::fromGame(*newGame(mode)).timelines.size());
      decided += game->result() != GameResult::Ongoing;
    }
  }
  CHECK(records == 27);
  CHECK(withBranches > 0);
  MESSAGE("records " << records << ", with new timelines " << withBranches << ", decided " << decided);
}

TEST_CASE("record: a game started from a mid-game position embeds that position") {
  auto game = newGame("timeline-battle");
  playRandom(*game, 5, 6);
  auto mid = Position::fromGame(*game, "mid").makeGame();
  CHECK(mid->history().empty());
  playRandom(*mid, 9, 6);
  const std::string text = writeRecord(*mid);
  CHECK(text.find("position:") != std::string::npos);
  CHECK(text.find("mode:") == std::string::npos);
  auto copy = loadRecord(text);
  checkSameGame(*mid, *copy);
}

TEST_CASE("record: a game that is not built from a position has no record") {
  Sandbox sandbox(4);
  CHECK_THROWS_AS(writeRecord(sandbox), std::logic_error);
}

TEST_CASE("record: wrong records fail cleanly with a line number") {
  auto message = [](const std::string& text) {
    try {
      loadRecord(text);
    } catch (const ParseError& e) {
      return std::string(e.what());
    }
    return std::string("loaded");
  };
  CHECK(message("") != "loaded");
  CHECK(message("hello\n") != "loaded");
  CHECK(message("5dchess-record 2\nmode: standard\n") != "loaded");
  CHECK(message("5dchess-record 1\n") != "loaded");
  CHECK(message("5dchess-record 1\nmode: nope\n").find("line 2") == 0);
  CHECK(message("5dchess-record 1\nmode: standard\nT2w: (L0T2)e2>(L0T2)e4\n").find("turn label") != std::string::npos);
  CHECK(message("5dchess-record 1\nmode: standard\nT1w: (L0T1)e2>(L0T1)e5\n").find("illegal") != std::string::npos);
  CHECK(message("5dchess-record 1\nmode: standard\nT1w: (L0T1)e7>(L0T1)e5\n").find("illegal") != std::string::npos);  // Black's pawn
  CHECK(message("5dchess-record 1\nmode: standard\nT1w:\n").find("at least one move") != std::string::npos);
  CHECK(message("5dchess-record 1\nmode: standard\nT1w: (L0T1)e2>(L0T1)e4 (L0T1)d2>(L0T1)d4\n") != "loaded"); // two moves on one board
  CHECK(message("5dchess-record 1\nmode: standard\nT1w: (L0T1)e2>(L0T1)e4\nT1w: (L0T1)e7>(L0T1)e5\n").find("turn label") != std::string::npos);
  CHECK(message("5dchess-record 1\nposition:\n5dchess-position 1\nsize: 8\n").find("without 'end-position'") != std::string::npos);
  CHECK(message("5dchess-record 1\nposition:\nnonsense\nend-position\n").find("position block") != std::string::npos);
  CHECK(message("5dchess-record 1\nmode: standard\nmode: standard\n") != "loaded");
  CHECK(message(kOpening) == "loaded");
  // comments and blank lines are fine
  CHECK(message("# a record\n5dchess-record 1\n\nmode: standard\n# first turn\nT1w: (L0T1)e2>(L0T1)e4\n") == "loaded");
}

TEST_CASE("record: a mate ends the record; nothing may follow it") {
  // White mates with Qc1-c4 (the king on b3 protects the queen, the black king on d4 has no square).
  const std::string header = "5dchess-record 1\nposition:\n" +
                             writePosition(parsePosition("5dchess-position 1\nsize: 4\nrules: none\nto-move: white\n"
                                                         "L0 T1w: 3k/1K2/4/2Q1\n")) + "end-position\n";
  const std::string mate = header + "T1w: (L0T1)c1>(L0T1)c4\n";
  auto game = loadRecord(mate);
  CHECK(game->result() == GameResult::WhiteWins);
  CHECK(writeRecord(*game) == mate);
  CHECK(failsCleanly(mate + "T1b: (L0T1)d4>(L0T1)d3\n"));
  CHECK(failsCleanly(mate + "T1b: (L0T1)d4>(L0T1)d3\n"));
}

TEST_CASE("record: truncated and corrupted records never crash (run under ASan/UBSan)") {
  auto game = newGame("standard");
  playRandom(*game, 3, 8);
  const std::string text = writeRecord(*game);
  // every prefix: a prefix that ends inside a token is an error, one that ends on a line boundary still loads
  int loaded = 0;
  for (size_t n = 0; n <= text.size(); n += 3) loaded += !failsCleanly(text.substr(0, n));
  CHECK(loaded > 0);
  // random corruption: replace, insert or delete a few characters
  std::mt19937 rng(99);
  static const char alphabet[] = "()LT>=-0123456789abcdefghwbQRBNKP: \n#x";
  int stillValid = 0, rejected = 0;
  for (int i = 0; i < 500; ++i) {
    std::string s = text;
    for (int k = randInt(rng, 1, 4); k > 0 and !s.empty(); --k) {
      const size_t at = size_t(randInt(rng, 0, int(s.size()) - 1));
      switch (randInt(rng, 0, 2)) {
        case 0: s[at] = alphabet[randInt(rng, 0, int(sizeof alphabet) - 2)]; break;
        case 1: s.insert(s.begin() + long(at), alphabet[randInt(rng, 0, int(sizeof alphabet) - 2)]); break;
        default: s.erase(at, 1); break;
      }
    }
    (failsCleanly(s) ? rejected : stillValid) += 1;
  }
  CHECK(rejected > 50);
  MESSAGE("corrupted records: " << rejected << " rejected, " << stillValid << " still valid");
}

TEST_CASE("record: parsing is bounded") {
  // too large
  CHECK(failsCleanly(std::string(5u << 20, '#')));
  // too many turns (each line is also illegal after the first, but the cap must hit before any work is done)
  std::string many = "5dchess-record 1\nmode: standard\n";
  for (int i = 0; i < 6000; ++i) many += "T1w: (L0T1)e2>(L0T1)e4\n";
  CHECK(failsCleanly(many));
  // too many moves in one turn
  std::string longTurn = "5dchess-record 1\nmode: standard\nT1w:";
  for (int i = 0; i < 400; ++i) longTurn += " (L0T1)e2>(L0T1)e4";
  CHECK(failsCleanly(longTurn));
  // huge numbers and hostile embedded positions
  CHECK(failsCleanly("5dchess-record 1\nmode: standard\nT1w: (L99999999999999999999T1)e2>(L0T1)e4\n"));
  CHECK(failsCleanly("5dchess-record 1\nposition:\n5dchess-position 1\nsize: 8\nrules: none\nto-move: white\nL100000000 T1w: 8/8/8/8/8/8/8/8\nend-position\n"));
  CHECK(failsCleanly("5dchess-record 1\nposition:\n5dchess-position 1\nsize: 99999999999\nend-position\n"));
  // no terminating newline, lone CR, NUL bytes
  CHECK(!failsCleanly("5dchess-record 1\nmode: standard"));
  CHECK(failsCleanly(std::string("5dchess-record 1\nmode: standard\nT1w: (L0T1)e2>(L0T1)e4\0\n", 56)));
  CHECK(!failsCleanly("5dchess-record 1\r\nmode: standard\r\nT1w: (L0T1)e2>(L0T1)e4\r\n"));   // CRLF line ends load
}
