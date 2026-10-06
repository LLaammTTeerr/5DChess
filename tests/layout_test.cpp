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
