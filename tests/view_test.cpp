#include <doctest/doctest.h>

#include "play/MultiverseView.h"
#include "engine/Position.h"
#include "services/SettingsFile.h"
#include "Render/PieceThemes.h"
#include <filesystem>
#include <fstream>
#include <set>
#include <sstream>
#include "test_support.h"

using namespace Chess;
using Chess::Core::Coord;
using CMove = Chess::Core::Move;
using namespace test;
using play::BoardRole;
using play::MultiverseView;

namespace {
void playTurn(IGame& game, const CMove& move) {
  game.makeMove(move);
  game.submitTurn();
  game.resolveResult();
}
} // namespace

TEST_CASE("MultiverseView: the start position has one mandatory board, one timeline and no check") {
  auto game = newGame("standard");
  const MultiverseView view = MultiverseView::build(*game);
  REQUIRE(view.boards.size() == 1);
  CHECK(view.boards[0].role == BoardRole::Mandatory);
  CHECK(view.boards[0].whiteToMove);
  CHECK_FALSE(view.boards[0].inactive);
  REQUIRE(view.timelines.size() == 1);
  CHECK(view.timelines[0].active);
  CHECK_FALSE(view.timelines[0].created);
  CHECK(view.checks.empty());
  CHECK(view.jumps.empty());
  CHECK(view.presentHalfTurn == 0);
}

TEST_CASE("MultiverseView: after a move the old board is history and the new board is not yet the opponent's to move on") {
  auto game = newGame("standard");
  game->makeMove(CMove{Coord{4, 1, 0, 0}, Coord{4, 3, 0, 0}});
  const MultiverseView view = MultiverseView::build(*game);
  REQUIRE(view.boards.size() == 2);
  CHECK(view.boards[0].role == BoardRole::Past);
  CHECK(view.boards[1].halfTurn == 1);
  CHECK_FALSE(view.boards[1].whiteToMove);
  CHECK(view.boards[1].role == BoardRole::Past); // Black's board while White is still to move: nobody can move on it yet
  CHECK(view.presentHalfTurn == 1);
  CHECK(view.jumps.empty()); // a move within one board is no jump
  REQUIRE(view.board(0, 1));
  CHECK_FALSE(view.board(0, 5));
}

TEST_CASE("MultiverseView: a time-travel move creates a White timeline and is listed as a jump") {
  auto game = newGame("standard");
  playTurn(*game, CMove{Coord{4, 1, 0, 0}, Coord{4, 3, 0, 0}});
  playTurn(*game, CMove{Coord{4, 6, 1, 0}, Coord{4, 4, 1, 0}});
  const Coord knight{6, 0, 2, 0};
  CMove jump;
  bool found = false;
  for (const CMove& m : game->legalMovesFrom(knight))
    if (m.to.l != 0 || m.to.t != 2) { jump = m; found = true; break; }
  REQUIRE(found);
  game->makeMove(jump);

  const MultiverseView view = MultiverseView::build(*game);
  REQUIRE(view.jumps.size() == 1);
  CHECK(view.jumps[0].pending);
  CHECK(view.jumps[0].piece.type == PieceType::Knight);
  CHECK(view.jumps[0].move == jump);
  REQUIRE(view.timelines.size() == 2);
  const play::TimelineInfo* created = view.timeline(1);
  REQUIRE(created);
  CHECK(created->created);
  CHECK(created->byWhite);
  CHECK(created->parent == 0);
  CHECK(created->forkHalfTurn == jump.to.t);
  CHECK(view.timeline(0)->active);
  CHECK(view.timeline(7) == nullptr);
  // Boards are sorted like BoardLayout::boards(): by timeline, then half-turn
  for (size_t i = 1; i < view.boards.size(); ++i)
    CHECK(std::make_pair(view.boards[i - 1].timeline, view.boards[i - 1].halfTurn) <
          std::make_pair(view.boards[i].timeline, view.boards[i].halfTurn));

  game->undo();
  CHECK(MultiverseView::build(*game).jumps.empty());
}

TEST_CASE("MultiverseView: a checking attack is reported as a line from the attacker to the king") {
  Sandbox game(5);
  game.place(0, 0, 0, make(PieceType::King, PieceColor::PIECEWHITE));
  game.place(0, 4, 4, make(PieceType::King, PieceColor::PIECEBLACK));
  game.place(0, 0, 4, make(PieceType::Rook, PieceColor::PIECEBLACK));
  const MultiverseView view = MultiverseView::build(game);
  REQUIRE(view.checks.size() == 1);
  CHECK(view.checks[0].attacker == Coord{0, 4, 0, 0});
  CHECK(view.checks[0].king == Coord{0, 0, 0, 0});
}

TEST_CASE("MultiverseView: every built-in mode builds a consistent view") {
  for (const std::string& id : allModeIds()) {
    CAPTURE(id);
    auto game = newGame(id);
    const MultiverseView view = MultiverseView::build(*game);
    CHECK_FALSE(view.boards.empty());
    CHECK(view.timelines.size() == size_t(game->timeLineCount()));
    int mandatory = 0;
    for (const auto& b : view.boards) mandatory += b.role == BoardRole::Mandatory;
    CHECK(mandatory == int(game->mandatoryBoards().size()));
  }
}

namespace {
std::shared_ptr<IGame> uiPosition(const char* name) {
  return Chess::Core::loadPositionFile(std::string(FDCHESS_UI_POSITIONS_DIR) + "/" + name).makeGame();
}
} // namespace

