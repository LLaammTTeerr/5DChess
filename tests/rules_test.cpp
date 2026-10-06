// Directed tests for the official 5D Chess rules (see docs/RULES.md). Test names state the rule they cover.
#include <doctest/doctest.h>

#include "test_support.h"

using namespace Chess;
using namespace test;

namespace {

Move mv(const std::shared_ptr<Board>& fromBoard, int fx, int fy, const std::shared_ptr<Board>& toBoard, int tx, int ty) {
  return Move{SelectedPosition(fromBoard, Position2D(fx, fy)), SelectedPosition(toBoard, Position2D(tx, ty))};
}

Move mv(const std::shared_ptr<Board>& board, int fx, int fy, int tx, int ty) { return mv(board, fx, fy, board, tx, ty); }

// Plays and submits one move on the only moveable board.
void playOne(IGame& game, int fx, int fy, int tx, int ty) {
  auto boards = game.mandatoryBoards();
  REQUIRE(boards.size() == 1);
  game.makeMove(mv(boards[0], fx, fy, tx, ty));
  REQUIRE(game.canSubmit());
  game.submitTurn();
}

} // namespace

// ---------------------------------------------------------------------------------------------------------------
// 1. Timeline ownership
// ---------------------------------------------------------------------------------------------------------------

TEST_CASE("timeline ownership: a branch created by White gets max+1, one created by Black gets min-1") {
  auto gameHolder = newGame("standard");
  IGame& game = *gameHolder;
  playOne(game, 4, 1, 4, 3); // 1. e4
  playOne(game, 7, 6, 7, 5); // 1... h6
  playOne(game, 7, 1, 7, 2); // 2. h3
  // Black's knight jumps from the h3 board (end-turn 3) back to the board after 1. e4 (end-turn 1).
  auto tip = game.mandatoryBoards()[0];
  REQUIRE(tip->halfTurnNumber() == 3);
  auto past = game.getBoard(0, 1);
  REQUIRE(contains(movesAt(game, tip, 1, 7), past, 1, 5));
  game.makeMove(mv(tip, 1, 7, past, 1, 5));
  CHECK(game.timeLineIds() == std::vector<int>{-1, 0});
  CHECK(game.timeLine(-1)->parentId() == 0);
  CHECK(game.timeLine(-1)->forkAt() == 1);
  REQUIRE(game.canSubmit());
  game.submitTurn();

  // White: the knight on the (optional) tip of timeline 0 jumps back to the h2 board; White must also move on the
  // present board of timeline -1.
  CHECK(game.mandatoryBoards().size() == 1);
  CHECK(game.mandatoryBoards()[0]->timeLineId() == -1);
  auto tip0 = game.timeLine(0)->back();
  REQUIRE(tip0->halfTurnNumber() == 4);
  auto past0 = game.getBoard(0, 2);
  REQUIRE(contains(movesAt(game, tip0, 1, 0), past0, 1, 2));
  game.makeMove(mv(tip0, 1, 0, past0, 1, 2));
  CHECK(game.timeLineIds() == std::vector<int>{-1, 0, 1});
  game.makeMove(mv(game.mandatoryBoards()[0], 0, 1, 0, 2));
  CHECK(game.canSubmit());
}

// ---------------------------------------------------------------------------------------------------------------
// 2. Active timelines and the present
// ---------------------------------------------------------------------------------------------------------------

TEST_CASE("active timelines: the n-th timeline of a player is active iff the opponent created at least n-1") {
  Sandbox game(3, std::vector<int>{3}, 2);
  CHECK(game.isTimeLineActive(0));
  game.addCreatedTimeLine(1, 3);          // White's 1st: always active (opponent has created >= 0)
  CHECK(game.isTimeLineActive(1));
  game.addCreatedTimeLine(2, 3);          // White's 2nd: needs Black to have created 1
  CHECK_FALSE(game.isTimeLineActive(2));
  game.addCreatedTimeLine(-1, 3);         // Black's 1st: active; makes White's 2nd active
  CHECK(game.isTimeLineActive(-1));
  CHECK(game.isTimeLineActive(2));
  game.addCreatedTimeLine(3, 3);          // White's 3rd: needs Black to have created 2
  CHECK_FALSE(game.isTimeLineActive(3));
  game.addCreatedTimeLine(-2, 3);         // Black's 2nd: needs White to have created 1 (it has 3)
  CHECK(game.isTimeLineActive(-2));
  CHECK(game.isTimeLineActive(3));
  CHECK(game.activeTimeLineIds() == std::vector<int>{-2, -1, 0, 1, 2, 3});
}

