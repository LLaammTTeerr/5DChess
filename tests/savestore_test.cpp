#include <doctest/doctest.h>

#include "test_support.h"

#include "engine/Notation.h"
#include "services/SaveStore.h"

#include <filesystem>
#include <fstream>

using namespace Chess;
using namespace savegame;

namespace {

namespace fs = std::filesystem;

// A fresh directory under the system temp directory, removed at the end of the test.
struct TempDir {
  fs::path path;
  explicit TempDir(const std::string& tag) {
    path = fs::temp_directory_path() / ("5dchess-savestore-" + tag + "-" + std::to_string(std::random_device{}()));
    fs::remove_all(path);
  }
  ~TempDir() {
    std::error_code ec;
    fs::remove_all(path, ec);
  }
};

std::shared_ptr<IGame> playedGame(int turns) {
  auto game = test::newGame("standard");
  const char* moves[] = {"e2>e4", "e7>e5", "g1>f3", "b8>c6", "f1>c4", "g8>f6"};
  for (int i = 0; i < turns && i < 6; ++i) {
    const std::string m = moves[i];
    const auto color = i % 2 == 0 ? PieceColor::PIECEWHITE : PieceColor::PIECEBLACK;
    const int h = i;
    const std::string text = "(L0T" + std::to_string(h / 2 + 1) + ")" + m.substr(0, 2) + ">(L0T" + std::to_string(h / 2 + 1) + ")" + m.substr(3);
    game->makeMove(parseMove(text, color));
    game->submitTurn();
  }
  game->resolveResult(50000);
  return game;
}

void writeFile(const fs::path& path, const std::string& text) {
  fs::create_directories(path.parent_path());
  std::ofstream(path, std::ios::binary) << text;
}

} // namespace

TEST_CASE("slots: write, read back, summarize, delete (files)") {
  TempDir dir("slots");
  SaveStore store(std::make_unique<FileStorage>(dir.path.string()));
  CHECK_FALSE(store.anySlot());
  CHECK(store.slot(1).state == SlotSummary::State::Empty);

  const auto game = playedGame(4);
  bool dropped = true;
  CHECK(store.saveSlot(1, *game, "2026-10-06 14:32", &dropped));
  CHECK_FALSE(dropped);
  CHECK(fs::is_regular_file(dir.path / "slot2.5dr"));
  CHECK(store.anySlot());

  const SlotSummary summary = store.slot(1);
  CHECK(summary.state == SlotSummary::State::Ready);
  CHECK(summary.turns == 4);
  CHECK(summary.date == "2026-10-06 14:32");
  CHECK(summary.describe().find("4 turns") != std::string::npos);
  CHECK(summary.headline().find(summary.title) == 0); // the list row: the title at the left ...
  CHECK(summary.detail() == "4 turns \xC2\xB7 2026-10-06 14:32"); // ... the turns and the date, muted, at the right
  CHECK(store.slot(0).headline() == "Empty");
  CHECK(store.slot(0).detail().empty());
  CHECK(store.slot(0).state == SlotSummary::State::Empty);

  const LoadResult loaded = store.loadSlot(1);
  REQUIRE(loaded);
  CHECK(loaded.game->history() == game->history());
  CHECK(writeRecord(*loaded.game) == writeRecord(*game)); // the date comment is not part of the game

  store.deleteSlot(1);
  CHECK_FALSE(fs::exists(dir.path / "slot2.5dr"));
  CHECK(store.slot(1).state == SlotSummary::State::Empty);
  CHECK_FALSE(store.loadSlot(1));
  store.deleteSlot(1); // deleting an empty slot is fine
}

TEST_CASE("slots: pending moves are reported and not saved; bad slot numbers are refused") {
  MemoryStorage* memory = new MemoryStorage;
  SaveStore store{std::unique_ptr<Storage>(memory)};
  const auto game = playedGame(2);
  game->makeMove(parseMove("(L0T2)g1>(L0T2)f3", PieceColor::PIECEWHITE));
  bool dropped = false;
  CHECK(store.saveSlot(0, *game, "", &dropped));
  CHECK(dropped);
  CHECK(store.slot(0).turns == 2);
  CHECK_FALSE(store.saveSlot(-1, *game, ""));
  CHECK_FALSE(store.saveSlot(kSlots, *game, ""));
  CHECK(store.slot(kSlots).state == SlotSummary::State::Empty);
}

TEST_CASE("autosave: written once a turn was submitted, loads the same game, can be cleared") {
  TempDir dir("auto");
  SaveStore store(std::make_unique<FileStorage>(dir.path.string()));
  CHECK_FALSE(store.hasAutosave());
  CHECK_FALSE(store.autosave(*test::newGame("standard"))); // nothing submitted yet
  CHECK_FALSE(store.hasAutosave());

  const auto game = playedGame(3);
  CHECK(store.autosave(*game));
  CHECK(store.hasAutosave());
  CHECK(fs::is_regular_file(dir.path / "autosave.5dr"));
  const LoadResult loaded = store.loadAutosave();
  REQUIRE(loaded);
  CHECK(loaded.game->history() == game->history());
  CHECK(loaded.game->getCurrentTurnColor() == game->getCurrentTurnColor());
  store.clearAutosave();
  CHECK_FALSE(store.hasAutosave());
  CHECK_FALSE(store.loadAutosave());
}

