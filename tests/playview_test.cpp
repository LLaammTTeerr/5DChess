#include <doctest/doctest.h>

#include <algorithm>
#include <fstream>
#include <set>
#include <sstream>

#include "engine/Notation.h"
#include "play/PlayViewLayout.h"
#include "test_support.h"

using namespace Chess;
using namespace Chess::Core;
using namespace test;
using play::BoardRole;
using play::Rect;
using play::PlayViewLayout;
namespace pv = play::pv;

namespace {
// The free rectangle PlayScreen hands the layout in a 1400x800 window with no side panel: from the HUD zone's bottom + 8 to the controls bar.
const Rect kFree{12.0f, 128.0f, 1376.0f, 616.0f};

bool inside(const Rect& outer, const Rect& r) {
  return r.x >= outer.x - 0.01f && r.y >= outer.y - 0.01f && r.x + r.w <= outer.x + outer.w + 0.01f && r.y + r.h <= outer.y + outer.h + 0.01f;
}
bool overlap(const Rect& a, const Rect& b) { return a.x < b.x + b.w && b.x < a.x + a.w && a.y < b.y + b.h && b.y < a.y + a.h; }

// `n` timelines (ids 0, then 1, -1, 2, -2 ...) whose latest board is at half-turn `last`
std::unique_ptr<Sandbox> timelines(int n, int last = 8) {
  std::vector<int> counts(1, last + 1);
  auto game = std::make_unique<Sandbox>(8, counts, last);
  for (int i = 1; i < n; ++i) game->addCreatedTimeLine(i % 2 ? (i + 1) / 2 : -(i / 2), last + 1);
  return game;
}
} // namespace

TEST_CASE("PlayView grid: the square size at the shipped free rectangle meets the targets") {
  // `cards + 1` cells: the inspector is the last cell
  const pv::Grid six = pv::gridFor({12, 156, 1376, 588}, 6 + 1, 8);
  CHECK(six.square >= 28.0f);
  CHECK_FALSE(six.scrolls);
  const pv::Grid eleven = pv::gridFor({12, 156, 1376, 588}, 11 + 1, 8);
  CHECK(eleven.square >= 20.0f);
  CHECK_FALSE(eleven.scrolls);
  CHECK(six.square > eleven.square);
  MESSAGE("6 timelines: " << six.cols << "x" << six.rows << " at " << six.square << " px; 11 timelines: " << eleven.cols << "x" << eleven.rows
                          << " at " << eleven.square << " px");
}

TEST_CASE("PlayView grid: cells lie inside the area, never overlap, and the squares are whole pixels") {
  const Rect area{12, 156, 1376, 588};
  for (int count = 1; count <= 40; ++count) {
    CAPTURE(count);
    const pv::Grid g = pv::gridFor(area, count, 8);
    REQUIRE(static_cast<int>(g.cells.size()) == count);
    CHECK(g.square == std::floor(g.square));
    CHECK(g.square >= pv::kMinSquare);
    CHECK(g.square <= pv::kMaxSquare);
    for (size_t i = 0; i < g.cells.size(); ++i) {
      CHECK(g.cells[i].x >= area.x);
      CHECK(g.cells[i].x + g.cells[i].w <= area.x + area.w + 1.0f);
      if (!g.scrolls) CHECK(inside({area.x, area.y, area.w, area.h + 1.0f}, g.cells[i]));
      for (size_t j = i + 1; j < g.cells.size(); ++j) CHECK_FALSE(overlap(g.cells[i], g.cells[j]));
    }
  }
}

TEST_CASE("PlayView grid: the square never grows when a cell is added, and large counts scroll at the minimum square") {
  float previous = 1e9f;
  for (int count = 1; count <= 30; ++count) {
    const pv::Grid g = pv::gridFor({12, 156, 1376, 588}, count, 8);
    CHECK(g.square <= previous);
    previous = g.square;
  }
  const pv::Grid many = pv::gridFor({12, 156, 1376, 588}, 60, 8);
  CHECK(many.scrolls);
  CHECK(many.square == pv::kMinSquare);
  CHECK(many.contentHeight > 588.0f);
}

