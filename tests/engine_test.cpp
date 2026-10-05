#include <doctest/doctest.h>

#include "test_support.h"

using namespace Chess;
using namespace test;

TEST_CASE("standard game: initial position") {
  StandardGame game;
  CHECK(game.dim() == 8);
  CHECK(game.presentHalfTurn() == 0);
  CHECK(game.getCurrentTurnColor() == PieceColor::PIECEWHITE);
  CHECK(game.result() == GameResult::Ongoing);
  CHECK_FALSE(game.undoable());
  REQUIRE(game.getTimeLines().size() == 1);
  REQUIRE(game.getMoveableBoards().size() == 1);

  // 16 pawn moves (single + double step) and 4 knight moves, exactly as in 2D chess.
  CHECK(game.allPseudoLegalMoves().size() == 20);
}

TEST_CASE("selecting an empty square or an enemy piece throws") {
  StandardGame game;
  auto board = game.getMoveableBoards()[0];
  CHECK_THROWS_AS(movesAt(game, board, 4, 4), std::runtime_error);
  CHECK_THROWS_AS(movesAt(game, board, 0, 7), std::runtime_error);
}

TEST_CASE("a move on the same board extends the timeline and leaves the old board untouched") {
  StandardGame game;
  auto before = game.getMoveableBoards()[0];
  game.makeMove({{before, {4, 1}}, {before, {4, 3}}});

  auto timeLine = game.getTimeLines()[0];
  REQUIRE(timeLine->size() == 2);
  auto after = timeLine->back();
  CHECK(after->halfTurnNumber() == 1);
  CHECK(after->getPiece({4, 1}) == nullptr);
  REQUIRE(after->getPiece({4, 3}) != nullptr);
  CHECK(after->getPiece({4, 3})->name() == "pawn");
  // Snapshot immutability: the source board still holds the pawn on its old square.
  CHECK(before->getPiece({4, 1}) != nullptr);
  CHECK(before->getPiece({4, 3}) == nullptr);
  CHECK(game.getNewBoard() == after);
}

TEST_CASE("submitTurn hands the move to the other side and advances the present") {
  StandardGame game;
  auto board = game.getMoveableBoards()[0];
  game.makeMove({{board, {4, 1}}, {board, {4, 3}}});
  CHECK(game.getMoveableBoards().empty());
  game.submitTurn();
  CHECK(game.getCurrentTurnColor() == PieceColor::PIECEBLACK);
  CHECK(game.presentHalfTurn() == 1);
  CHECK_FALSE(game.undoable());
  REQUIRE(game.getMoveableBoards().size() == 1);
  CHECK(game.getMoveableBoards()[0]->halfTurnNumber() == 1);
}

TEST_CASE("undo restores the exact pre-move state") {
  StandardGame game;
  const std::string start = snapshot(game);
  auto board = game.getMoveableBoards()[0];
  game.makeMove({{board, {6, 0}}, {board, {5, 2}}});
  CHECK(game.undoable());
  game.undo();
  CHECK(snapshot(game) == start);
}

TEST_CASE("a knight jumping back in time branches a new timeline") {
  StandardGame game;
  auto b0 = game.getMoveableBoards()[0];
  game.makeMove({{b0, {4, 1}}, {b0, {4, 3}}});
  game.submitTurn();
  auto b1 = game.getMoveableBoards()[0];
  game.makeMove({{b1, {4, 6}}, {b1, {4, 4}}});
  game.submitTurn();

  // White to move on the half-turn 2 board. The knight on (1, 0) can make a (y, z) jump:
  // two ranks forward and one full turn back, onto the empty square (1, 2) of the half-turn 0 board.
  auto b2 = game.getMoveableBoards()[0];
  REQUIRE(b2->halfTurnNumber() == 2);
  auto past = game.getBoard(0, 0);
  REQUIRE(past->halfTurnNumber() == 0);
  REQUIRE(past->getPiece({1, 2}) == nullptr);
  auto moves = movesAt(game, b2, 1, 0);
  REQUIRE(contains(moves, past, 1, 2));

  game.makeMove({{b2, {1, 0}}, {past, {1, 2}}});
  REQUIRE(game.getTimeLines().size() == 2);
  auto branch = game.getTimeLines()[1];
  CHECK(branch->ID() == 1);
  CHECK(branch->forkAt() == 0);
  CHECK(branch->parentId() == 0);
  REQUIRE(branch->size() == 1);
  CHECK(branch->back()->halfTurnNumber() == 1);
  REQUIRE(branch->back()->getPiece({1, 2}) != nullptr);
  CHECK(branch->back()->getPiece({1, 2})->name() == "knight");
  // The original past board was not modified.
  CHECK(past->getPiece({1, 2}) == nullptr);

  // Undo removes the new timeline again.
  game.undo();
  CHECK(game.getTimeLines().size() == 1);
}