TEST_CASE("present: earliest end-turn among ACTIVE timelines; inactive timelines are optional") {
  Sandbox game(4, std::vector<int>{5}, 4);      // timeline 0 ends at h4, the present
  game.addCreatedTimeLine(1, 5);                // White's 1st, active, ends at h4
  game.addCreatedTimeLine(2, 3);                // White's 2nd: inactive (Black created none), ends earlier at h2
  game.place(0, 0, 0, make(PieceType::Knight, PieceColor::PIECEWHITE));
  game.place(1, 0, 0, make(PieceType::Knight, PieceColor::PIECEWHITE));
  game.place(2, 0, 0, make(PieceType::Knight, PieceColor::PIECEWHITE));
  REQUIRE_FALSE(game.isTimeLineActive(2));
  CHECK(game.bufferHalfTurn() == 4);            // not 2: the inactive timeline does not drag the present back
  CHECK(game.mandatoryBoards().size() == 2);
  CHECK(game.getMoveableBoards().size() == 3);  // ... but it can still be played on

  game.makeMove(mv(game.tip(0), 0, 0, 1, 2));
  CHECK_FALSE(game.canSubmit());                // timeline 1 is still unmoved
  game.makeMove(mv(game.tip(1), 0, 0, 1, 2));
  CHECK(game.canSubmit());                      // the inactive timeline 2 may stay untouched
  game.submitTurn();
  CHECK(game.presentHalfTurn() == 5);
  CHECK(game.getCurrentTurnColor() == PieceColor::PIECEBLACK);
}

TEST_CASE("present: a Black timeline reactivates White's second timeline and moves the present back") {
  Sandbox game(4, std::vector<int>{5}, 4);
  game.addCreatedTimeLine(1, 5);
  game.addCreatedTimeLine(2, 3);
  CHECK(game.bufferHalfTurn() == 4);
  game.addCreatedTimeLine(-1, 5);               // Black has now created one timeline: timeline 2 becomes active
  CHECK(game.isTimeLineActive(2));
  CHECK(game.bufferHalfTurn() == 2);
  REQUIRE(game.mandatoryBoards().size() == 1);
  CHECK(game.mandatoryBoards()[0]->timeLineId() == 2);
}

// ---------------------------------------------------------------------------------------------------------------
// 3. Movement corrections
// ---------------------------------------------------------------------------------------------------------------

TEST_CASE("pawns: step and (unmoved) double step on the rank axis, capture diagonally only on the same board") {
  Sandbox game(5);
  game.place(0, 2, 1, make(PieceType::Pawn, PieceColor::PIECEWHITE));
  game.place(0, 1, 2, make(PieceType::Pawn, PieceColor::PIECEBLACK));
  game.place(0, 2, 2, make(PieceType::Rook, PieceColor::PIECEBLACK));
  auto board = game.tip(0);
  auto moves = movesAt(game, board, 2, 1);
  CHECK(moves.size() == 1);                     // blocked straight ahead; only the diagonal capture remains
  CHECK(contains(moves, board, 1, 2));

  Sandbox free(5);
  free.place(0, 2, 1, make(PieceType::Pawn, PieceColor::PIECEWHITE));
  auto fb = free.tip(0);
  auto m2 = movesAt(free, fb, 2, 1);
  CHECK(m2.size() == 2);
  CHECK(contains(m2, fb, 2, 2));
  CHECK(contains(m2, fb, 2, 3));
  markMoved(fb, 2, 1);
  CHECK(movesAt(free, fb, 2, 1).size() == 1);   // a pawn that has moved no longer double-steps
}

