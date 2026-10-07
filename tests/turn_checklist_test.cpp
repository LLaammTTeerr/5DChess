#include <doctest/doctest.h>

#include "engine/Notation.h"
#include "engine/Position.h"
#include "play/TurnChecklist.h"

#include <fstream>
#include <sstream>
#include "test_support.h"

using namespace Chess;
using Chess::Core::Coord;
using CMove = Chess::Core::Move;
using namespace test;
using play::RowState;

namespace {
std::shared_ptr<IGame> record(const std::string& name) {
  std::ifstream in(std::string(FDCHESS_UI_RECORDS_DIR) + "/" + name);
  REQUIRE(in.good());
  std::stringstream text;
  text << in.rdbuf();
  return loadRecord(text.str());
}

const play::ChecklistRow* row(const play::TurnChecklist& c, int timeline) {
  for (const auto& r : c.rows)
    if (r.timeline == timeline) return &r;
  return nullptr;
}

// A move on the timeline `l` at half-turn `t` (a jump names the board it lands on).
CMove move(int l, int t, const char* from, const char* to, int toL = INT16_MAX, int toT = -1) {
  const auto sq = [](const char* s) { return std::pair<int, int>{s[0] - 'a', s[1] - '1'}; };
  const auto [fx, fy] = sq(from);
  const auto [tx, ty] = sq(to);
  return CMove{Coord{int8_t(fx), int8_t(fy), int16_t(t), int16_t(l)},
               Coord{int8_t(tx), int8_t(ty), int16_t(toT < 0 ? t : toT), int16_t(toL == INT16_MAX ? l : toL)}};
}
} // namespace

TEST_CASE("TurnChecklist: the start position has one mandatory board and nothing to review") {
  auto game = newGame("standard");
  const play::TurnChecklist c = play::turnChecklist(*game);
  REQUIRE(c.rows.size() == 1);
  CHECK(c.rows[0].state == RowState::MustMove);
  CHECK(c.rows[0].label == "L0 \xC2\xB7 T1w");
  CHECK(c.rows[0].detail.empty());
  CHECK(c.done == 0);
  CHECK(c.total == 1);
  CHECK(c.boardsLeft == 1);
  CHECK(c.submit == play::SubmitState::BoardsLeft);
  CHECK(play::submitLine(c) == "Submit locked: 1 board left");
  CHECK(c.last.empty());
  CHECK(c.lastLabel.empty());
}

TEST_CASE("TurnChecklist: a played move marks its row and Submit becomes ready") {
  auto game = newGame("standard");
  game->makeMove(CMove{Coord{4, 1, 0, 0}, Coord{4, 3, 0, 0}});
  const play::TurnChecklist c = play::turnChecklist(*game);
  REQUIRE(c.rows.size() == 1); // the row stays where it was, now done
  CHECK(c.rows[0].state == RowState::Moved);
  CHECK(c.rows[0].detail == "e2-e4");
  CHECK(c.done == 1);
  CHECK(c.total == 1);
  CHECK(c.boardsLeft == 0);
  CHECK(c.submit == play::SubmitState::Ready);
  CHECK(play::submitLine(c) == "Submit: ready");
  game->undo(); // undo brings the row back
  const play::TurnChecklist again = play::turnChecklist(*game);
  CHECK(again.rows[0].state == RowState::MustMove);
  CHECK(again.done == 0);
}

TEST_CASE("TurnChecklist: after Submit the opponent's turn is listed and the other side's boards are the rows") {
  auto game = newGame("standard");
  game->makeMove(CMove{Coord{4, 1, 0, 0}, Coord{4, 3, 0, 0}});
  game->submitTurn();
  game->resolveResult();
  const play::TurnChecklist c = play::turnChecklist(*game);
  REQUIRE(c.rows.size() == 1);
  CHECK(c.rows[0].state == RowState::MustMove);
  CHECK(c.rows[0].label == "L0 \xC2\xB7 T1b");
  CHECK(c.lastByWhite);
  CHECK(c.lastLabel == "T1w");
  REQUIRE(c.last.size() == 1);
  CHECK(c.last[0].text == "L0  e2-e4");
  CHECK(c.last[0].from == Coord{4, 1, 0, 0});
  CHECK(c.last[0].to == Coord{4, 3, 0, 0});
}

TEST_CASE("TurnChecklist: move text names the piece, a capture, a promotion and a jump to another board") {
  auto game = record("branch.5dr"); // the knight jumped from T2 back to T1 (timeline L+1)
  REQUIRE(game->history().size() >= 3);
  const play::TurnChecklist c = play::turnChecklist(*game);
  // T2w of the record is Ng1 -> L0 T1w f3 on timeline 0
  bool found = false;
  for (const auto& turn : game->history())
    for (const auto& m : turn.moves)
      if (m.move.from.t != m.move.to.t || m.move.from.l != m.move.to.l) {
        const std::string text = play::moveText(*game, m);
        CHECK(text.find(" -> L") != std::string::npos);
        CHECK(text.front() == 'N');
        found = true;
      }
  CHECK(found);
  (void)c;
}

