// Directed positions for the legal-turn search and for the time-travel rules around it.
#include <doctest/doctest.h>

#include "test_support.h"

using namespace Chess;
using namespace test;

namespace {

// Black to move on `boards` identical 8x8 boards: king h8, pawn g7 (and h7 unless `escape`), White rook a8 giving check.
// `bystanders` adds pawns with harmless moves (the kind of position that made the old exhaustive search give up).
class MateRow : public IGame {
public:
  MateRow(int boards, bool bystanders, bool escape) : IGame(8) {
    for (int id = 0; id < boards; ++id) {
      auto line = _addTimeLine(std::make_shared<TimeLine>(8, id));
      for (int h = 0; h < 2; ++h) line->pushBack(std::make_shared<Board>(8, id, h));
    }
    _presentHalfTurn = 1;
    _currentTurnColor = PieceColor::PIECEBLACK;
    for (int id = 0; id < boards; ++id) {
      auto b = timeLine(id)->back();
      b->placePiece({7, 7}, std::make_shared<King>(PieceColor::PIECEBLACK));
      b->placePiece({6, 6}, std::make_shared<Pawn>(PieceColor::PIECEBLACK));
      if (!escape) b->placePiece({7, 6}, std::make_shared<Pawn>(PieceColor::PIECEBLACK));
      if (bystanders) {
        for (int x = 1; x <= 5; ++x) b->placePiece({x, 2}, std::make_shared<Pawn>(PieceColor::PIECEBLACK));
      }
      b->placePiece({0, 7}, std::make_shared<Rook>(PieceColor::PIECEWHITE));
      b->placePiece({0, 0}, std::make_shared<King>(PieceColor::PIECEWHITE));
    }
  }
};

TurnSearch::Status runToEnd(TurnSearch& s, int budget = 5000, int maxSteps = 100000) {
  TurnSearch::Status st = TurnSearch::Status::Running;
  for (int i = 0; i < maxSteps and st == TurnSearch::Status::Running; ++i) st = s.step(budget);
  return st;
}

void checkTurn(const IGame& original, const TurnSearch& search) {
  auto copy = original.clone();
  for (const auto& step : search.turn()) copy->makeMove(step.move, step.promotion);
  CHECK(copy->canSubmit());
}

} // namespace

TEST_CASE("a mate on 1 to 4 identical boards is proven quickly; with an escape square a legal turn is found") {
  for (int boards = 1; boards <= 4; ++boards) {
    for (bool bystanders : {false, true}) {
      CAPTURE(boards);
      CAPTURE(bystanders);
      MateRow mate(boards, bystanders, false);
      TurnSearch search(mate);
      CHECK(runToEnd(search) == TurnSearch::Status::None);
      CHECK(search.nodes() < 5000);
      CHECK(search.inCheck());

      // Before the search was rewritten this kind of position came back "unknown" although a legal turn exists.
      MateRow escape(boards, bystanders, true);
      TurnSearch found(escape);
      REQUIRE(runToEnd(found) == TurnSearch::Status::Found);
      CHECK(found.nodes() < 5000);
      checkTurn(escape, found);
      CHECK(found.turn().size() >= std::size_t(boards));
    }
  }
}

TEST_CASE("a 3-board mate is decided in a few small steps, and a pending game result is resolved by stepping") {
  MateRow mate(3, true, false);
  TurnSearch search(mate);
  int steps = 0;
  while (search.step(200) == TurnSearch::Status::Running) ++steps;
  CHECK(steps < 20);
  CHECK(search.status() == TurnSearch::Status::None);

  // Through the game: White gives the mate on a single board, submitTurn() arms the search, stepping decides.
  Sandbox game(4);
  game.place(0, 0, 0, make<King>(PieceColor::PIECEWHITE));
  game.place(0, 3, 0, make<Rook>(PieceColor::PIECEWHITE));
  game.place(0, 0, 3, make<King>(PieceColor::PIECEBLACK));
  game.place(0, 0, 2, make<Pawn>(PieceColor::PIECEBLACK));
  game.place(0, 1, 2, make<Pawn>(PieceColor::PIECEBLACK));
  game.makeMove(Move{SelectedPosition(game.tip(0), Position2D(3, 0)), SelectedPosition(game.tip(0), Position2D(3, 3))});
  game.submitTurn();
  CHECK(game.result() == GameResult::Ongoing);
  int frames = 0;
  while (game.stepResultSearch(50)) ++frames;
  CHECK(frames < 5);
  CHECK(game.result() == GameResult::WhiteWins);
}