TEST_CASE("UI test positions: check, promotion and an inactive timeline are what the screenshot scripts expect") {
  {
    auto game = uiPosition("check.5dp");
    const MultiverseView view = MultiverseView::build(*game);
    REQUIRE(view.checks.size() == 1);
    CHECK(view.checks[0].attacker == Coord{4, 7, 4, 0}); // the rook on e8
    CHECK(view.checks[0].king == Coord{4, 0, 4, 0});     // the king on e1
  }
  {
    auto game = uiPosition("promotion.5dp");
    CHECK(game->legalMovesFrom(Coord{4, 6, 4, 0}).size() == 4); // e7: four promotion choices onto e8
  }
  {
    auto game = uiPosition("inactive.5dp");
    const MultiverseView view = MultiverseView::build(*game);
    REQUIRE(view.timelines.size() == 3);
    CHECK(view.timeline(0)->active);
    CHECK(view.timeline(1)->active);
    CHECK_FALSE(view.timeline(2)->active);
    CHECK(view.timeline(2)->created);
    CHECK(view.board(2, 4)->inactive);
    CHECK(view.board(2, 4)->role == BoardRole::Optional); // White may still move there, but need not
    CHECK(view.board(0, 4)->role == BoardRole::Mandatory);
    CHECK(view.board(1, 4)->role == BoardRole::Mandatory);
  }
}

TEST_CASE("settingsfile: parse ignores comments, blanks and malformed lines; format round-trips") {
  const auto values = settingsfile::parse("# a comment\n\ntheme = Pixel\nmusic=Some Track\nbroken line\n=novalue\nsfx=off\nsfx=on\r\n");
  CHECK(values.at("theme") == "Pixel");
  CHECK(values.at("music") == "Some Track");
  CHECK(values.at("sfx") == "on"); // the later duplicate wins, a trailing CR is dropped
  CHECK(values.size() == 3);
  CHECK(settingsfile::parse(settingsfile::format(values)) == values);
  CHECK(settingsfile::parse("").empty());
}

TEST_CASE("settingsfile: booleans fall back on anything unknown") {
  CHECK(settingsfile::toBool("on", false));
  CHECK_FALSE(settingsfile::toBool("off", true));
  CHECK(settingsfile::toBool("maybe", true));
  CHECK_FALSE(settingsfile::toBool("", false));
  CHECK(std::string(settingsfile::fromBool(true)) == "on");
}

TEST_CASE("settingsfile: the file lives in the platform's config directory") {
  using settingsfile::Platform;
  auto env = [](std::map<std::string, std::string> vars) {
    return [vars](const char* name) {
      const auto it = vars.find(name);
      return it == vars.end() ? std::string() : it->second;
    };
  };
  CHECK(settingsfile::pathFor(Platform::Linux, env({{"XDG_CONFIG_HOME", "/cfg"}, {"HOME", "/home/a"}})) == "/cfg/5dchess/settings.txt");
  CHECK(settingsfile::pathFor(Platform::Linux, env({{"HOME", "/home/a"}})) == "/home/a/.config/5dchess/settings.txt");
  CHECK(settingsfile::pathFor(Platform::Linux, env({{"XDG_CONFIG_HOME", "relative"}, {"HOME", "/home/a"}})) ==
        "/home/a/.config/5dchess/settings.txt"); // the XDG spec: only absolute paths count
  CHECK(settingsfile::pathFor(Platform::Linux, env({})).empty());
  CHECK(settingsfile::pathFor(Platform::MacOS, env({{"HOME", "/Users/a"}})) == "/Users/a/Library/Application Support/5DChess/settings.txt");
  CHECK(settingsfile::pathFor(Platform::Windows, env({{"APPDATA", "C:\\Users\\a\\AppData\\Roaming"}})) ==
        "C:\\Users\\a\\AppData\\Roaming\\5DChess\\settings.txt");
  CHECK(settingsfile::pathFor(Platform::Windows, env({})).empty());
}

TEST_CASE("piece themes: every theme's textures are in the manifest and on disk") {
  const std::filesystem::path assets = std::filesystem::path(FDCHESS_GUIDE_DIR).parent_path();
  std::set<std::string> ids;
  std::ifstream manifest(assets / "manifest.txt");
  REQUIRE(manifest);
  for (std::string line; std::getline(manifest, line);) {
    std::istringstream fields(line);
    std::string kind, id, path;
    if (fields >> kind >> id >> path && kind == "texture") {
      ids.insert(id);
      CHECK_MESSAGE(std::filesystem::exists(assets / path), id << " -> " << path);
    }
  }
  for (const char* name : Themes::names) {
    const PieceTheme& theme = *Themes::byName(name);
    for (const char* side : {"white", "black"})
      for (const char* piece : {"king", "queen", "rook", "bishop", "knight", "pawn"}) {
        const std::string id = std::string(theme.prefix) + side + "_" + piece;
        CHECK_MESSAGE(ids.count(id) == 1, id);
        if (theme.hasBlink) CHECK_MESSAGE(ids.count(id + "_blink") == 1, id << "_blink");
      }
  }
}

TEST_CASE("piece themes: names round-trip; a removed theme's saved name is unknown, so the default Pixel stays") {
  for (const char* name : Themes::names) {
    REQUIRE(Themes::byName(name) != nullptr);
    CHECK(std::string(Themes::nameOf(*Themes::byName(name))) == name);
  }
  CHECK(std::string(Themes::names[0]) == "Pixel"); // the default and the first Settings entry
  // SettingsStore keeps the default unless byName finds the stored name.
  for (const char* gone : {"Classic", "Modern", "Fantasy", "classic", ""}) CHECK(Themes::byName(gone) == nullptr);
  CHECK(std::string(Themes::nameOf(PieceTheme{"piece.classic.", false})) == "Pixel");
}