TEST_CASE("pawns: move one timeline forward on the same square, capture one timeline forward and a full turn back") {
  // Forward on the timeline axis is towards the opponent's timelines: White towards lower IDs (5d-chess-js
  // timelineMove(l, -forward)).
  Sandbox game(5, std::vector<int>{3, 3}, 2);
  game.place(1, 2, 1, make(PieceType::Pawn, PieceColor::PIECEWHITE));
  game.boardAt(0, 0)->place({2, 1}, make(PieceType::Rook, PieceColor::PIECEBLACK));
  auto moves = movesAt(game, game.tip(1), 2, 1);
  CHECK(contains(moves, game.tip(0), 2, 1));            // sideways on the timeline axis (to a lower ID)
  CHECK(contains(moves, game.boardAt(0, 0), 2, 1));     // capture: one timeline down and one full turn back
  CHECK_FALSE(contains(moves, game.boardAt(0, 1), 2, 1));
  // A black piece straight ahead on the timeline axis blocks the step and cannot be captured by it.
  game.place(0, 2, 1, make(PieceType::Rook, PieceColor::PIECEBLACK));
  CHECK_FALSE(contains(movesAt(game, game.tip(1), 2, 1), game.tip(0), 2, 1));
  // A white pawn never steps towards higher IDs.
  Sandbox up(5, std::vector<int>{3, 3}, 2);
  up.place(0, 2, 1, make(PieceType::Pawn, PieceColor::PIECEWHITE));
  CHECK_FALSE(contains(movesAt(up, up.tip(0), 2, 1), up.tip(1), 2, 1));
}

TEST_CASE("pawns: a black pawn steps towards higher timeline IDs") {
  Sandbox game(5, std::vector<int>{1, 1}, 0);
  game.place(1, 0, 0, make(PieceType::King, PieceColor::PIECEWHITE));
  game.place(0, 4, 0, make(PieceType::Knight, PieceColor::PIECEWHITE));
  game.place(0, 3, 3, make(PieceType::Pawn, PieceColor::PIECEBLACK));
  game.makeMove(mv(game.tip(1), 0, 0, 0, 1));
  game.makeMove(mv(game.tip(0), 4, 0, 3, 2));
  REQUIRE(game.canSubmit());
  game.submitTurn();
  auto moves = movesAt(game, game.tip(0), 3, 3);
  CHECK(contains(moves, game.tip(1), 3, 3));            // towards timeline 1, same square
  CHECK(contains(moves, game.tip(0), 3, 2) == false);   // blocked by the white knight straight ahead
}

TEST_CASE("pawns: an unmoved pawn may also make the double step along the timeline axis") {
  Sandbox game(4, std::vector<int>{1, 1, 1}, 0);
  game.place(2, 1, 1, make(PieceType::Pawn, PieceColor::PIECEWHITE));
  auto moves = movesAt(game, game.tip(2), 1, 1);
  CHECK(contains(moves, game.tip(1), 1, 1));
  CHECK(contains(moves, game.tip(0), 1, 1));
  game.place(1, 1, 1, make(PieceType::Rook, PieceColor::PIECEBLACK));  // a piece in between blocks both
  CHECK_FALSE(contains(movesAt(game, game.tip(2), 1, 1), game.tip(0), 1, 1));
}

TEST_CASE("en passant: capturing the pawn that just made the double step, on the same board") {
  Sandbox game(5);
  game.place(0, 0, 0, make(PieceType::King, PieceColor::PIECEWHITE));
  game.place(0, 1, 2, make(PieceType::Pawn, PieceColor::PIECEWHITE));
  game.place(0, 4, 4, make(PieceType::King, PieceColor::PIECEBLACK));
  game.place(0, 2, 4, make(PieceType::Pawn, PieceColor::PIECEBLACK));
  playOne(game, 0, 0, 0, 1);
  playOne(game, 2, 4, 2, 2);                   // double step next to the white pawn
  auto board = game.mandatoryBoards()[0];
  auto moves = movesAt(game, board, 1, 2);
  REQUIRE(contains(moves, board, 2, 3));       // en passant target
  game.makeMove(mv(board, 1, 2, 2, 3));
  auto after = game.getNewBoard();
  CHECK(after->at({2, 2}) == std::nullopt);   // the black pawn is gone
  REQUIRE(after->at({2, 3}) != std::nullopt);
  CHECK(after->at({2, 3})->type == PieceType::Pawn);
  CHECK(after->at({2, 3})->color == PieceColor::PIECEWHITE);
  CHECK(after->at({1, 2}) == std::nullopt);
}