TEST_CASE("PlayView grid: boards of other sizes (5x5) get squares at least as large in the same area") {
  const pv::Grid eight = pv::gridFor({12, 156, 1376, 588}, 7, 8);
  const pv::Grid five = pv::gridFor({12, 156, 1376, 588}, 7, 5);
  CHECK(five.square >= eight.square);
}

TEST_CASE("PlayView grid: a narrow free rectangle (a side panel) lowers the square, it does not break the layout") {
  const pv::Grid narrow = pv::gridFor({12, 156, 1050, 588}, 11 + 1, 8);
  CHECK(narrow.square >= pv::kMinSquare);
  CHECK(narrow.cols * narrow.rows >= 12);
  for (const Rect& c : narrow.cells) CHECK(c.x + c.w <= 12 + 1050 + 1.0f);
}

TEST_CASE("PlayView order: must-move boards first, then optional, then waiting; White's timelines first inside a group") {
  using pv::Group;
  const auto ids = pv::orderCards({{0, Group::Waiting}, {1, Group::MustMove}, {-1, Group::Optional}, {2, Group::MustMove}, {-2, Group::MustMove}, {3, Group::Waiting}});
  CHECK(ids == std::vector<int>{2, 1, -2, -1, 3, 0});
}

TEST_CASE("PlayView order: the same keys always give the same order") {
  using pv::Group;
  std::vector<pv::OrderKey> keys{{0, Group::Waiting}, {1, Group::Optional}, {-1, Group::Optional}, {2, Group::MustMove}};
  const auto a = pv::orderCards(keys);
  std::reverse(keys.begin(), keys.end());
  CHECK(a == pv::orderCards(keys));
}

TEST_CASE("PlayViewLayout: one card per active timeline showing its latest board, frozen order within a turn") {
  auto game = timelines(4, 6);
  auto view = play::MultiverseView::build(*game);
  PlayViewLayout layout;
  layout.sync(*game, view);
  REQUIRE(layout.cards().size() == 4);
  std::vector<int> first;
  for (const auto& c : layout.cards()) first.push_back(c.timeline);
  for (const auto& c : layout.cards()) CHECK(c.halfTurn == 6);

  // a timeline appears mid-turn: appended, the others stay where they are
  game->addCreatedTimeLine(-2, 7);
  view = play::MultiverseView::build(*game);
  layout.sync(*game, view);
  REQUIRE(layout.cards().size() == 5);
  for (size_t i = 0; i < first.size(); ++i) CHECK(layout.cards()[i].timeline == first[i]);
  CHECK(layout.cards().back().timeline == -2);
  CHECK(layout.cards().back().chip == play::Chip::Moved); // not there when the turn started
  CHECK(layout.cards().back().created);
}

TEST_CASE("PlayViewLayout: place() puts every card inside the area, clear of the others, and the inspector last") {
  auto game = timelines(6, 8);
  const auto view = play::MultiverseView::build(*game);
  PlayViewLayout layout;
  layout.sync(*game, view);
  layout.place(kFree, 8);
  REQUIRE(layout.cards().size() == 6);
  CHECK(layout.grid().square >= 28.0f);
  std::vector<Rect> rects;
  for (const auto& c : layout.cards()) {
    CHECK(inside(kFree, c.cell));
    CHECK(inside(c.cell, c.card));
    CHECK(inside(c.cell, c.histRect));
    CHECK(inside(c.card, c.board));
    CHECK_FALSE(overlap(c.card, c.histRect));
    for (const Rect& r : rects) CHECK_FALSE(overlap(r, c.cell));
    rects.push_back(c.cell);
  }
  CHECK(inside(kFree, layout.inspectorCell()));
  for (const Rect& r : rects) CHECK_FALSE(overlap(r, layout.inspectorCell()));
}

TEST_CASE("PlayViewLayout: 11 timelines keep at least 20 px squares") {
  auto game = timelines(11, 8);
  const auto view = play::MultiverseView::build(*game);
  PlayViewLayout layout;
  layout.sync(*game, view);
  layout.place(kFree, 8);
  CHECK(layout.cards().size() + layout.inactive().size() == 11);
  CHECK(layout.grid().square >= 20.0f);
  MESSAGE("11 timelines: " << layout.cards().size() << " cards, " << layout.inactive().size() << " inactive, " << layout.grid().cols << "x"
                           << layout.grid().rows << " at " << layout.grid().square << " px");
}