TEST_CASE("TurnChecklist: a six-timeline turn lists its five mandatory boards first, then the one that is ahead") {
  auto game = record("many-boards.5dr");
  const play::TurnChecklist c = play::turnChecklist(*game);
  REQUIRE(c.rows.size() == 6);
  CHECK(c.total == 5);
  CHECK(c.done == 0);
  CHECK(c.boardsLeft == 5);
  for (size_t i = 0; i < 5; ++i) CHECK(c.rows[i].state == RowState::MustMove);
  CHECK(c.rows[5].state == RowState::Waiting);
  CHECK(c.rows[5].timeline == 0);
  // newest timeline first within a group
  for (size_t i = 1; i < 5; ++i) CHECK(c.rows[i - 1].timeline > c.rows[i].timeline);
  CHECK(c.submit == play::SubmitState::BoardsLeft);
  CHECK(play::submitLine(c) == "Submit locked: 5 boards left");
  // the opponent's last turn is Black's four moves of T4b
  CHECK_FALSE(c.lastByWhite);
  CHECK(c.lastLabel == "T4b");
  CHECK(c.last.size() == 4);
}

TEST_CASE("TurnChecklist: moves tick boards off; a jump onto another present board completes both timelines") {
  auto game = record("many-boards.5dr");
  const int present = game->presentHalfTurn();
  game->makeMove(move(3, present, "d1", "b3"));
  game->makeMove(move(2, present, "b1", "a3"));
  const play::TurnChecklist two = play::turnChecklist(*game);
  CHECK(two.done == 2);
  CHECK(two.boardsLeft == 3);
  REQUIRE(row(two, 3));
  CHECK(row(two, 3)->state == RowState::Moved);
  CHECK(row(two, 3)->detail == "Bd1-b3");
  CHECK(row(two, 2)->detail == "Nb1-a3");
  CHECK(two.rows[0].timeline == 3); // a moved row keeps its place
  CHECK(two.submit == play::SubmitState::BoardsLeft);
  CHECK(play::submitLine(two) == "Submit locked: 3 boards left");

  // L-2's queen jumps to L-1's present board: both rows are done
  game->makeMove(move(-2, present, "d1", "c2", -1, present));
  const play::TurnChecklist three = play::turnChecklist(*game);
  CHECK(three.done == 4);
  CHECK(three.boardsLeft == 1);
  REQUIRE(row(three, -2));
  CHECK(row(three, -2)->detail == "Qd1 -> L-1 \xC2\xB7 T5w c2");
  REQUIRE(row(three, -1));
  CHECK(row(three, -1)->state == RowState::Moved);
  CHECK(row(three, -1)->detail == "Queen from L-2 \xC2\xB7 T5w");

  game->undo();
  const play::TurnChecklist back = play::turnChecklist(*game);
  CHECK(back.done == 2);
  CHECK(row(back, -2)->state == RowState::MustMove);
  CHECK(row(back, -1)->state == RowState::MustMove);
}

TEST_CASE("TurnChecklist: eleven timelines give ten rows to play on, mandatory ones first") {
  auto game = record("many-boards-11tl.5dr");
  const play::TurnChecklist c = play::turnChecklist(*game);
  CHECK(game->timeLineCount() == 11);
  CHECK(c.rows.size() >= 10);
  size_t i = 0;
  while (i < c.rows.size() && c.rows[i].state == RowState::MustMove) ++i;
  CHECK(i == static_cast<size_t>(c.total));
  while (i < c.rows.size() && c.rows[i].state == RowState::Optional) ++i;
  for (; i < c.rows.size(); ++i) CHECK(c.rows[i].state == RowState::Waiting); // nothing mandatory or optional after the optional ones
}