TEST_CASE("en passant: not available after two single steps, nor one turn late") {
  {
    Sandbox game(5);
    game.place(0, 0, 0, make(PieceType::King, PieceColor::PIECEWHITE));
    game.place(0, 1, 2, make(PieceType::Pawn, PieceColor::PIECEWHITE));
    game.place(0, 4, 4, make(PieceType::King, PieceColor::PIECEBLACK));
    game.place(0, 2, 4, make(PieceType::Pawn, PieceColor::PIECEBLACK));
    playOne(game, 0, 0, 0, 1);
    playOne(game, 2, 4, 2, 3);
    playOne(game, 0, 1, 0, 0);
    playOne(game, 2, 3, 2, 2);                 // reached the same square in two single steps
    auto board = game.mandatoryBoards()[0];
    CHECK_FALSE(contains(movesAt(game, board, 1, 2), board, 2, 3));
  }
  {
    Sandbox game(5);
    game.place(0, 0, 0, make(PieceType::King, PieceColor::PIECEWHITE));
    game.place(0, 1, 2, make(PieceType::Pawn, PieceColor::PIECEWHITE));
    game.place(0, 4, 4, make(PieceType::King, PieceColor::PIECEBLACK));
    game.place(0, 2, 4, make(PieceType::Pawn, PieceColor::PIECEBLACK));
    playOne(game, 0, 0, 0, 1);
    playOne(game, 2, 4, 2, 2);
    playOne(game, 0, 1, 0, 0);                 // White declines
    playOne(game, 4, 4, 4, 3);
    auto board = game.mandatoryBoards()[0];
    CHECK_FALSE(contains(movesAt(game, board, 1, 2), board, 2, 3));
  }
}

TEST_CASE("castling: king moves two files towards an unmoved rook, the rook jumps over it (same board)") {
  Sandbox game(8);
  game.place(0, 4, 0, make(PieceType::King, PieceColor::PIECEWHITE));
  game.place(0, 0, 0, make(PieceType::Rook, PieceColor::PIECEWHITE));
  game.place(0, 7, 0, make(PieceType::Rook, PieceColor::PIECEWHITE));
  game.place(0, 0, 7, make(PieceType::King, PieceColor::PIECEBLACK));
  auto board = game.tip(0);
  auto moves = movesAt(game, board, 4, 0);
  CHECK(contains(moves, board, 6, 0));
  CHECK(contains(moves, board, 2, 0));

  game.makeMove(mv(board, 4, 0, 6, 0));
  auto after = game.getNewBoard();
  REQUIRE(after->at({6, 0}) != std::nullopt);
  CHECK(after->at({6, 0})->type == PieceType::King);
  REQUIRE(after->at({5, 0}) != std::nullopt);
  CHECK(after->at({5, 0})->type == PieceType::Rook);
  CHECK(after->at({7, 0}) == std::nullopt);
  CHECK(after->at({4, 0}) == std::nullopt);
  CHECK(after->at({0, 0}) != std::nullopt);   // the other rook is untouched
  CHECK_FALSE(after->at({6, 0})->unmoved);
  CHECK_FALSE(after->at({5, 0})->unmoved);
  CHECK(after->at({0, 0})->unmoved);
  game.undo();

  game.makeMove(mv(board, 4, 0, 2, 0));
  after = game.getNewBoard();
  CHECK(after->at({2, 0})->type == PieceType::King);
  CHECK(after->at({3, 0})->type == PieceType::Rook);
  CHECK(after->at({0, 0}) == std::nullopt);
}

TEST_CASE("castling: needs an unmoved king and an unmoved rook") {
  {
    Sandbox game(8);
    game.place(0, 4, 0, make(PieceType::King, PieceColor::PIECEWHITE));
    game.place(0, 7, 0, make(PieceType::Rook, PieceColor::PIECEWHITE));
    markMoved(game.tip(0), 4, 0);
    CHECK_FALSE(contains(movesAt(game, game.tip(0), 4, 0), game.tip(0), 6, 0));
  }
  {
    Sandbox game(8);
    game.place(0, 4, 0, make(PieceType::King, PieceColor::PIECEWHITE));
    game.place(0, 7, 0, make(PieceType::Rook, PieceColor::PIECEWHITE));
    markMoved(game.tip(0), 7, 0);
    CHECK_FALSE(contains(movesAt(game, game.tip(0), 4, 0), game.tip(0), 6, 0));
  }
  {
    Sandbox game(8);   // a piece between king and rook
    game.place(0, 4, 0, make(PieceType::King, PieceColor::PIECEWHITE));
    game.place(0, 7, 0, make(PieceType::Rook, PieceColor::PIECEWHITE));
    game.place(0, 6, 0, make(PieceType::Knight, PieceColor::PIECEWHITE));
    CHECK_FALSE(contains(movesAt(game, game.tip(0), 4, 0), game.tip(0), 6, 0));
  }
}