// Rule replaced: the old engine ended the game when a king was captured. In the official rules a king is never
// captured; instead a turn after which the opponent could capture a king may not be submitted (see docs/RULES.md).
TEST_CASE("check rule: a turn that leaves a king capturable cannot be submitted (replaces king-capture-ends-game)") {
  Sandbox game(4);
  game.place(0, 0, 0, make<Rook>(PieceColor::PIECEBLACK));
  game.place(0, 3, 0, make<King>(PieceColor::PIECEWHITE));
  game.place(0, 0, 3, make<Knight>(PieceColor::PIECEWHITE));
  auto board = game.tip(0);
  // White's king is attacked along the first rank; moving the knight does not help.
  CHECK(game.inCheck());
  game.makeMove({{board, {0, 3}}, {board, {1, 1}}});
  CHECK_FALSE(game.canSubmit());
  CHECK_FALSE(game.threatsAgainst(PieceColor::PIECEWHITE).empty());
  game.undo();
  // Stepping the king out of the rook's line is fine.
  game.makeMove({{board, {3, 0}}, {board, {3, 1}}});
  CHECK(game.canSubmit());
  CHECK(game.threatsAgainst(PieceColor::PIECEWHITE).empty());
}

TEST_CASE("pawns promote to a queen on the last rank") {
  Sandbox game(4);
  game.place(0, 1, 2, make<Pawn>(PieceColor::PIECEWHITE));
  auto board = game.tip(0);
  game.makeMove({{board, {1, 2}}, {board, {1, 3}}});
  auto after = game.getNewBoard();
  REQUIRE(after->getPiece({1, 3}) != nullptr);
  CHECK(after->getPiece({1, 3})->name() == "queen");
  CHECK(after->getPiece({1, 3})->color() == PieceColor::PIECEWHITE);
}

TEST_CASE("a queen on a corner reaches the opposite corner of its rank, file and diagonal") {
  Sandbox game(8);
  game.place(0, 0, 0, make<Queen>(PieceColor::PIECEWHITE));
  auto board = game.tip(0);
  auto moves = movesAt(game, board, 0, 0);
  CHECK(contains(moves, board, 7, 0));
  CHECK(contains(moves, board, 0, 7));
  CHECK(contains(moves, board, 7, 7));
  CHECK(moves.size() == 21);
}

TEST_CASE("a white bishop moving diagonally across timelines targets a white-to-move board") {
  Sandbox game(4, 2);
  game.place(0, 1, 1, make<Bishop>(PieceColor::PIECEWHITE));
  auto from = game.tip(0);
  auto moves = movesAt(game, from, 1, 1);
  // (x, w) diagonal: one file right, one timeline up -> same half-turn on timeline 1.
  CHECK(contains(moves, game.tip(1), 2, 1));
  CHECK(contains(moves, game.tip(1), 1, 2));
  for (const auto& m : moves) {
    CHECK(m.board->halfTurnNumber() % 2 == 0);
  }
}

TEST_CASE("timeline invariants hold for every built-in game mode") {
  std::vector<std::shared_ptr<IGame>> games = {
    createGame<StandardGame>(),          createGame<CustomGameEmitBishop>(),
    createGame<CustomGameEmitKnight>(),  createGame<CustomGameEmitQueen>(),
    createGame<CustomGameEmitRook>(),    createGame<CustomGameKVB>(),
    createGame<MiscGameTimeLineInvasion>(), createGame<MiscGameTimeLineBattle>(),
    createGame<MiscGameTimeLineFragment>(),
  };
  for (const auto& game : games) {
    CAPTURE(game->dim());
    CHECK_FALSE(game->getMoveableBoards().empty());
    CHECK_FALSE(game->allPseudoLegalMoves().empty());
    int whiteKings = 0, blackKings = 0;
    for (const auto& timeLine : game->getTimeLines()) {
      for (const auto& board : timeLine->getBoards()) {
        CHECK(board->timeLineId() == timeLine->ID());
        for (int x = 0; x < board->dim(); ++x)
          for (int y = 0; y < board->dim(); ++y)
            if (auto p = board->getPiece({x, y})) {
              CHECK(p->getBoard() == board);
              CHECK(p->getPosition() == Position2D(x, y));
              if (p->name() == "king") (p->color() == PieceColor::PIECEWHITE ? whiteKings : blackKings) += 1;
            }
      }
    }
    CHECK(whiteKings >= 1);
    CHECK(blackKings >= 1);
  }
}

TEST_CASE("game-mode names are unique") {
  std::vector<std::string> names = {
    NameOfGame<StandardGame>::value,          NameOfGame<CustomGameEmitBishop>::value,
    NameOfGame<CustomGameEmitKnight>::value,  NameOfGame<CustomGameEmitQueen>::value,
    NameOfGame<CustomGameEmitRook>::value,    NameOfGame<CustomGameKVB>::value,
    NameOfGame<MiscGameTimeLineInvasion>::value, NameOfGame<MiscGameTimeLineBattle>::value,
    NameOfGame<MiscGameTimeLineFragment>::value,
  };
  std::sort(names.begin(), names.end());
  CHECK(std::adjacent_find(names.begin(), names.end()) == names.end());
}

