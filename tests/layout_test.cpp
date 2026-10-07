#include <doctest/doctest.h>

#include "play/BoardLayout.h"
#include "test_support.h"

using namespace Chess;
using namespace Chess::Core;
using namespace test;
using play::BoardLayout;
using play::Rect;

TEST_CASE("BoardLayout: boards sit on a grid of half-turn columns and timeline rows; White's (positive) timelines are above") {
  const Rect origin = BoardLayout::boardRect(0, 0);
  CHECK(origin.x == 0);
  CHECK(origin.y == 0);
  CHECK(origin.w == BoardLayout::kBoardSize);
  const Rect later = BoardLayout::boardRect(0, 3);
  CHECK(later.x == 3 * BoardLayout::kPitch);
  CHECK(later.y == origin.y);
  CHECK(BoardLayout::boardRect(2, 0).y == -2 * BoardLayout::kPitch);
  CHECK(BoardLayout::boardRect(2, 0).y < origin.y);
  CHECK(BoardLayout::boardRect(-1, 0).y == BoardLayout::kPitch);
  CHECK(BoardLayout::boardRect(-1, 0).y > origin.y);
}

TEST_CASE("BoardLayout: squares have White at the bottom and the a-file on the left") {
  const Rect board = BoardLayout::boardRect(0, 0);
  const Rect a1 = BoardLayout::squareRect(board, 8, 0, 0);
  const Rect h8 = BoardLayout::squareRect(board, 8, 7, 7);
  CHECK(a1.x == board.x);
  CHECK(a1.y + a1.h == doctest::Approx(board.y + board.h));
  CHECK(h8.x + h8.w == doctest::Approx(board.x + board.w));
  CHECK(h8.y == board.y);
}

TEST_CASE("BoardLayout: hitTest inverts squareRect on every square of every board, for several board sizes") {
  for (int dim : {5, 6, 8}) {
    CAPTURE(dim);
    Sandbox game(dim, std::vector<int>{3, 2}, 4);
    game.addCreatedTimeLine(-2, 2);
    game.addCreatedTimeLine(2, 1);
    BoardLayout layout;
    REQUIRE(layout.sync(game));
    CHECK(layout.dim() == dim);
    CHECK(layout.minTimeline() == -2);
    CHECK(layout.maxTimeline() == 2);
    CHECK(layout.boards().size() == 3 + 2 + 2 + 1);

    for (const auto& slot : layout.boards()) {
      for (int x = 0; x < dim; ++x) {
        for (int y = 0; y < dim; ++y) {
          const Coord expected{int8_t(x), int8_t(y), int16_t(slot.halfTurn), int16_t(slot.timeline)};
          const Rect sq = BoardLayout::squareRect(slot.rect, dim, x, y);
          const auto hit = layout.hitTest(sq.centerX(), sq.centerY());
          REQUIRE(hit.has_value());
          CHECK(*hit == expected);
          // a point just inside the top-left corner belongs to the square, the point just left of it does not
          const auto corner = layout.hitTest(sq.x + 0.01f, sq.y + 0.01f);
          REQUIRE(corner.has_value());
          CHECK(*corner == expected);
          const auto left = layout.hitTest(sq.x - 0.01f, sq.centerY());
          CHECK((!left.has_value() || *left != expected));
        }
      }
    }
  }
}

TEST_CASE("BoardLayout: points between boards, outside them and on missing boards hit nothing") {
  Sandbox game(8, std::vector<int>{2, 1}, 2);
  BoardLayout layout;
  layout.sync(game);
  const Rect b = BoardLayout::boardRect(0, 0);
  CHECK(layout.hitTest(b.x + b.w + BoardLayout::kSpacing / 2, b.y + 10).has_value() == false); // gap to the right
  CHECK(layout.hitTest(b.x + 10, b.y + b.h + BoardLayout::kSpacing / 2).has_value() == false);  // gap below
  CHECK_FALSE(layout.hitTest(-5, 10).has_value());
  CHECK_FALSE(layout.hitTest(10, -5).has_value());
  const Rect missing = BoardLayout::boardRect(1, 1); // timeline 1 has only its half-turn 0 board
  CHECK_FALSE(layout.hitTest(missing.x + 10, missing.y + 10).has_value());
  const Rect farAway = BoardLayout::boardRect(40, 40);
  CHECK_FALSE(layout.hitTest(farAway.x + 10, farAway.y + 10).has_value());
  CHECK(layout.contains(0, 1));
  CHECK_FALSE(layout.contains(1, 1));
}