TEST_CASE("castling: not out of, through or into check on that board") {
  auto castlingMoves = [](int rookFile) {
    Sandbox game(8);
    game.place(0, 4, 0, make(PieceType::King, PieceColor::PIECEWHITE));
    game.place(0, 0, 0, make(PieceType::Rook, PieceColor::PIECEWHITE));
    game.place(0, 7, 0, make(PieceType::Rook, PieceColor::PIECEWHITE));
    if (rookFile >= 0) game.place(0, rookFile, 7, make(PieceType::Rook, PieceColor::PIECEBLACK));
    auto board = game.tip(0);
    auto moves = movesAt(game, board, 4, 0);
    return std::make_pair(contains(moves, board, 2, 0), contains(moves, board, 6, 0));
  };
  CHECK(castlingMoves(-1) == std::make_pair(true, true));
  CHECK(castlingMoves(4) == std::make_pair(false, false));  // out of check
  CHECK(castlingMoves(5) == std::make_pair(true, false));   // through check (kingside crossing square)
  CHECK(castlingMoves(6) == std::make_pair(true, false));   // into check
  CHECK(castlingMoves(3) == std::make_pair(false, true));   // through check (queenside crossing square)
  CHECK(castlingMoves(2) == std::make_pair(false, true));   // into check (queenside)
  CHECK(castlingMoves(1) == std::make_pair(true, true));    // only the rook's path is attacked: allowed
}

TEST_CASE("promotion: a pawn on the last rank becomes the chosen piece (default Queen)") {
  for (PieceType type : {PieceType::Queen, PieceType::Rook, PieceType::Bishop, PieceType::Knight}) {
    CAPTURE(pieceName(type));
    Sandbox game(4);
    game.place(0, 1, 2, make(PieceType::Pawn, PieceColor::PIECEWHITE));
    game.makeMove(mv(game.tip(0), 1, 2, 1, 3), type);
    auto piece = game.getNewBoard()->at({1, 3});
    REQUIRE(piece.has_value());
    CHECK(piece->type == type);
    CHECK(piece->color == PieceColor::PIECEWHITE);
    CHECK_FALSE(piece->unmoved);
  }
  Sandbox game(4);
  game.place(0, 1, 2, make(PieceType::Pawn, PieceColor::PIECEWHITE));
  game.makeMove(mv(game.tip(0), 1, 2, 1, 3));
  CHECK(game.getNewBoard()->at({1, 3})->type == PieceType::Queen);
}

TEST_CASE("promotion: capturing onto the last rank promotes, and Black promotes on rank 0") {
  Sandbox game(4);
  game.place(0, 1, 2, make(PieceType::Pawn, PieceColor::PIECEWHITE));
  game.place(0, 2, 3, make(PieceType::Rook, PieceColor::PIECEBLACK));
  game.makeMove(mv(game.tip(0), 1, 2, 2, 3), PieceType::Knight);
  CHECK(game.getNewBoard()->at({2, 3})->type == PieceType::Knight);

  Sandbox other(4);
  other.place(0, 0, 0, make(PieceType::King, PieceColor::PIECEWHITE));
  other.place(0, 2, 1, make(PieceType::Pawn, PieceColor::PIECEBLACK));
  playOne(other, 0, 0, 0, 1);
  auto board = other.mandatoryBoards()[0];
  other.makeMove(mv(board, 2, 1, 2, 0), PieceType::Rook);
  CHECK(other.getNewBoard()->at({2, 0})->type == PieceType::Rook);
  CHECK(other.getNewBoard()->at({2, 0})->color == PieceColor::PIECEBLACK);
}

// ---------------------------------------------------------------------------------------------------------------
// 4. Check and legality at turn level
// ---------------------------------------------------------------------------------------------------------------

TEST_CASE("check: checkingAttacks names the attacker and the king on real boards of the game") {
  Sandbox game(4);
  game.place(0, 0, 0, make(PieceType::Rook, PieceColor::PIECEBLACK));
  game.place(0, 3, 0, make(PieceType::King, PieceColor::PIECEWHITE));
  auto board = game.tip(0);
  auto threats = game.checkingAttacks();
  REQUIRE(threats.size() == 1);
  CHECK(threats[0].attacker.board == board);
  CHECK(threats[0].attacker.position == Position2D(0, 0));
  CHECK(threats[0].king.board == board);
  CHECK(threats[0].king.position == Position2D(3, 0));
  CHECK(game.inCheck());
  CHECK_FALSE(game.canSubmit());               // nothing moved yet
  game.makeMove(mv(board, 3, 0, 3, 1));
  CHECK(game.canSubmit());
  CHECK_FALSE(game.inCheck());
}