TEST_CASE("PlayViewLayout: hit() maps a point to the square, the card chrome or nothing") {
  auto game = timelines(3, 4);
  const auto view = play::MultiverseView::build(*game);
  PlayViewLayout layout;
  layout.sync(*game, view);
  layout.place(kFree, 8);
  for (const auto& c : layout.cards()) {
    for (int x = 0; x < 8; ++x)
      for (int y = 0; y < 8; ++y) {
        const Rect sq = play::BoardLayout::squareRect(c.board, 8, x, y);
        const auto h = layout.hit(sq.centerX(), sq.centerY());
        REQUIRE(h.kind == PlayViewLayout::Hit::Kind::Square);
        CHECK(h.square == Coord{int8_t(x), int8_t(y), int16_t(c.halfTurn), int16_t(c.timeline)});
      }
    // the footer strip is chrome
    const auto chrome = layout.hit(c.card.x + c.card.w / 2, c.card.y + c.card.h - 3.0f);
    CHECK(chrome.kind == PlayViewLayout::Hit::Kind::Chrome);
    CHECK(chrome.key == std::make_pair(c.timeline, c.halfTurn));
    CHECK(layout.squareRect({int8_t(0), int8_t(0), int16_t(c.halfTurn), int16_t(c.timeline)}).has_value());
  }
  CHECK(layout.hit(0.0f, 0.0f).kind == PlayViewLayout::Hit::Kind::None);
  CHECK(layout.hit(5000.0f, 5000.0f).kind == PlayViewLayout::Hit::Kind::None);
}

TEST_CASE("PlayViewLayout: boards a lifted piece can reach that are not cards go to the inspector, nearest timeline first") {
  auto game = timelines(3, 6);
  const auto view = play::MultiverseView::build(*game);
  PlayViewLayout layout;
  layout.sync(*game, view);
  layout.place(kFree, 8);
  const int top = layout.cards().front().timeline;
  const Coord from{int8_t(1), int8_t(1), int16_t(6), int16_t(top)};
  std::vector<Coord> targets{
      {int8_t(2), int8_t(2), int16_t(6), int16_t(top)},   // its own board: a card
      {int8_t(2), int8_t(2), int16_t(3), int16_t(top)},   // a past board of its own timeline
      {int8_t(3), int8_t(3), int16_t(3), int16_t(top)},   // the same board again
      {int8_t(2), int8_t(4), int16_t(2), int16_t(0)},     // a past board of another timeline
  };
  layout.setSelection(from, targets);
  layout.place(kFree, 8);
  CHECK(layout.mode() == PlayViewLayout::Mode::Targets);
  REQUIRE(layout.tabs().size() == 2);
  CHECK(layout.tabs()[0].key == std::make_pair(top, 3)); // same timeline first
  CHECK(layout.tabs()[0].targets == 2);
  CHECK(layout.inspectorKey() == std::make_optional(std::make_pair(top, 3)));
  REQUIRE(layout.squareRect(targets[1]).has_value()); // the docked board takes clicks
  const Rect sq = *layout.squareRect(targets[1]);
  const auto h = layout.hit(sq.centerX(), sq.centerY());
  REQUIRE(h.kind == PlayViewLayout::Hit::Kind::Square);
  CHECK(h.square == targets[1]);

  layout.pickTarget({0, 2});
  layout.place(kFree, 8);
  CHECK(layout.inspectorKey() == std::make_optional(std::make_pair(0, 2)));

  layout.setSelection(std::nullopt, {});
  layout.place(kFree, 8);
  CHECK(layout.mode() == PlayViewLayout::Mode::Empty);
}

TEST_CASE("PlayViewLayout: only targets on cards means no inspector content") {
  auto game = timelines(3, 6);
  const auto view = play::MultiverseView::build(*game);
  PlayViewLayout layout;
  layout.sync(*game, view);
  layout.place(kFree, 8);
  const int a = layout.cards()[0].timeline, b = layout.cards()[1].timeline;
  layout.setSelection(Coord{int8_t(1), int8_t(1), int16_t(6), int16_t(a)}, {{int8_t(1), int8_t(2), int16_t(6), int16_t(b)}});
  layout.place(kFree, 8);
  CHECK(layout.mode() == PlayViewLayout::Mode::Empty);
}

