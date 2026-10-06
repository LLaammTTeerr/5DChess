// The Guide's pages: every position file loads, every "Try it" goal is reachable by an intended move or turn and is not
// met by a wrong one (the goal predicates are what the Guide screen shows its check mark from).
#include <doctest/doctest.h>
#include <cstring>
#include "guide/Guide.h"
#include "test_support.h"

using namespace Chess;

namespace {

Core::Coord at(int l, int t, const char* square) {
  return Core::Coord{static_cast<int8_t>(square[0] - 'a'), static_cast<int8_t>(square[1] - '1'), static_cast<int16_t>(t),
                     static_cast<int16_t>(l)};
}

const guide::Page& page(int number) { return guide::pages().at(static_cast<size_t>(number - 1)); }

std::shared_ptr<IGame> open(int number) {
  static const bool directorySet = (guide::setDirectory(FDCHESS_GUIDE_DIR), true);
  (void)directorySet;
  test::newGame("standard"); // sets the catalog directory for the last page
  return guide::load(page(number));
}

/// Plays `from` -> `to` (timeline, half-turn, square on each side) through the rules, like a click would; fails the test
/// when the move is not offered.
void play(IGame& game, int l, int t, const char* from, int l2, int t2, const char* to, PieceType promotion = PieceType::Queen) {
  Core::Move move{at(l, t, from), at(l2, t2, to), promotion};
  const auto offered = game.legalMovesFrom(move.from);
  INFO("move " << from << " -> " << to);
  REQUIRE(std::find(offered.begin(), offered.end(), move) != offered.end());
  game.makeMove(move);
}

void submit(IGame& game) {
  REQUIRE(game.canSubmit());
  game.submitTurn();
  game.resolveResult();
}

bool met(int number, const IGame& game) {
  REQUIRE(page(number).goal != nullptr);
  return page(number).goal->met(game);
}

} // namespace

TEST_CASE("guide: ten pages, each with a title, text and a position that loads") {
  REQUIRE(guide::pages().size() == 10);
  for (size_t i = 0; i < guide::pages().size(); ++i) {
    const guide::Page& p = guide::pages()[i];
    INFO("page " << i + 1 << ": " << p.title);
    CHECK(std::strlen(p.title) > 0);
    CHECK(std::strlen(p.text) > 40);
    for (const char* s : {p.title, p.text, p.note})
      for (const char* c = s; *c; ++c) CHECK(static_cast<unsigned char>(*c) < 0x80); // the UI fonts only cover ASCII
    CHECK((*p.position != 0) != (*p.mode != 0)); // exactly one source
    auto game = open(static_cast<int>(i) + 1);
    REQUIRE(game != nullptr);
    CHECK(game->result() == GameResult::Ongoing);
    if (p.goal) {
      CHECK(std::strlen(p.goal->prompt) > 0);
      CHECK(std::strlen(p.goal->hint) > 0);
      CHECK(std::strlen(p.goal->success) > 0);
      CHECK_FALSE(p.goal->met(*game)); // a goal must not be met before the player does anything
    }
  }
  CHECK(page(10).startsGame);
  CHECK(page(10).goal == nullptr);
}

TEST_CASE("guide page 1: submitting a turn meets the goal, a pending move does not") {
  auto game = open(1);
  CHECK(game->history().empty());
  CHECK(game->presentHalfTurn() == 2);
  play(*game, 0, 2, "g1", 0, 2, "f3");
  CHECK_FALSE(met(1, *game));
  CHECK(guide::anyMoveMade(*game));
  submit(*game);
  CHECK(met(1, *game));
}

TEST_CASE("guide page 2: the knight jumping back in time makes a timeline; other moves do not") {
  {
    auto game = open(2);
    play(*game, 0, 2, "b1", 0, 2, "c3"); // same board
    CHECK_FALSE(met(2, *game));
    game->undo();
    play(*game, 0, 2, "d3", 0, 2, "d4"); // a pawn
    CHECK_FALSE(met(2, *game));
  }
  auto game = open(2);
  play(*game, 0, 2, "b1", 0, 0, "b3"); // two squares along the rank axis, one turn back
  CHECK(game->timeLineCount() == 2);
  CHECK(met(2, *game));
}