TEST_CASE("check: a king can be attacked from another timeline") {
  // White passes on both timelines; the black rook on timeline 1 then captures the king on timeline 0 sideways.
  Sandbox game(4, std::vector<int>{3, 3}, 2);
  game.place(0, 1, 1, make(PieceType::King, PieceColor::PIECEWHITE));
  game.place(1, 1, 1, make(PieceType::Rook, PieceColor::PIECEBLACK));
  game.place(1, 3, 3, make(PieceType::Knight, PieceColor::PIECEWHITE));
  auto threats = game.checkingAttacks();
  REQUIRE(threats.size() == 1);
  CHECK(threats[0].attacker.board == game.tip(1));
  CHECK(threats[0].king.board == game.tip(0));
  // Moving the king away on its own board resolves the check.
  game.makeMove(mv(game.tip(0), 1, 1, 2, 2));
  game.makeMove(mv(game.tip(1), 3, 3, 2, 1));
  CHECK(game.canSubmit());
}

TEST_CASE("check: a turn is submittable only if every mandatory board was moved") {
  Sandbox game(4, std::vector<int>{3, 3}, 2);
  game.place(0, 0, 0, make(PieceType::Knight, PieceColor::PIECEWHITE));
  game.place(1, 0, 0, make(PieceType::Knight, PieceColor::PIECEWHITE));
  CHECK_FALSE(game.canSubmit());
  game.makeMove(mv(game.tip(0), 0, 0, 1, 2));
  CHECK_FALSE(game.canSubmit());
  game.makeMove(mv(game.tip(1), 0, 0, 1, 2));
  CHECK(game.canSubmit());
}

// ---------------------------------------------------------------------------------------------------------------
// 5. Game end
// ---------------------------------------------------------------------------------------------------------------

TEST_CASE("checkmate: a back-rank mate ends the game with a win for the mating side") {
  Sandbox game(4);
  game.place(0, 0, 0, make(PieceType::King, PieceColor::PIECEWHITE));
  game.place(0, 3, 0, make(PieceType::Rook, PieceColor::PIECEWHITE));
  game.place(0, 0, 3, make(PieceType::King, PieceColor::PIECEBLACK));
  game.place(0, 0, 2, make(PieceType::Pawn, PieceColor::PIECEBLACK));
  game.place(0, 1, 2, make(PieceType::Pawn, PieceColor::PIECEBLACK));
  CHECK(game.result() == GameResult::Ongoing);
  game.makeMove(mv(game.tip(0), 3, 0, 3, 3));
  REQUIRE(game.canSubmit());
  game.submitTurn();
  CHECK(game.result() == GameResult::Ongoing);  // submitTurn() does not search: the result is armed, not decided
  CHECK(game.resultPending());
  CHECK(game.resolveResult());
  CHECK_FALSE(game.resultPending());
  CHECK(game.result() == GameResult::WhiteWins);
  CHECK(game.inCheck());                       // Black to move, in check
  CHECK(game.findLegalTurn() == TurnSearch::Status::None);
  CHECK_FALSE(game.canSubmit());
}

TEST_CASE("checkmate: the same position with an escape square is not mate") {
  Sandbox game(4);
  game.place(0, 0, 0, make(PieceType::King, PieceColor::PIECEWHITE));
  game.place(0, 3, 0, make(PieceType::Rook, PieceColor::PIECEWHITE));
  game.place(0, 0, 3, make(PieceType::King, PieceColor::PIECEBLACK));
  game.place(0, 1, 2, make(PieceType::Pawn, PieceColor::PIECEBLACK));   // (0,2) is free now: the king can step there
  game.makeMove(mv(game.tip(0), 3, 0, 3, 3));
  game.submitTurn();
  game.resolveResult();
  CHECK(game.result() == GameResult::Ongoing);
  CHECK(game.inCheck());
  CHECK(game.findLegalTurn() == TurnSearch::Status::Found);
}

TEST_CASE("stalemate: no legal turn and not in check is a draw") {
  Sandbox game(4);
  game.place(0, 3, 0, make(PieceType::King, PieceColor::PIECEWHITE));
  game.place(0, 3, 1, make(PieceType::Queen, PieceColor::PIECEWHITE));
  game.place(0, 0, 3, make(PieceType::King, PieceColor::PIECEBLACK));
  game.makeMove(mv(game.tip(0), 3, 1, 1, 1));
  REQUIRE(game.canSubmit());
  game.submitTurn();
  game.resolveResult();
  CHECK(game.result() == GameResult::Draw);
  CHECK_FALSE(game.inCheck());
  CHECK(game.findLegalTurn() == TurnSearch::Status::None);
}