TEST_CASE("PlayViewLayout: the history of a timeline is browsed one board at a time in the inspector") {
  auto game = timelines(2, 6);
  const auto view = play::MultiverseView::build(*game);
  PlayViewLayout layout;
  layout.sync(*game, view);
  layout.place(kFree, 8);
  const int tl = layout.cards()[0].timeline;
  layout.browse(tl);
  layout.place(kFree, 8);
  CHECK(layout.mode() == PlayViewLayout::Mode::History);
  CHECK(layout.inspectorKey() == std::make_optional(std::make_pair(tl, 5))); // the board before the card's
  CHECK(layout.tabs().size() >= 1);
  layout.stepBrowse(-1);
  layout.place(kFree, 8);
  CHECK(layout.inspectorKey() == std::make_optional(std::make_pair(tl, 4)));
  layout.stepBrowse(+1);
  layout.stepBrowse(+1); // the newest history board is the last one: it stops there
  layout.place(kFree, 8);
  CHECK(layout.inspectorKey() == std::make_optional(std::make_pair(tl, 5)));
  layout.closeBrowse();
  layout.place(kFree, 8);
  CHECK(layout.mode() == PlayViewLayout::Mode::Empty);
}

TEST_CASE("PlayViewLayout: neighbour() walks the grid by row and column") {
  auto game = timelines(6, 6);
  const auto view = play::MultiverseView::build(*game);
  PlayViewLayout layout;
  layout.sync(*game, view);
  layout.place(kFree, 8);
  using Dir = play::BoardLayout::Dir;
  const int cols = layout.grid().cols;
  REQUIRE(cols >= 2);
  const int first = layout.cards()[0].timeline;
  CHECK_FALSE(layout.neighbour(first, Dir::Left).has_value());
  CHECK_FALSE(layout.neighbour(first, Dir::Up).has_value());
  CHECK(layout.neighbour(first, Dir::Right) == std::make_optional(layout.cards()[1].timeline));
  if (layout.grid().rows > 1) CHECK(layout.neighbour(first, Dir::Down) == std::make_optional(layout.cards()[static_cast<size_t>(cols)].timeline));
}

TEST_CASE("PlayViewLayout: scrolling is clamped and only when the grid overflows") {
  auto few = timelines(3, 6);
  PlayViewLayout layout;
  layout.sync(*few, play::MultiverseView::build(*few));
  layout.place(kFree, 8);
  layout.scrollBy(100.0f);
  CHECK(layout.scroll() == 0.0f);

  auto lots = timelines(40, 6);
  PlayViewLayout big;
  big.sync(*lots, play::MultiverseView::build(*lots));
  big.place(kFree, 8);
  REQUIRE(big.scrolls());
  big.scrollBy(-50.0f);
  CHECK(big.scroll() == 0.0f);
  big.scrollBy(1e6f);
  big.place(kFree, 8);
  CHECK(big.scroll() > 0.0f);
  CHECK(big.scroll() <= big.grid().contentHeight);
  const float bottom = big.inspectorCell().y + big.inspectorCell().h;
  CHECK(bottom <= big.gridArea().y + big.gridArea().h + 1.0f); // the last cell is reachable
}

// ---- the real positions of the UI scripts (tests/ui/records) ----

namespace {
std::shared_ptr<IGame> loadUiRecord(const std::string& name) {
  newGame("standard"); // sets the catalog directory the record's mode is read from
  std::ifstream file(std::string(FDCHESS_UI_RECORDS_DIR) + "/" + name);
  std::stringstream text;
  text << file.rdbuf();
  auto game = loadRecord(text.str());
  if (!game) throw std::runtime_error("cannot load record " + name);
  return game;
}
} // namespace