TEST_CASE("BoardLayout: bounds cover every board; sync rebuilds only when the game state changes") {
  auto game = newGame("standard");
  BoardLayout layout;
  CHECK(layout.sync(*game));
  CHECK_FALSE(layout.sync(*game));
  CHECK(layout.boards().size() == 1);
  CHECK(layout.bounds().w == BoardLayout::kBoardSize);

  game->makeMove(Core::Move{Coord{4, 1, 0, 0}, Coord{4, 3, 0, 0}});
  CHECK(layout.sync(*game));
  CHECK(layout.boards().size() == 2);
  CHECK(layout.bounds().w == BoardLayout::kPitch + BoardLayout::kBoardSize);
  CHECK_FALSE(layout.sync(*game));
  game->undo();
  CHECK(layout.sync(*game));
  CHECK(layout.boards().size() == 1);
}

TEST_CASE("BoardLayout: boardAt hits the whole card (frame and label strip), find and cardBounds agree") {
  Sandbox game(8, std::vector<int>{3, 2}, 3); // timeline 0: half-turns 0..2, timeline 1: 0..1
  BoardLayout layout;
  layout.sync(game);
  const auto board = BoardLayout::boardRect(0, 1);
  const Rect card = BoardLayout::cardRect(board);
  const auto onSquare = layout.boardAt(board.x + 10, board.y + 10);
  REQUIRE(onSquare.has_value());
  CHECK(onSquare->timeline == 0);
  CHECK(onSquare->halfTurn == 1);
  const float strip = board.y + board.h + BoardLayout::kCardFooter - 2; // the label strip: on the card, on no square
  const auto onLabel = layout.boardAt(board.x + 100, strip);
  REQUIRE(onLabel.has_value());
  CHECK(onLabel->halfTurn == 1);
  CHECK_FALSE(layout.hitTest(board.x + 100, strip).has_value());
  const auto onFrame = layout.boardAt(card.x + 2, card.y + 2);
  REQUIRE(onFrame.has_value());
  CHECK(onFrame->halfTurn == 1);
  CHECK_FALSE(layout.boardAt(card.x + card.w + 5, card.centerY()).has_value()); // the gap to the next card
  const Rect missing = BoardLayout::boardRect(1, 2);
  CHECK_FALSE(layout.boardAt(missing.x + 10, missing.y + 10).has_value());
  CHECK_FALSE(layout.boardAt(-500, -500).has_value());

  CHECK(layout.find(0, 2).has_value());
  CHECK_FALSE(layout.find(1, 2).has_value());
  const auto all = layout.cardBounds();
  REQUIRE(all.has_value());
  const Rect first = BoardLayout::cardRect(BoardLayout::boardRect(1, 0)), last = BoardLayout::cardRect(BoardLayout::boardRect(0, 2));
  CHECK(all->x == doctest::Approx(first.x));
  CHECK(all->y == doctest::Approx(first.y));
  CHECK(all->x + all->w == doctest::Approx(last.x + last.w));
  CHECK(all->y + all->h == doctest::Approx(last.y + last.h));
  BoardLayout empty;
  CHECK_FALSE(empty.cardBounds().has_value());
}

