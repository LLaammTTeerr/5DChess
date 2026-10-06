#include <doctest/doctest.h>

#include "test_support.h"

#include "engine/GameCatalog.h"
#include "engine/Position.h"

#include <set>
#include <unordered_set>

using namespace Chess;
using namespace Chess::Core;
using namespace test;
using VMove = Chess::Core::Move; // Chess::Move is the older pointer-based move

namespace {

struct CatalogDir {
  CatalogDir() { GameCatalog::setDirectory(FDCHESS_POSITIONS_DIR); }
};
const CatalogDir catalogDir;

// The old hand-written subclass that each catalog mode must reproduce, by id.
std::shared_ptr<IGame> oracle(const std::string& id) {
  if (id == "standard") return createGame<StandardGame>();
  if (id == "omit-bishop") return createGame<CustomGameEmitBishop>();
  if (id == "omit-knight") return createGame<CustomGameEmitKnight>();
  if (id == "omit-queen") return createGame<CustomGameEmitQueen>();
  if (id == "omit-rook") return createGame<CustomGameEmitRook>();
  if (id == "knight-vs-bishop") return createGame<CustomGameKVB>();
  if (id == "timeline-invasion") return createGame<MiscGameTimeLineInvasion>();
  if (id == "timeline-battle") return createGame<MiscGameTimeLineBattle>();
  if (id == "timeline-fragment") return createGame<MiscGameTimeLineFragment>();
  return nullptr;
}

std::string oracleTitle(const std::string& id) {
  if (id == "standard") return NameOfGame<StandardGame>::value;
  if (id == "omit-knight") return NameOfGame<CustomGameEmitKnight>::value;
  if (id == "omit-queen") return NameOfGame<CustomGameEmitQueen>::value;
  if (id == "omit-rook") return NameOfGame<CustomGameEmitRook>::value;
  if (id == "knight-vs-bishop") return NameOfGame<CustomGameKVB>::value;
  if (id == "timeline-invasion") return NameOfGame<MiscGameTimeLineInvasion>::value;
  if (id == "timeline-battle") return NameOfGame<MiscGameTimeLineBattle>::value;
  if (id == "timeline-fragment") return NameOfGame<MiscGameTimeLineFragment>::value;
  return "";
}

void checkSameGame(const IGame& a, const IGame& b) {
  CHECK(snapshot(a) == snapshot(b));
  CHECK(a.dim() == b.dim());
  CHECK(a.rule().castling == b.rule().castling);
  CHECK(a.rule().pawnCanMakeTwoMoveOnFirstTurn == b.rule().pawnCanMakeTwoMoveOnFirstTurn);
  CHECK(a.timeLineIds() == b.timeLineIds());
  for (int id : a.timeLineIds()) CHECK(a.isTimeLineActive(id) == b.isTimeLineActive(id));
  CHECK(Position::fromGame(a) == Position::fromGame(b));
}

} // namespace

TEST_CASE("Coord and Move are values: ordering, equality, hashing") {
  const Coord a{1, 2, 3, -1}, b{1, 2, 3, 0};
  CHECK(a == a);
  CHECK(a != b);
  CHECK(a < b);
  CHECK(std::hash<Coord>()(a) != std::hash<Coord>()(b));
  std::unordered_set<Coord> set{a, b, a};
  CHECK(set.size() == 2);
  std::unordered_set<VMove> moves{VMove{a, b}, VMove{a, b, PieceType::Rook}, VMove{a, b}};
  CHECK(moves.size() == 2);
}

TEST_CASE("value move API agrees with the shared_ptr API") {
  StandardGame game;
  const Coord e2{4, 1, 0, 0};
  CHECK(game.boardExists(e2));
  CHECK_FALSE(game.boardExists(Coord{0, 0, 5, 0}));
  CHECK_FALSE(game.boardExists(Coord{0, 0, 0, 7}));
  CHECK(&game.board(0, 0) == game.getBoard(0, 0).get());

  const SelectedPosition sel = game.selected(e2);
  CHECK(sel.board == game.getBoard(0, 0));
  CHECK(sel.coord() == e2);

  std::set<std::string> viaOld, viaNew;
  for (const auto& to : game.getMoveablePositions(sel)) viaOld.insert(key(to));
  for (const VMove& m : game.legalMovesFrom(e2)) {
    CHECK(m.from == e2);
    viaNew.insert(key(game.selected(m.to)));
  }
  CHECK(viaOld == viaNew);
  CHECK(viaNew.size() == 2); // e3, e4

  CHECK(game.legalMovesFrom(Coord{4, 6, 0, 0}).empty()); // a Black pawn: not the mover's
  CHECK(game.legalMovesFrom(Coord{4, 4, 0, 0}).empty()); // empty square
  CHECK(game.legalMovesFrom(Coord{4, 1, 3, 0}).empty()); // no such board

  auto old = game.clone();
  game.makeMove(VMove{e2, Coord{4, 3, 0, 0}});
  old->makeMove(Chess::Move{old->selected(e2), old->selected(Coord{4, 3, 0, 0})});
  CHECK(snapshot(game) == snapshot(*old));
}