TEST_CASE("game end: a search that has not finished leaves the game Ongoing and reports Running") {
  auto gameHolder = newGame("standard");
  IGame& game = *gameHolder;
  CHECK(game.findLegalTurn(0) == TurnSearch::Status::Running);
  CHECK(game.findLegalTurn() == TurnSearch::Status::Found);

  Sandbox mate(4);
  mate.place(0, 0, 0, make(PieceType::King, PieceColor::PIECEWHITE));
  mate.place(0, 3, 0, make(PieceType::Rook, PieceColor::PIECEWHITE));
  mate.place(0, 0, 3, make(PieceType::King, PieceColor::PIECEBLACK));
  mate.place(0, 0, 2, make(PieceType::Pawn, PieceColor::PIECEBLACK));
  mate.place(0, 1, 2, make(PieceType::Pawn, PieceColor::PIECEBLACK));
  mate.makeMove(mv(mate.tip(0), 3, 0, 3, 3));
  mate.submitTurn();
  CHECK(mate.stepResultSearch(0));             // no node budget: nothing proven yet
  CHECK(mate.result() == GameResult::Ongoing);
  CHECK(mate.resultPending());
  CHECK_FALSE(mate.stepResultSearch(1000000)); // done
  CHECK(mate.result() == GameResult::WhiteWins);
}

TEST_CASE("clone: carries unmoved flags and the result, but not a pending search") {
  Sandbox game(4);
  game.place(0, 0, 0, make(PieceType::King, PieceColor::PIECEWHITE));
  game.place(0, 3, 0, make(PieceType::Rook, PieceColor::PIECEWHITE));
  game.place(0, 0, 3, make(PieceType::King, PieceColor::PIECEBLACK));
  game.place(0, 0, 2, make(PieceType::Pawn, PieceColor::PIECEBLACK));
  game.place(0, 1, 2, make(PieceType::Pawn, PieceColor::PIECEBLACK));
  game.makeMove(mv(game.tip(0), 3, 0, 3, 3));
  game.submitTurn();
  auto pendingCopy = game.clone();
  CHECK_FALSE(pendingCopy->resultPending());
  game.resolveResult();
  auto copy = game.clone();
  CHECK(copy->result() == GameResult::WhiteWins);
  CHECK(snapshot(*copy) == snapshot(game));
  auto moved = copy->timeLine(0)->back()->at({3, 3});
  REQUIRE(moved.has_value());
  CHECK_FALSE(moved->unmoved);
  CHECK(copy->timeLine(0)->back()->at({0, 0})->unmoved);
  CHECK(copy->checkingAttacks().size() == game.checkingAttacks().size());
}

TEST_CASE("standard game: the first turn has 20 moves for White and for Black") {
  // Self-consistency numbers (as in 2D chess): 16 pawn moves + 4 knight moves. Not cross-checked against another engine.
  auto gameHolder = newGame("standard");
  IGame& game = *gameHolder;
  CHECK(game.allPseudoLegalMoves().size() == 20);
  playOne(game, 4, 1, 4, 3);
  CHECK(game.allPseudoLegalMoves().size() == 20);
  CHECK(game.result() == GameResult::Ongoing);
}

// ---------------------------------------------------------------------------------------------------------------
// Movement vectors (5d-chess-js piece.js movePos / moveVecs): every piece may also move FORWARD in time, onto a board
// of its own colour that lies in the future of the board it stands on (such boards exist on timelines that are ahead).
// ---------------------------------------------------------------------------------------------------------------

TEST_CASE("movement vectors: sizes match 5d-chess-js") {
  CHECK(pieceVectors(PieceType::Rook).size() == 8);
  CHECK(pieceVectors(PieceType::Bishop).size() == 24);
  CHECK(pieceVectors(PieceType::Queen).size() == 80);
  CHECK(pieceVectors(PieceType::King).size() == 80);
  CHECK(pieceVectors(PieceType::Knight).size() == 48);
  CHECK(pieceVectors(PieceType::Pawn).empty());
  // Every bishop vector has exactly two non-zero components, every rook vector exactly one.
  for (const auto& v : pieceVectors(PieceType::Bishop)) CHECK((v[0] != 0) + (v[1] != 0) + (v[2] != 0) + (v[3] != 0) == 2);
  for (const auto& v : pieceVectors(PieceType::Rook)) CHECK((v[0] != 0) + (v[1] != 0) + (v[2] != 0) + (v[3] != 0) == 1);
}

