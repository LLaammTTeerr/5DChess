#include <doctest/doctest.h>

#include "play/Selection.h"
#include "test_support.h"

using namespace Chess;
using namespace Chess::Core;
using namespace test;
using play::Intent;
using play::Selection;

namespace {
const Coord e2{4, 1, 0, 0}, e3{4, 2, 0, 0}, e4{4, 3, 0, 0}, d2{3, 1, 0, 0}, d3{3, 2, 0, 0}, d4{3, 3, 0, 0};
const Coord e7{4, 6, 0, 0}, e5{4, 4, 0, 0};

bool hasTarget(const Selection& s, const Coord& c) {
  return std::find(s.targets().begin(), s.targets().end(), c) != s.targets().end();
}
} // namespace

TEST_CASE("Selection: clicking nothing, an empty square or an enemy piece does nothing while idle") {
  auto game = newGame("standard");
  Selection s;
  CHECK(s.click(std::nullopt, *game).kind == Intent::Kind::None);
  CHECK(s.click(Coord{4, 4, 0, 0}, *game).kind == Intent::Kind::None); // empty square
  CHECK(s.click(e7, *game).kind == Intent::Kind::None);                // Black's pawn, White to move
  CHECK_FALSE(s.active());
}

TEST_CASE("Selection: picking up a piece lists its legal targets") {
  auto game = newGame("standard");
  Selection s;
  const Intent intent = s.click(e2, *game);
  CHECK(intent.kind == Intent::Kind::Select);
  CHECK(intent.from == e2);
  REQUIRE(s.active());
  CHECK(*s.from() == e2);
  CHECK(s.targets().size() == 2);
  CHECK(hasTarget(s, e3));
  CHECK(hasTarget(s, e4));
}

TEST_CASE("Selection: clicking another own piece switches the selection") {
  auto game = newGame("standard");
  Selection s;
  s.click(e2, *game);
  const Intent intent = s.click(d2, *game);
  CHECK(intent.kind == Intent::Kind::Select);
  CHECK(*s.from() == d2);
  CHECK(hasTarget(s, d3));
  CHECK(hasTarget(s, d4));
  CHECK_FALSE(hasTarget(s, e3));
}

TEST_CASE("Selection: a click on an empty or enemy square keeps the selection, a click on the piece clears it") {
  auto game = newGame("standard");
  Selection s;
  s.click(e2, *game);
  CHECK(s.click(Coord{0, 4, 0, 0}, *game).kind == Intent::Kind::None);
  CHECK(s.click(e7, *game).kind == Intent::Kind::None);
  CHECK(s.click(std::nullopt, *game).kind == Intent::Kind::None);
  CHECK(s.active());
  CHECK(s.click(e2, *game).kind == Intent::Kind::Clear);
  CHECK_FALSE(s.active());
  CHECK(s.targets().empty());
}

TEST_CASE("Selection: clicking a target yields the move and returns to idle") {
  auto game = newGame("standard");
  Selection s;
  s.click(e2, *game);
  const Intent intent = s.click(e4, *game);
  REQUIRE(intent.kind == Intent::Kind::Move);
  CHECK(intent.move.from == e2);
  CHECK(intent.move.to == e4);
  CHECK_FALSE(s.active());
  game->makeMove(intent.move);
  CHECK(game->undoable());
}

TEST_CASE("Selection: boards the player can no longer move on cannot be picked from") {
  auto game = newGame("standard");
  game->makeMove(Core::Move{e2, e4}); // White's pawn moved: the t=0 board is history, the new board is Black's
  Selection s;
  CHECK(s.click(d2, *game).kind == Intent::Kind::None);                       // White piece on the old board
  CHECK(s.click(Coord{3, 1, 1, 0}, *game).kind == Intent::Kind::None);        // White piece, but board is Black's turn
  CHECK(s.click(Coord{4, 6, 1, 0}, *game).kind == Intent::Kind::None);        // Black piece, Black's board, White to move
  CHECK(s.click(Coord{0, 0, 9, 0}, *game).kind == Intent::Kind::None);        // no such board
  CHECK_FALSE(s.active());
}

TEST_CASE("Selection: a promoting pawn has one target square and four moves; the click picks the Queen") {
  Sandbox game(5);
  game.place(0, 0, 3, make(PieceType::Pawn, PieceColor::PIECEWHITE));
  game.place(0, 4, 0, make(PieceType::King, PieceColor::PIECEWHITE));
  game.place(0, 4, 4, make(PieceType::King, PieceColor::PIECEBLACK));
  const Coord pawn{0, 3, 0, 0}, last{0, 4, 0, 0};
  Selection s;
  REQUIRE(s.click(pawn, game).kind == Intent::Kind::Select);
  CHECK(s.moves().size() == 4);
  CHECK(s.targets() == std::vector<Coord>{last});
  const Intent intent = s.click(last, game);
  REQUIRE(intent.kind == Intent::Kind::Move);
  CHECK(intent.move.promotion == PieceType::Queen);
}

TEST_CASE("Selection: clear() drops the selection") {
  Sandbox game(4);
  game.place(0, 0, 0, make(PieceType::King, PieceColor::PIECEWHITE));
  game.place(0, 3, 3, make(PieceType::King, PieceColor::PIECEBLACK));
  Selection s;
  REQUIRE(s.click(Coord{0, 0, 0, 0}, game).kind == Intent::Kind::Select);
  s.clear();
  CHECK_FALSE(s.active());
}

TEST_CASE("Selection: a move to another timeline is a Move intent onto that board") {
  Sandbox game(5, 2);
  game.place(0, 0, 0, make(PieceType::Rook, PieceColor::PIECEWHITE));
  const Coord rook{0, 0, 0, 0}, across{0, 0, 0, 1};
  Selection s;
  REQUIRE(s.click(rook, game).kind == Intent::Kind::Select);
  CHECK(std::find(s.targets().begin(), s.targets().end(), across) != s.targets().end());
  const Intent intent = s.click(across, game);
  REQUIRE(intent.kind == Intent::Kind::Move);
  CHECK(intent.move.from == rook);
  CHECK(intent.move.to == across);
}

namespace {
struct FinishedGame : Sandbox {
  FinishedGame() : Sandbox(5) { _result = GameResult::WhiteWins; }
};
} // namespace

TEST_CASE("Selection: a finished game gives no intent") {
  FinishedGame game;
  game.place(0, 0, 0, make(PieceType::Rook, PieceColor::PIECEWHITE));
  Selection s;
  CHECK(s.click(Coord{0, 0, 0, 0}, game).kind == Intent::Kind::None);
  CHECK_FALSE(s.active());
}