TEST_CASE("legalMovesFrom expands a promotion into four moves") {
  Sandbox game(5);
  game.place(0, 0, 3, make<Pawn>(PieceColor::PIECEWHITE));
  game.place(0, 4, 0, make<King>(PieceColor::PIECEWHITE));
  game.place(0, 4, 4, make<King>(PieceColor::PIECEBLACK));
  std::set<PieceType> promos;
  for (const VMove& m : game.legalMovesFrom(Coord{0, 3, 0, 0})) {
    CHECK(m.to == Coord{0, 4, 0, 0});
    promos.insert(m.promotion);
  }
  CHECK(promos == std::set<PieceType>{PieceType::Queen, PieceType::Rook, PieceType::Bishop, PieceType::Knight});
}

TEST_CASE("position text format: parse and write") {
  const std::string text =
      "# a comment\n"
      "5dchess-position 1\n"
      "title: Standard\n"
      "size: 8\n"
      "rules: double-step castling\n"
      "to-move: white\n"
      "\n"
      "L0 T0w: rnbkqbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBKQBNR\n";
  const Position p = parsePosition(text);
  CHECK(p.title == "Standard");
  CHECK(p.size == 8);
  REQUIRE(p.timelines.size() == 1);
  const BoardData& b = p.timelines[0].boards[0];
  // x = file, y = rank from White's back rank: king on x=3, queen on x=4, White at the bottom.
  CHECK(b.cells[0 * 8 + 3]->type == PieceType::King);
  CHECK(b.cells[0 * 8 + 3]->color == PieceColor::PIECEWHITE);
  CHECK(b.cells[0 * 8 + 4]->type == PieceType::Queen);
  CHECK(b.cells[7 * 8 + 4]->color == PieceColor::PIECEBLACK);
  CHECK(b.cells[1 * 8 + 0]->type == PieceType::Pawn);
  CHECK_FALSE(b.cells[3 * 8 + 3].has_value());
  // writing normalises (no comment, no blank line) and parsing it back is the identity
  CHECK(writePosition(p) == "5dchess-position 1\ntitle: Standard\nsize: 8\nrules: double-step castling\nto-move: white\n"
                            "L0 T0w: rnbkqbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBKQBNR\n");
  CHECK(parsePosition(writePosition(p)) == p);
}

TEST_CASE("position text format: moved pieces, history, branches, present") {
  const std::string text =
      "5dchess-position 1\n"
      "size: 3\n"
      "rules: none\n"
      "to-move: black\n"
      "L0 T0w: k1K/3/P*1R\n"
      "L0 T0b: k1K/3/1P*R\n"
      "L-1 parent: L0\n"
      "L-1 T0b: 3/3/1P*R\n"
      "L-1 T1w: 3/3/1P*R\n";
  const Position p = parsePosition(text);
  CHECK_FALSE(p.doubleStep);
  CHECK_FALSE(p.castling);
  CHECK(p.toMove == PieceColor::PIECEBLACK);
  CHECK(p.present == 1);
  REQUIRE(p.timelines.size() == 2);
  CHECK(p.timelines[0].id == -1);
  CHECK(p.timelines[0].parent == 0);
  CHECK(p.timelines[1].boards.size() == 2);
  CHECK(p.timelines[1].boards[0].cells[0]->moved);
  CHECK(parsePosition(writePosition(p)) == p);
  auto game = p.makeGame();
  CHECK(game->timeLine(-1)->forkAt() == 0);
  CHECK(game->timeLine(-1)->parentId() == 0);
  CHECK(game->presentHalfTurn() == 1);
  CHECK(game->getCurrentTurnColor() == PieceColor::PIECEBLACK);
}

TEST_CASE("position parser rejects malformed input with a line number") {
  const std::string head = "5dchess-position 1\nsize: 3\nrules: none\nto-move: white\n";
  auto fails = [](const std::string& text, const std::string& needle) {
    try {
      parsePosition(text);
    } catch (const ParseError& e) {
      return std::string(e.what()).find(needle) != std::string::npos;
    }
    return false;
  };
  CHECK(fails("", "empty"));
  CHECK(fails("hello\n", "line 1"));
  CHECK(fails(head + "L0 T0w: 3/3\n", "line 5: expected 3 rows"));
  CHECK(fails(head + "L0 T0w: 3/3/4\n", "line 5"));
  CHECK(fails(head + "L0 T0w: 3/3/xx1\n", "unknown piece"));
  CHECK(fails(head + "L0 T0w: 3/3/*2\n", "'*'"));
  CHECK(fails(head + "L0 T0w: 3/3/3\nL0 T2w: 3/3/3\n", "consecutive"));
  CHECK(fails(head + "L0 T0b: 3/3/3\n", "does not match"));
  CHECK(fails(head + "L1 parent: L0\nL1 T0w: 3/3/3\n", "unknown parent"));
  CHECK(fails(head + "colour: red\n", "unknown key"));
  CHECK(fails(head, "no boards"));
  CHECK(fails("5dchess-position 1\nL0 T0w: 3/3/3\n", "'size'"));
}