TEST_CASE("a jump into the past from another board clears the obligation to move on the other present boards") {
  // Both timelines end at half-turn 2 (White to move): both are mandatory. A White knight on timeline 1 jumps back to the
  // first board of its own timeline. That creates a timeline whose latest board ends at half-turn 1, so the present moves
  // back to Black's turn: nothing has to be played on timeline 0 any more (it is only "optional" from then on).
  Sandbox game(4, std::vector<int>{3, 3}, 2);
  game.place(0, 0, 0, make<King>(PieceColor::PIECEWHITE));
  game.place(0, 3, 3, make<King>(PieceColor::PIECEBLACK));
  game.place(1, 0, 0, make<Knight>(PieceColor::PIECEWHITE));
  game.place(1, 3, 3, make<King>(PieceColor::PIECEBLACK));
  REQUIRE(game.mandatoryBoards().size() == 2);

  // Moving on timeline 1 only (an ordinary move) is not enough.
  {
    auto plain = game.clone();
    plain->makeMove(Move{SelectedPosition(game.tip(1), Position2D(0, 0)), SelectedPosition(game.tip(1), Position2D(1, 2))});
    CHECK_FALSE(plain->canSubmit());
    CHECK(plain->mandatoryBoards().size() == 1);
  }
  // The jump is.
  auto past = game.boardAt(1, 0);
  Move jump{SelectedPosition(game.tip(1), Position2D(0, 0)), SelectedPosition(past, Position2D(2, 0))};
  REQUIRE(contains(movesAt(game, game.tip(1), 0, 0), past, 2, 0));
  game.makeMove(jump);
  CHECK(game.timeLineCount() == 3);
  CHECK(game.bufferHalfTurn() == 1);
  CHECK(game.mandatoryBoards().empty());
  CHECK(game.canSubmit());

  // The search finds exactly that kind of turn when asked from the start position.
  Sandbox start(4, std::vector<int>{3, 3}, 2);
  start.place(0, 0, 0, make<King>(PieceColor::PIECEWHITE));
  start.place(0, 3, 3, make<King>(PieceColor::PIECEBLACK));
  start.place(1, 0, 0, make<Knight>(PieceColor::PIECEWHITE));
  start.place(1, 3, 3, make<King>(PieceColor::PIECEBLACK));
  TurnSearch search(start);
  REQUIRE(runToEnd(search) == TurnSearch::Status::Found);
  checkTurn(start, search);
}

TEST_CASE("a pawn captures one timeline forward and a full turn ahead in time, through makeMove") {
  // Timeline 0 is ahead (boards h0..h4); the pawn stands on the latest board of timeline 1 (h2) and captures the black
  // piece on the same square of timeline 0's latest board. Forward on the timeline axis for White is towards lower IDs.
  Sandbox game(5, std::vector<int>{5, 3}, 4);
  game.place(1, 2, 1, make<Pawn>(PieceColor::PIECEWHITE));
  game.place(0, 2, 1, make<Rook>(PieceColor::PIECEBLACK));
  auto moves = movesAt(game, game.tip(1), 2, 1);
  REQUIRE(contains(moves, game.tip(0), 2, 1));
  Move m{SelectedPosition(game.tip(1), Position2D(2, 1)), SelectedPosition(game.tip(0), Position2D(2, 1))};
  auto oldTip0 = game.tip(0);
  game.makeMove(m);
  CHECK(game.timeLine(0)->back() != oldTip0);
  CHECK(game.timeLine(0)->back()->getPiece({2, 1})->type() == PieceType::Pawn);
  CHECK(game.timeLine(1)->back()->getPiece({2, 1}) == nullptr);
}

TEST_CASE("a move on an inactive timeline changes nothing about the present, also through a full submit") {
  // Timeline 0 and White's first timeline 1 are active; White's second timeline 2 is inactive (Black has made none).
  // Its latest board ends before the present but must not pull the present back.
  Sandbox game(4, std::vector<int>{3}, 2);
  game.addCreatedTimeLine(1, 3);
  game.addCreatedTimeLine(2, 1); // one board only: it ends at half-turn 0
  REQUIRE(game.isTimeLineActive(1));
  REQUIRE_FALSE(game.isTimeLineActive(2));
  for (int id : {0, 1, 2}) game.place(id, 0, 0, make<Knight>(PieceColor::PIECEWHITE));
  CHECK(game.bufferHalfTurn() == 2);
  CHECK(game.mandatoryBoards().size() == 2);
  game.makeMove(Move{SelectedPosition(game.tip(0), Position2D(0, 0)), SelectedPosition(game.tip(0), Position2D(1, 2))});
  game.makeMove(Move{SelectedPosition(game.tip(1), Position2D(0, 0)), SelectedPosition(game.tip(1), Position2D(1, 2))});
  REQUIRE(game.canSubmit());
  game.makeMove(Move{SelectedPosition(game.tip(2), Position2D(0, 0)), SelectedPosition(game.tip(2), Position2D(1, 2))});
  CHECK(game.canSubmit());
  CHECK(game.bufferHalfTurn() == 3);
  game.submitTurn();
  CHECK(game.presentHalfTurn() == 3);
  CHECK(game.getCurrentTurnColor() == PieceColor::PIECEBLACK);
  CHECK(game.timeLine(2)->halfTurnNumber() == 1);
}