// Still valid under the official present rule (present = earliest end-turn among ACTIVE timelines): timeline 0
// ends at h3 after the move, which is earlier than timeline 1's h5. Only the reasoning changed: the old engine took
// the min of the NEW boards, the official rule takes the min over all active timelines.
TEST_CASE("present does not skip a timeline after a move onto a board ahead in time") {
  // Timeline 0 is at h2 (the present), timeline 1 is already at h4.
  Sandbox game(5, {3, 5}, 2);
  game.place(0, 0, 0, make<Queen>(PieceColor::PIECEWHITE));
  auto from = game.boardAt(0, 2);
  auto to = game.boardAt(1, 4);
  auto moves = movesAt(game, from, 0, 0);
  REQUIRE(contains(moves, to, 0, 0));
  game.makeMove(Move{SelectedPosition(from, Position2D(0, 0)), SelectedPosition(to, Position2D(0, 0))});
  game.submitTurn();
  // Timeline 0 only reached h3, so the present must be h3, not h5.
  CHECK(game.presentHalfTurn() == 3);
}

namespace {

// Standard game after 1. e4 e5 followed by a knight time-jump, so a branch timeline exists.
std::unique_ptr<StandardGame> branchedGame() {
  auto game = std::make_unique<StandardGame>();
  auto b0 = game->getMoveableBoards()[0];
  game->makeMove({{b0, {4, 1}}, {b0, {4, 3}}});
  game->submitTurn();
  auto b1 = game->getMoveableBoards()[0];
  game->makeMove({{b1, {4, 6}}, {b1, {4, 4}}});
  game->submitTurn();
  auto b2 = game->getMoveableBoards()[0];
  game->makeMove({{b2, {1, 0}}, {game->getBoard(0, 0), {1, 2}}});
  return game;
}

} // namespace

TEST_CASE("clone copies the full state, including a pending turn") {
  auto game = branchedGame();
  auto copy = game->clone();
  CHECK(snapshot(*copy) == snapshot(*game));
  CHECK(copy->timeLineCount() == 2);
  CHECK(copy->undoable());
  CHECK(copy->bufferHalfTurn() == game->bufferHalfTurn());
  CHECK(copy->timeLine(1)->parentId() == 0);
  // TimeLine objects are deep-copied, Boards are shared.
  CHECK(copy->timeLine(1) != game->timeLine(1));
  CHECK(copy->getBoard(1, 1) == game->getBoard(1, 1));
}

TEST_CASE("moves on a clone do not affect the original, and vice versa") {
  auto game = branchedGame();
  auto copy = game->clone();
  const std::string start = snapshot(*game);

  // The clone undoes the branching move and finishes the turn differently; the original is untouched.
  copy->undo();
  CHECK(copy->timeLineCount() == 1);
  CHECK(snapshot(*game) == start);
  CHECK(game->timeLineCount() == 2);
  auto cb = copy->getMoveableBoards()[0];
  copy->makeMove({{cb, {0, 1}}, {cb, {0, 3}}});
  copy->submitTurn();
  CHECK(snapshot(*game) == start);
  CHECK(game->undoable());
  CHECK(game->getCurrentTurnColor() == PieceColor::PIECEWHITE);

  // Now mutate the original; the clone's snapshot must stay put.
  const std::string copySnapshot = snapshot(*copy);
  game->undo();
  auto gb = game->getMoveableBoards()[0];
  game->makeMove({{gb, {7, 1}}, {gb, {7, 3}}});
  game->submitTurn();
  CHECK(snapshot(*copy) == copySnapshot);
  CHECK(snapshot(*game) != copySnapshot);
}

TEST_CASE("timeline storage is keyed by ID and supports negative IDs") {
  struct NegativeSandbox : IGame {
    NegativeSandbox() : IGame(4) {
      for (int id : {-2, -1, 0, 1}) _addTimeLine(std::make_shared<TimeLine>(4, id))->pushBack(std::make_shared<Board>(4, id));
    }
    using IGame::timeLine;
  } game;
  CHECK(game.timeLineIds() == std::vector<int>{-2, -1, 0, 1});
  CHECK(game.minTimeLineId() == -2);
  CHECK(game.maxTimeLineId() == 1);
  CHECK(game.timeLineCount() == 4);
  CHECK(game.hasTimeLine(-1));
  CHECK_FALSE(game.hasTimeLine(2));
  CHECK_FALSE(game.boardExists(-3, 0));
  CHECK(game.boardExists(-2, 0));
  CHECK(game.getTimeLines().front()->ID() == -2);
  CHECK(game.getBoard(-1, 0)->timeLineId() == -1);

  // A rook on timeline 0 slides over timeline -1 and captures on -2 along the timeline axis.
  game.getBoard(0, 0)->placePiece({0, 0}, make<Rook>(PieceColor::PIECEWHITE));
  game.getBoard(-2, 0)->placePiece({0, 0}, make<Pawn>(PieceColor::PIECEBLACK));
  auto moves = movesAt(game, game.getBoard(0, 0), 0, 0);
  CHECK(contains(moves, game.getBoard(-1, 0), 0, 0));
  CHECK(contains(moves, game.getBoard(-2, 0), 0, 0));
  CHECK(contains(moves, game.getBoard(1, 0), 0, 0));
}