TEST_CASE("PlayViewLayout: the 6-timeline record: five must-move boards first (White's timelines first), the ahead board last") {
  const auto game = loadUiRecord("play-view-6tl.5dr");
  const auto view = play::MultiverseView::build(*game);
  PlayViewLayout layout;
  layout.sync(*game, view);
  std::vector<int> ids;
  for (const auto& c : layout.cards()) ids.push_back(c.timeline);
  CHECK(ids == std::vector<int>{3, 2, 1, -1, -2, 0});
  for (size_t i = 0; i < 5; ++i) CHECK(layout.cards()[i].chip == play::Chip::MustMove);
  CHECK(layout.cards()[5].chip == play::Chip::Waiting); // L0 is ahead: T5b
  CHECK(layout.cards()[5].halfTurn == 9);
  const auto n = layout.counts();
  CHECK(n.must == 5);
  CHECK(n.waiting == 1);
  layout.place(kFree, 8);
  CHECK(layout.grid().square >= 28.0f);
}

TEST_CASE("PlayViewLayout: moves in the turn mark cards \"moved\" and never reorder them; a new turn orders afresh; undo restores") {
  const auto game = loadUiRecord("play-view-6tl.5dr");
  PlayViewLayout layout;
  layout.sync(*game, play::MultiverseView::build(*game));
  const auto order = [&] {
    std::vector<int> ids;
    for (const auto& c : layout.cards()) ids.push_back(c.timeline);
    return ids;
  };
  const std::vector<int> start = order();
  const auto move = [&](const char* text) {
    game->makeMove(parseMove(text, PieceColor::PIECEWHITE));
    layout.sync(*game, play::MultiverseView::build(*game));
  };
  move("(L3T5)d1>(L3T5)b3");
  CHECK(order() == start);
  CHECK(layout.card(3)->chip == play::Chip::Moved);
  CHECK(layout.card(3)->halfTurn == 9); // the card shows the board the move made
  CHECK(layout.card(2)->chip == play::Chip::MustMove);
  move("(L-2T5)d1>(L-1T5)c2"); // a move between two cards: both boards are new
  CHECK(order() == start);
  CHECK(layout.card(-2)->chip == play::Chip::Moved);
  CHECK(layout.card(-1)->chip == play::Chip::Moved);
  CHECK(layout.counts().moved == 3);

  game->undo();
  layout.sync(*game, play::MultiverseView::build(*game));
  CHECK(order() == start);
  CHECK(layout.card(-2)->chip == play::Chip::MustMove);
  CHECK(layout.card(3)->chip == play::Chip::Moved);

  move("(L2T5)b1>(L2T5)a3");
  move("(L-2T5)d1>(L-1T5)c2");
  move("(L1T5)d2>(L1T5)d4");
  REQUIRE(game->canSubmit());
  game->submitTurn();
  layout.sync(*game, play::MultiverseView::build(*game));
  // Black's turn: every timeline's board is now Black's to move, L0 included: a fresh order, nothing "moved"
  CHECK(layout.counts().moved == 0);
  CHECK(layout.counts().must == 6);
  CHECK(order() == std::vector<int>{3, 2, 1, 0, -1, -2});
}

TEST_CASE("PlayViewLayout: the 11-timeline record: ten cards, the inactive timeline in the row, 20 px squares") {
  const auto game = loadUiRecord("play-view-11tl.5dr");
  PlayViewLayout layout;
  layout.sync(*game, play::MultiverseView::build(*game));
  CHECK(layout.cards().size() == 10);
  REQUIRE(layout.inactive().size() == 1);
  CHECK(layout.inactive()[0].timeline == -6);
  layout.place(kFree, 8);
  CHECK(layout.grid().square >= 20.0f);
  CHECK(layout.inactiveRow().h > 0.0f);
  CHECK(layout.inactive()[0].rect.w > 0.0f);
  const auto h = layout.hit(layout.inactive()[0].rect.centerX(), layout.inactive()[0].rect.centerY());
  CHECK(h.kind == PlayViewLayout::Hit::Kind::Inactive);
  CHECK(h.timeline == -6);
  layout.browseBoard(-6, layout.inactive()[0].halfTurn);
  layout.place(kFree, 8);
  CHECK(layout.mode() == PlayViewLayout::Mode::History);
  CHECK(layout.inspectorKey() == std::make_optional(std::make_pair(-6, layout.inactive()[0].halfTurn)));
}