TEST_CASE("corrupted save files are reported, never thrown, and kept") {
  TempDir dir("corrupt");
  SaveStore store(std::make_unique<FileStorage>(dir.path.string()));
  const std::string garbage[] = {
      "this is not a record\n",
      "5dchess-record 1\nmode: standard\nT1w: (L0T1)e2>(L0T1)e5\n",   // an illegal move
      "5dchess-record 1\nmode: no-such-mode\n",                       // an unknown mode
      "5dchess-record 1\nmode: standard\nT2w: (L0T1)e2>(L0T1)e4\n",     // a label that is not the present
      std::string("5dchess-record 1\nmode: standard\n\0\0\xff\xfe", 36),
      "",
  };
  for (const std::string& text : garbage) {
    writeFile(dir.path / "slot1.5dr", text);
    CHECK_NOTHROW(store.slot(0));
    const LoadResult loaded = store.loadSlot(0);
    CHECK_FALSE(loaded);
    CHECK_FALSE(loaded.error.empty());
    CHECK(fs::exists(dir.path / "slot1.5dr")); // the player may still want to look at the file
  }
  writeFile(dir.path / "slot1.5dr", "this is not a record\n");
  CHECK(store.slot(0).state == SlotSummary::State::Unreadable);
  CHECK(store.slot(0).describe() == "Unreadable save");
  CHECK(store.anySlot());
  store.deleteSlot(0); // but it can be deleted
  CHECK_FALSE(store.anySlot());
}

TEST_CASE("oversized save files are not read") {
  TempDir dir("big");
  SaveStore store(std::make_unique<FileStorage>(dir.path.string()));
  std::string text = "5dchess-record 1\nmode: standard\n# ";
  text.append(kMaxBytes, 'x');
  writeFile(dir.path / "slot3.5dr", text);
  writeFile(dir.path / "autosave.5dr", text);

  std::string out = "stale";
  FileStorage files(dir.path.string());
  CHECK(files.read("slot3", out) == ReadStatus::TooLarge);
  CHECK(out.empty());
  CHECK(store.slot(2).state == SlotSummary::State::Unreadable);
  CHECK_FALSE(store.loadSlot(2));
  CHECK_FALSE(store.loadAutosave());
  CHECK(fs::exists(dir.path / "slot3.5dr"));
  CHECK(fs::exists(dir.path / "autosave.5dr"));

  // exactly at the limit is still read (and then rejected as a record, not as too large)
  writeFile(dir.path / "slot2.5dr", std::string(kMaxBytes, 'y'));
  CHECK(files.read("slot2", out) == ReadStatus::Ok);
  CHECK(out.size() == kMaxBytes);
}

TEST_CASE("a directory or missing path in place of a save does not crash") {
  TempDir dir("odd");
  fs::create_directories(dir.path / "slot1.5dr"); // a directory named like a save
  SaveStore store(std::make_unique<FileStorage>(dir.path.string()));
  CHECK_FALSE(store.loadSlot(0));
  CHECK(store.slot(0).state != SlotSummary::State::Ready);
  CHECK_FALSE(store.hasAutosave());

  SaveStore nowhere(std::make_unique<FileStorage>((dir.path / "does" / "not" / "exist").string()));
  CHECK_FALSE(nowhere.loadAutosave());
  CHECK(nowhere.saveSlot(0, *playedGame(2), "x")); // created on the first write
  CHECK(nowhere.slot(0).state == SlotSummary::State::Ready);
}

TEST_CASE("summarize reads the header without replaying") {
  const SlotSummary custom = summarize("5dchess-record 1\n# saved: 2026-01-02 03:04\nposition:\nT1w: nonsense\nend-position\nT1w: x\nT1b: y\n");
  CHECK(custom.state == SlotSummary::State::Ready);
  CHECK(custom.title == "Custom position");
  CHECK(custom.turns == 2); // the lines inside the position block do not count
  CHECK(custom.date == "2026-01-02 03:04");
  CHECK(summarize("").state == SlotSummary::State::Unreadable);
  CHECK(summarize("5dchess-record 2\nmode: standard\n").state == SlotSummary::State::Unreadable);
  CHECK(summarize("5dchess-record 1\n").state == SlotSummary::State::Unreadable); // no header
}

TEST_CASE("autosaveSummary: Ready, Unreadable (garbage, oversized) or Empty, so Continue is never a dead end") {
  TempDir dir("autosum");
  SaveStore store(std::make_unique<FileStorage>(dir.path.string()));
  CHECK(store.autosaveSummary().state == SlotSummary::State::Empty);
  CHECK(store.autosave(*playedGame(2)));
  CHECK(store.autosaveSummary().state == SlotSummary::State::Ready);
  writeFile(dir.path / "autosave.5dr", "garbage");
  CHECK(store.autosaveSummary().state == SlotSummary::State::Unreadable);
  CHECK(store.hasAutosave());
  writeFile(dir.path / "autosave.5dr", std::string(kMaxBytes + 1, 'x'));
  CHECK(store.autosaveSummary().state == SlotSummary::State::Unreadable);
  store.clearAutosave();
  CHECK(store.autosaveSummary().state == SlotSummary::State::Empty);
}

TEST_CASE("temporary files do not linger after a write") {
  TempDir dir("tmp");
  FileStorage files(dir.path.string());
  CHECK(files.write("slot1", "x"));
  CHECK(files.write("slot1", "y"));
  int entries = 0;
  for (const auto& e : fs::directory_iterator(dir.path)) {
    (void)e;
    ++entries;
  }
  CHECK(entries == 1);
}