TEST_CASE("movement vectors: king, rook, bishop and queen move forward in time") {
  // Timeline 0 has boards h0..h2, timeline 1 has h0..h4: its h2 and h4 boards are in the future of h0 / h2 of timeline 0.
  Sandbox game(4, std::vector<int>{3, 5}, 2);
  game.boardAt(0, 0)->place({1, 1}, make(PieceType::King, PieceColor::PIECEWHITE));
  game.boardAt(0, 1)->place({1, 1}, make(PieceType::Pawn, PieceColor::PIECEBLACK)); // no effect on a white move (other half-turn)
  auto king = movesAt(game, game.boardAt(0, 0), 1, 1);
  CHECK(contains(king, game.boardAt(0, 2), 1, 1));     // dz = +1 on the same timeline
  CHECK(contains(king, game.boardAt(1, 2), 1, 1));     // dz = +1, dw = +1
  CHECK(contains(king, game.boardAt(1, 0), 1, 1));     // dw = +1
  CHECK(contains(king, game.boardAt(1, 2), 2, 2));     // dz = +1, dw = +1, dx = dy = +1
  CHECK_FALSE(contains(king, game.boardAt(1, 4), 1, 1)); // two full turns away: a king steps once

  Sandbox rook(4, std::vector<int>{3, 5}, 2);
  rook.boardAt(0, 0)->place({1, 1}, make(PieceType::Rook, PieceColor::PIECEWHITE));
  auto rm = movesAt(rook, rook.boardAt(0, 0), 1, 1);
  CHECK(contains(rm, rook.boardAt(0, 2), 1, 1));
  CHECK_FALSE(contains(rm, rook.boardAt(1, 2), 1, 1));  // a rook moves along one axis only
  rook.boardAt(0, 2)->place({1, 1}, make(PieceType::Pawn, PieceColor::PIECEBLACK));
  CHECK(contains(movesAt(rook, rook.boardAt(0, 0), 1, 1), rook.boardAt(0, 2), 1, 1)); // capture ends the slide

  Sandbox bishop(4, std::vector<int>{3, 5}, 2);
  bishop.boardAt(0, 0)->place({1, 1}, make(PieceType::Bishop, PieceColor::PIECEWHITE));
  auto bm = movesAt(bishop, bishop.boardAt(0, 0), 1, 1);
  CHECK(contains(bm, bishop.boardAt(0, 2), 2, 1));      // file + time
  CHECK(contains(bm, bishop.boardAt(0, 2), 1, 2));      // rank + time
  CHECK(contains(bm, bishop.boardAt(1, 2), 1, 1));      // timeline + time
  CHECK_FALSE(contains(bm, bishop.boardAt(0, 2), 1, 1)); // a single axis is a rook move

  Sandbox queen(4, std::vector<int>{3, 3, 5}, 2);
  queen.boardAt(0, 0)->place({0, 0}, make(PieceType::Queen, PieceColor::PIECEWHITE));
  auto qm = movesAt(queen, queen.boardAt(0, 0), 0, 0);
  CHECK(contains(qm, queen.boardAt(1, 2), 1, 1));       // (dx, dy, dz, dw) = (1, 1, 1, 1)
  CHECK(contains(qm, queen.boardAt(2, 4), 2, 2));       // twice that: not capped by the board's own turn number
}

TEST_CASE("the UI-facing move list never offers a king capture; the state version changes with every move") {
  Sandbox game(4, 1);
  game.place(0, 0, 0, make(PieceType::Rook, PieceColor::PIECEWHITE));
  game.place(0, 0, 3, make(PieceType::King, PieceColor::PIECEBLACK));
  game.place(0, 3, 0, make(PieceType::Pawn, PieceColor::PIECEBLACK));
  CHECK_FALSE(contains(movesAt(game, game.tip(0), 0, 0), game.tip(0), 0, 3));
  CHECK(contains(movesAt(game, game.tip(0), 0, 0), game.tip(0), 3, 0)); // other captures stay
  const auto v0 = game.stateVersion();
  game.makeMove(mv(game.tip(0), 0, 0, 1, 0));
  const auto v1 = game.stateVersion();
  CHECK(v1 != v0);
  game.undo();
  CHECK(game.stateVersion() != v1);
}