TEST_CASE("guide page 3: Black jumping back creates timeline -1 below L0, White's is L+1") {
  auto game = open(3);
  CHECK(game->getCurrentTurnColor() == PieceColor::PIECEBLACK);
  CHECK(game->hasTimeLine(1));
  CHECK_FALSE(game->hasTimeLine(-1));
  play(*game, 1, 1, "b8", 1, 1, "c6"); // a quiet knight move on L+1
  CHECK_FALSE(met(3, *game));
  game->undo();
  play(*game, 0, 3, "b8", 0, 1, "b6");
  CHECK(game->hasTimeLine(-1));
  CHECK(met(3, *game));
}

TEST_CASE("guide page 4: both present boards must be moved on before Submit; L+2 is inactive and optional") {
  auto game = open(4);
  CHECK_FALSE(game->isTimeLineActive(2));
  CHECK(game->isTimeLineActive(1));
  CHECK(game->mandatoryBoards().size() == 2);
  play(*game, 0, 4, "a2", 0, 4, "a3");
  CHECK_FALSE(game->canSubmit());
  CHECK_FALSE(met(4, *game));
  play(*game, 1, 4, "a2", 1, 4, "a3");
  submit(*game);
  CHECK(met(4, *game));
}

TEST_CASE("guide page 5: moving to another board meets the goal, a same-board move does not") {
  {
    auto game = open(5);
    play(*game, 1, 2, "a2", 1, 2, "a3");
    CHECK_FALSE(met(5, *game));
  }
  auto game = open(5);
  // the queen reaches the same square of the earlier White board of the neighbouring timeline
  const auto moves = game->legalMovesFrom(at(1, 2, "d1"));
  const auto other = std::find_if(moves.begin(), moves.end(), [](const Core::Move& m) { return m.to.l != 1 || m.to.t != 2; });
  REQUIRE(other != moves.end());
  game->makeMove(*other);
  CHECK(met(5, *game));
}

TEST_CASE("guide page 6: capturing with a pawn meets the goal, pushing it does not") {
  {
    auto game = open(6);
    play(*game, 0, 4, "e4", 0, 4, "e5");
    CHECK_FALSE(met(6, *game));
  }
  auto game = open(6);
  play(*game, 0, 4, "e4", 0, 4, "d5");
  CHECK(met(6, *game));
}

TEST_CASE("guide page 7: the king is in check from another timeline; moving it away meets the goal") {
  auto game = open(7);
  CHECK(game->inCheck());
  CHECK_FALSE(game->canSubmit());
  play(*game, 1, 4, "h3", 1, 4, "h4"); // only L+1 moved: the king on L0 is still attacked
  CHECK_FALSE(game->canSubmit());
  CHECK_FALSE(met(7, *game));
  play(*game, 0, 4, "e1", 0, 4, "d1");
  submit(*game);
  CHECK(met(7, *game));
}

TEST_CASE("guide page 8: Ra8 is checkmate, another rook move is not") {
  {
    auto game = open(8);
    play(*game, 0, 0, "a1", 0, 0, "a7");
    submit(*game);
    CHECK_FALSE(met(8, *game));
    CHECK(game->result() == GameResult::Ongoing);
  }
  auto game = open(8);
  play(*game, 0, 0, "a1", 0, 0, "a8");
  submit(*game);
  CHECK(game->result() == GameResult::WhiteWins);
  CHECK(met(8, *game));
}

TEST_CASE("guide page 9: castling and en passant are on offer, promoting meets the goal") {
  auto game = open(9);
  const auto kingMoves = game->legalMovesFrom(at(0, 4, "e1"));
  for (const char* target : {"g1", "c1"})
    CHECK(std::any_of(kingMoves.begin(), kingMoves.end(), [&](const Core::Move& m) { return m.to == at(0, 4, target); }));
  const auto pawnMoves = game->legalMovesFrom(at(0, 4, "e5"));
  CHECK(std::any_of(pawnMoves.begin(), pawnMoves.end(), [](const Core::Move& m) { return m.to == at(0, 4, "d6"); }));

  play(*game, 0, 4, "e1", 0, 4, "g1"); // castling is not the goal
  CHECK_FALSE(met(9, *game));
  game->undo();
  play(*game, 0, 4, "b7", 0, 4, "b8", PieceType::Knight);
  CHECK(met(9, *game));
}