TEST_CASE("BoardLayout: neighbour moves a cursor between existing boards") {
  using Dir = BoardLayout::Dir;
  Sandbox game(8, std::vector<int>{4, 2}, 3); // timeline 0: half-turns 0..3, timeline 1: 0..1
  game.addCreatedTimeLine(-1, 1);
  BoardLayout layout;
  layout.sync(game);
  auto at = [&](int l, int t) { return *layout.find(l, t); };

  CHECK(layout.neighbour(at(0, 1), Dir::Right)->halfTurn == 2);
  CHECK(layout.neighbour(at(0, 1), Dir::Left)->halfTurn == 0);
  CHECK_FALSE(layout.neighbour(at(0, 3), Dir::Right).has_value());
  CHECK_FALSE(layout.neighbour(at(0, 0), Dir::Left).has_value());
  // Up: the higher timeline id; on it the board with the closest half-turn
  const auto up = layout.neighbour(at(0, 3), Dir::Up);
  REQUIRE(up.has_value());
  CHECK(up->timeline == 1);
  CHECK(up->halfTurn == 1);
  const auto down = layout.neighbour(at(0, 2), Dir::Down);
  REQUIRE(down.has_value());
  CHECK(down->timeline == -1);
  CHECK(down->halfTurn == 0);
  CHECK_FALSE(layout.neighbour(at(1, 0), Dir::Up).has_value());
  CHECK_FALSE(layout.neighbour(at(-1, 0), Dir::Down).has_value());
  CHECK(layout.neighbour(at(1, 1), Dir::Down)->timeline == 0);
  CHECK(layout.neighbour(at(1, 1), Dir::Down)->halfTurn == 1);
  CHECK(layout.neighbour(at(-1, 0), Dir::Up)->timeline == 0);
  CHECK(layout.neighbour(at(-1, 0), Dir::Up)->halfTurn == 0);
}

TEST_CASE("BoardLayout: neighbour breaks a tie towards the earlier half-turn") {
  Sandbox game(8, std::vector<int>{2, 4}, 3); // timeline 1: half-turns 0..3; from (0, 1)... timeline 0 holds 0 and 1
  BoardLayout layout;
  layout.sync(game);
  // From (1, 3) down: timeline 0 holds half-turns 0 and 1, so 1 is the nearer; from (1, 0) down the nearest is 0
  CHECK(layout.neighbour(*layout.find(1, 3), BoardLayout::Dir::Down)->halfTurn == 1);
  CHECK(layout.neighbour(*layout.find(1, 0), BoardLayout::Dir::Down)->halfTurn == 0);
  Sandbox gap(8, std::vector<int>{1, 3}, 2); // timeline 0: only half-turn 0; timeline 1: 0..2
  BoardLayout other;
  other.sync(gap);
  CHECK(other.neighbour(*other.find(1, 2), BoardLayout::Dir::Down)->halfTurn == 0);
}

TEST_CASE("BoardLayout: columnBounds covers the cards of the half-turns asked for") {
  Sandbox game(8, std::vector<int>{4, 2}, 3);
  BoardLayout layout;
  layout.sync(game);
  const auto col = layout.columnBounds(1, 1);
  REQUIRE(col.has_value());
  const Rect top = BoardLayout::cardRect(BoardLayout::boardRect(1, 1)), bottom = BoardLayout::cardRect(BoardLayout::boardRect(0, 1));
  CHECK(col->x == doctest::Approx(top.x));
  CHECK(col->w == doctest::Approx(top.w));
  CHECK(col->y == doctest::Approx(top.y));
  CHECK(col->y + col->h == doctest::Approx(bottom.y + bottom.h));
  const auto wide = layout.columnBounds(0, 3);
  REQUIRE(wide.has_value());
  CHECK(wide->w == doctest::Approx(3 * BoardLayout::kPitch + top.w));
  CHECK(layout.columnBounds(1, 3)->x == doctest::Approx(top.x));
  CHECK_FALSE(layout.columnBounds(7, 9).has_value());
  // columns beyond the field are ignored, the ones inside kept
  CHECK(layout.columnBounds(-2, 1)->x == doctest::Approx(BoardLayout::cardRect(BoardLayout::boardRect(0, 0)).x));
}