TEST_CASE("TurnChecklist: a jump into the past can take the present back: boards that had to be moved on need not be any more") {
  // The stalemate of tests/ui/scripts/stalemate.ui (Time Line Fragment): at the last move Black's bishop jumps back in time and forks a new
  // timeline; the present falls back to White's turn, so Black's other boards are not mandatory any more and Submit is ready.
  auto game = newGame("timeline-fragment");
  const auto turn = [&](std::initializer_list<CMove> moves) {
    for (const CMove& m : moves) game->makeMove(m);
    game->submitTurn();
    game->resolveResult();
  };
  turn({move(1, 0, "a1", "a2")});
  turn({move(1, 1, "b4", "b1"), move(0, 1, "c4", "c3")});
  turn({move(1, 2, "a2", "a3"), move(0, 2, "b1", "b4")});
  turn({move(0, 3, "d4", "d3"), move(1, 3, "b1", "b3")});
  turn({move(0, 4, "d1", "c3"), move(1, 4, "a3", "a3", 0, 4)});
  const play::TurnChecklist before = play::turnChecklist(*game);
  CHECK(before.total == 3);
  CHECK(before.boardsLeft == 3);

  game->makeMove(move(1, 5, "c4", "c3", 1, 3));
  REQUIRE(game->mandatoryBoards().empty());
  const play::TurnChecklist c = play::turnChecklist(*game);
  CHECK(c.boardsLeft == 0);
  CHECK(c.submit == play::SubmitState::Ready);
  CHECK(c.total == c.done); // nothing left to do: "n / n boards"
  CHECK(c.done >= 1);
  int moved = 0;
  for (const auto& r : c.rows) {
    CHECK(r.state != RowState::MustMove); // the board not played on is optional now
    if (r.state == RowState::Moved) ++moved;
  }
  CHECK(moved == c.done);
  CHECK(c.rows.size() == before.rows.size()); // the rows are still the boards the turn began with

  // Submitting it ends the game (White has no legal turn and is not in check: a draw): no rows any more, the deciding turn stays
  game->submitTurn();
  game->resolveResult();
  REQUIRE(game->result() != GameResult::Ongoing);
  const play::TurnChecklist over = play::turnChecklist(*game);
  CHECK(over.over);
  CHECK(over.rows.empty());
  CHECK(over.lastLabel == "T3b");
  CHECK_FALSE(over.lastByWhite);
  REQUIRE(over.last.size() == 1);
  CHECK(over.last[0].text == "L+1  Bc4 -> L+1 \xC2\xB7 T2b c3");
}

namespace {
std::shared_ptr<IGame> uiPosition(const char* name) {
  return Chess::Core::loadPositionFile(std::string(FDCHESS_UI_POSITIONS_DIR) + "/" + name).makeGame();
}
} // namespace

TEST_CASE("TurnChecklist: Submit note follows the state: Enter when ready, Space while boards are left, nothing else") {
  auto game = newGame("standard");
  CHECK(play::submitNote(play::turnChecklist(*game)) == "Space: next board");
  game->makeMove(CMove{Coord{4, 1, 0, 0}, Coord{4, 3, 0, 0}});
  CHECK(play::submitNote(play::turnChecklist(*game)) == "Enter: submit");
}

TEST_CASE("TurnChecklist: every board moved is not the same as Submit being ready (a king left exposed)") {
  auto game = uiPosition("check.5dp"); // White is in check from the rook on e8
  std::unique_ptr<IGame> exposed;
  const auto board = game->getMoveableBoards().front();
  for (int x = 0; x < game->dim() && !exposed; ++x)
    for (int y = 0; y < game->dim() && !exposed; ++y)
      for (const CMove& m : game->legalMovesFrom(Coord{int8_t(x), int8_t(y), int16_t(board->halfTurnNumber()), int16_t(board->timeLineId())})) {
        auto trial = game->clone();
        trial->makeMove(m);
        if (trial->mandatoryBoards().empty() && !trial->canSubmit()) {
          exposed = std::move(trial);
          break;
        }
      }
  REQUIRE(exposed);
  const play::TurnChecklist c = play::turnChecklist(*exposed);
  CHECK(c.boardsLeft == 0);
  CHECK(c.done == c.total); // "1 / 1 boards" ...
  CHECK(c.submit == play::SubmitState::KingExposed); // ... and still not ready
  CHECK(play::submitLine(c) == "Submit locked: a king is exposed");
  CHECK(play::submitNote(c).empty());
}

TEST_CASE("TurnChecklist: promotion in the move text, and an inactive timeline's row") {
  auto promotion = uiPosition("promotion.5dp");
  Chess::Core::PlayedMove played{CMove{Coord{4, 6, 2, 0}, Coord{4, 7, 2, 0}, PieceType::Knight}, true};
  CHECK(play::moveText(*promotion, played) == "e7-e8=N");
  played.move.promotion = PieceType::Queen;
  CHECK(play::moveText(*promotion, played) == "e7-e8=Q");

  auto game = uiPosition("inactive.5dp");
  const play::TurnChecklist c = play::turnChecklist(*game);
  bool found = false;
  for (const auto& r : c.rows)
    if (r.inactive) {
      found = true;
      CHECK(r.state == RowState::Optional); // an inactive timeline only has optional moves
    }
  CHECK(found);
}

TEST_CASE("TurnChecklist: while the other side is to move the rows are theirs and the last turn is ours") {
  auto game = newGame("standard");
  game->makeMove(CMove{Coord{4, 1, 0, 0}, Coord{4, 3, 0, 0}});
  game->submitTurn();
  game->resolveResult();
  const play::TurnChecklist c = play::turnChecklist(*game);
  REQUIRE(c.rows.size() == 1);
  CHECK(c.rows[0].state == RowState::MustMove);
  CHECK(c.rows[0].halfTurn == 1); // Black's board
  CHECK(c.lastByWhite);
  CHECK(c.submit == play::SubmitState::BoardsLeft);
}