TEST_CASE("catalog lists the nine modes in menu order") {
  const auto& modes = GameCatalog::modes();
  std::vector<std::string> ids;
  for (const auto& m : modes) ids.push_back(m.id);
  CHECK(ids == std::vector<std::string>{"standard", "omit-bishop", "omit-knight", "omit-queen", "omit-rook",
                                         "knight-vs-bishop", "timeline-invasion", "timeline-battle",
                                         "timeline-fragment"});
  CHECK(GameCatalog::findByTitle("Simplify - No Bishop")->id == "omit-bishop");
  CHECK(GameCatalog::findById("nope") == nullptr);
  CHECK(GameCatalog::create("nope") == nullptr);
}

TEST_CASE("catalog positions equal the old hand-written games board for board") {
  for (const auto& mode : GameCatalog::modes()) {
    CAPTURE(mode.id);
    if (mode.id == "omit-bishop") continue; // deliberately fixed, see the next test
    auto fresh = GameCatalog::create(mode.id);
    auto old = oracle(mode.id);
    REQUIRE(fresh != nullptr);
    REQUIRE(old != nullptr);
    checkSameGame(*fresh, *old);
    CHECK(mode.title == oracleTitle(mode.id));
  }
}

TEST_CASE("omit-bishop fix: Simplify - No Bishop has knights and no bishops (the old class was a copy of No Knight)") {
  auto fresh = GameCatalog::create("omit-bishop");
  REQUIRE(fresh != nullptr);
  // The old class built bishops and no knights, i.e. exactly No Knight.
  CHECK(snapshot(*oracle("omit-bishop")) == snapshot(*oracle("omit-knight")));
  CHECK(snapshot(*fresh) != snapshot(*oracle("omit-knight")));
  const Position p = Position::fromGame(*fresh);
  int knights = 0, bishops = 0;
  for (const auto& cell : p.timelines[0].boards[0].cells) {
    if (!cell) continue;
    knights += cell->type == PieceType::Knight;
    bishops += cell->type == PieceType::Bishop;
  }
  CHECK(knights == 4);
  CHECK(bishops == 0);
  // Same layout as the other 6x6 Simplify modes: R N Q K N R behind a pawn row, like Knight vs Bishop's Black side.
  CHECK(writePosition(p).find("L0 T0w: rnqknr/pppppp/6/6/PPPPPP/RNQKNR\n") != std::string::npos);
}

TEST_CASE("round trip: parse(write(position)) == position over fuzz games, and the game it makes is the same game") {
  int snapshots = 0, branched = 0;
  for (const auto& mode : GameCatalog::modes()) {
    for (unsigned seed : {1u, 2u, 3u}) {
      CAPTURE(mode.id);
      CAPTURE(seed);
      std::mt19937 rng(seed);
      auto game = GameCatalog::create(mode.id);
      for (int turn = 0; turn < 8 and game->result() == GameResult::Ongoing; ++turn) {
        const Position p = Position::fromGame(*game, mode.title);
        const std::string text = writePosition(p);
        CHECK(parsePosition(text) == p);
        CHECK(writePosition(parsePosition(text)) == text);
        auto copy = p.makeGame();
        checkSameGame(*copy, *game);
        ++snapshots;
        branched += p.timelines.size() > 1 and p.timelines.back().parent.has_value();

        if (!buildRandomTurn(*game, rng, [](const IGame&, const Chess::Move&) {})) break;
        game->submitTurn();
        game->resolveResult(50000);
      }
    }
  }
  CHECK(snapshots > 60);
  CHECK(branched > 0);
}

TEST_CASE("a game made from a mid-game position plays on like the original") {
  // The random player can get stuck; take the first seed that plays a few turns.
  std::shared_ptr<IGame> game;
  for (unsigned seed = 1; seed < 40 and !game; ++seed) {
    std::mt19937 rng(seed);
    auto candidate = GameCatalog::create("timeline-battle");
    int played = 0;
    while (played < 5 and candidate->result() == GameResult::Ongoing and
           buildRandomTurn(*candidate, rng, [](const IGame&, const Chess::Move&) {})) {
      candidate->submitTurn();
      candidate->resolveResult(50000);
      ++played;
    }
    if (played == 5 and candidate->result() == GameResult::Ongoing) game = candidate;
  }
  REQUIRE(game != nullptr);
  auto copy = Position::fromGame(*game).makeGame();
  std::mt19937 rngA(5), rngB(5);
  for (int turn = 0; turn < 3 and game->result() == GameResult::Ongoing; ++turn) {
    const bool built = buildRandomTurn(*game, rngA, [](const IGame&, const Chess::Move&) {});
    CHECK(buildRandomTurn(*copy, rngB, [](const IGame&, const Chess::Move&) {}) == built);
    if (!built) break;
    CHECK(snapshot(*game) == snapshot(*copy));
    game->submitTurn();
    copy->submitTurn();
    game->resolveResult(50000);
    copy->resolveResult(50000);
    CHECK(snapshot(*game) == snapshot(*copy));
    CHECK(game->result() == copy->result());
  }
}
