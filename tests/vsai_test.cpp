// Tests of the pure logic behind "Play vs Computer" (include/play/VsAi.h): names, the player's side, the search seed, the record
// metadata line and how it travels through the save store, and rebuilding a game a few turns back (what Undo does).
#include <doctest/doctest.h>

#include "test_support.h"

#include "ai/Play.h"
#include "engine/Notation.h"
#include "engine/Position.h"
#include "play/VsAi.h"
#include "services/SaveStore.h"
#include "services/SettingsStore.h"

#include <set>

using namespace Chess;
using namespace play;

namespace {

// A few turns of a real game (the AI plays both sides at Easy, so time jumps and timelines turn up in the larger modes).
std::shared_ptr<IGame> playedByAi(const std::string& mode, int turns) {
  auto game = test::newGame(mode);
  for (int i = 0; i < turns; ++i) {
    if (game->result() != GameResult::Ongoing) break;
    if (!ai::playTurn(*game, {ai::Level::Easy, static_cast<std::uint64_t>(100 + i)})) break;
  }
  game->resolveResult(50000);
  return game;
}

std::string positionOf(const IGame& game) { return Core::writePosition(Core::Position::fromGame(game)); }

} // namespace

TEST_CASE("vs-ai: side and level names round-trip, unknown names are refused") {
  for (SideChoice s : {SideChoice::White, SideChoice::Black, SideChoice::Random}) {
    SideChoice back = SideChoice::Random == s ? SideChoice::White : SideChoice::Random;
    CHECK(fromName(name(s), back));
    CHECK(back == s);
  }
  for (AiLevel l : {AiLevel::Easy, AiLevel::Normal, AiLevel::Hard}) {
    AiLevel back = AiLevel::Easy == l ? AiLevel::Hard : AiLevel::Easy;
    CHECK(fromName(name(l), back));
    CHECK(back == l);
  }
  SideChoice side = SideChoice::Black;
  AiLevel level = AiLevel::Hard;
  CHECK_FALSE(fromName("purple", side));
  CHECK_FALSE(fromName("White", side)); // names are stored lower case
  CHECK_FALSE(fromName("", level));
  CHECK(side == SideChoice::Black); // untouched: the caller's default stays
  CHECK(level == AiLevel::Hard);
}

TEST_CASE("vs-ai: which side the computer plays") {
  for (std::uint64_t seed : {0ull, 1ull, 42ull, 0xFFFFFFFFFFFFFFFFull}) {
    CHECK(humanSide(SideChoice::White, seed) == PieceColor::PIECEWHITE);
    CHECK(humanSide(SideChoice::Black, seed) == PieceColor::PIECEBLACK);
    CHECK(makeVsAi(SideChoice::White, AiLevel::Easy, seed).aiColor() == PieceColor::PIECEBLACK);
    CHECK(makeVsAi(SideChoice::Black, AiLevel::Easy, seed).aiColor() == PieceColor::PIECEWHITE);
  }
  // Random is decided by the seed alone (so a replayed game gets the same side), and both sides do come up
  int white = 0, black = 0;
  for (std::uint64_t seed = 0; seed < 400; ++seed) {
    const PieceColor c = humanSide(SideChoice::Random, seed);
    CHECK(c == humanSide(SideChoice::Random, seed));
    (c == PieceColor::PIECEWHITE ? white : black)++;
  }
  CHECK(white > 120);
  CHECK(black > 120);
}

TEST_CASE("vs-ai: the search seed depends on the game seed and on the turn, and on nothing else") {
  CHECK(searchSeed(7, 3) == searchSeed(7, 3));
  std::set<std::uint64_t> seen;
  for (std::size_t turn = 0; turn < 50; ++turn) seen.insert(searchSeed(7, turn));
  CHECK(seen.size() == 50);
  CHECK(searchSeed(7, 3) != searchSeed(8, 3));
}

TEST_CASE("vs-ai: the record metadata line round-trips and is strict") {
  for (PieceColor c : {PieceColor::PIECEWHITE, PieceColor::PIECEBLACK})
    for (AiLevel l : {AiLevel::Easy, AiLevel::Normal, AiLevel::Hard})
      for (std::uint64_t seed : {0ull, 5ull, 123456789012345ull, 0xFFFFFFFFFFFFFFFFull}) {
        const VsAi vs{c, l, seed};
        const std::string line = formatMeta(vs);
        CHECK(line.front() == '#'); // a comment: record readers skip it
        REQUIRE(parseMeta(line).has_value());
        CHECK(*parseMeta(line) == vs);
      }
  CHECK(formatMeta({PieceColor::PIECEBLACK, AiLevel::Hard, 9}) == "# vs-computer: you=black level=hard seed=9");

  CHECK_FALSE(parseMeta("# saved: 2026-10-06 14:32"));
  CHECK_FALSE(parseMeta("# vs-computer: you=white level=easy"));                       // no seed
  CHECK_FALSE(parseMeta("# vs-computer: you=white level=easy seed=1 seed=2"));        // repeated
  CHECK_FALSE(parseMeta("# vs-computer: you=white level=easy seed=1 extra=1"));       // unknown field
  CHECK_FALSE(parseMeta("# vs-computer: you=random level=easy seed=1"));              // the side is resolved when saved
  CHECK_FALSE(parseMeta("# vs-computer: you=white level=impossible seed=1"));
  CHECK_FALSE(parseMeta("# vs-computer: you=white level=easy seed=-1"));
  CHECK_FALSE(parseMeta("# vs-computer: you=white level=easy seed=1x"));
  CHECK_FALSE(parseMeta("# vs-computer: you=white level=easy seed=99999999999999999999")); // beyond 64 bits
  CHECK_FALSE(parseMeta("# vs-computer: you=white level=easy seed="));
}

TEST_CASE("vs-ai: the metadata sits among the header comments of a record and does not change the game") {
  const auto game = playedByAi("standard", 4);
  const std::string plain = writeRecord(*game);
  CHECK_FALSE(findMeta(plain).has_value()); // an old record: a hot-seat game

  const VsAi vs{PieceColor::PIECEBLACK, AiLevel::Normal, 777};
  const std::string tagged = withMeta(plain, vs);
  REQUIRE(findMeta(tagged).has_value());
  CHECK(*findMeta(tagged) == vs);
  CHECK(tagged.substr(0, tagged.find('\n')) == "5dchess-record 1"); // still starts with the magic

  const auto loaded = loadRecord(tagged); // readers that know nothing of it skip the comment
  CHECK(writeRecord(*loaded) == plain);

  // after the header it is just a comment too, but not ours to look for
  std::string late = plain + formatMeta(vs) + "\n";
  CHECK_FALSE(findMeta(late).has_value());
}

TEST_CASE("vs-ai: autosave and slots keep the mode; records without it load as two-player games") {
  auto game = playedByAi("standard", 3);
  const VsAi vs{PieceColor::PIECEWHITE, AiLevel::Hard, 31337};

  savegame::SaveStore store(std::make_unique<savegame::MemoryStorage>());
  CHECK(store.autosave(*game, &vs));
  const auto resumed = store.loadAutosave();
  REQUIRE(resumed);
  REQUIRE(resumed.vsAi.has_value());
  CHECK(*resumed.vsAi == vs);
  CHECK(resumed.game->history() == game->history());
  CHECK(store.autosaveSummary().vsComputer);
  CHECK(store.autosaveSummary().describe().find("vs Computer") != std::string::npos);

  CHECK(store.saveSlot(1, *game, "2026-10-06 14:32", nullptr, &vs));
  const auto slot = store.loadSlot(1);
  REQUIRE(slot);
  REQUIRE(slot.vsAi.has_value());
  CHECK(*slot.vsAi == vs);
  CHECK(store.slot(1).date == "2026-10-06 14:32");
  CHECK(store.slot(1).vsComputer);

  // the same game saved without it, as every save of an older version
  CHECK(store.saveSlot(2, *game, "2026-10-06 14:33"));
  const auto hotSeat = store.loadSlot(2);
  REQUIRE(hotSeat);
  CHECK_FALSE(hotSeat.vsAi.has_value());
  CHECK_FALSE(store.slot(2).vsComputer);
  CHECK(store.slot(2).describe().find("vs Computer") == std::string::npos);
  CHECK(writeRecord(*hotSeat.game) == writeRecord(*slot.game));

  // a damaged metadata line costs the mode, not the game
  std::string text = withMeta(writeRecord(*game), vs);
  const size_t at = text.find("level=hard");
  text.replace(at, 10, "level=zzzz");
  const auto damaged = savegame::loadText(text);
  REQUIRE(damaged);
  CHECK_FALSE(damaged.vsAi.has_value());
}

TEST_CASE("vs-ai: replayPrefix rebuilds the game as it was") {
  for (const char* mode : {"standard", "timeline-battle", "timeline-fragment"}) {
    CAPTURE(mode);
    const auto game = playedByAi(mode, 6);
    const std::size_t n = game->history().size();
    REQUIRE(n >= 1);
    for (std::size_t keep : {std::size_t(0), n / 2, n - 1, n}) {
      CAPTURE(keep);
      const auto earlier = replayPrefix(*game, keep);
      REQUIRE(earlier);
      REQUIRE(earlier->history().size() == keep);
      for (std::size_t i = 0; i < keep; ++i) CHECK(earlier->history()[i] == game->history()[i]);
      CHECK(earlier->pendingMoves().empty());
      CHECK(earlier->modeId() == game->modeId());
      earlier->resolveResult(50000);
      // the same position as the original had after `keep` turns: replaying the record of that prefix gives it
      std::string record = writeRecord(*earlier);
      CHECK(positionOf(*loadRecord(record)) == positionOf(*earlier));
      if (keep == n) CHECK(positionOf(*earlier) == positionOf(*game));
    }
    CHECK_FALSE(replayPrefix(*game, n + 1));
  }
}

TEST_CASE("vs-ai: replayPrefix works for a game that started from an embedded position") {
  const auto origin = playedByAi("standard", 2);
  const auto fromMiddle = Core::Position::fromGame(*origin).makeGame(); // no catalog mode: the record embeds the position
  REQUIRE(fromMiddle->modeId().empty());
  for (int i = 0; i < 4; ++i) REQUIRE(ai::playTurn(*fromMiddle, {ai::Level::Easy, static_cast<std::uint64_t>(i + 1)}));
  const auto back = replayPrefix(*fromMiddle, 2);
  REQUIRE(back);
  CHECK(back->history().size() == 2);
  CHECK(back->history()[0] == fromMiddle->history()[0]);
  CHECK(back->history()[1] == fromMiddle->history()[1]);
  CHECK(back->startPosition() == fromMiddle->startPosition());
}

TEST_CASE("vs-ai: undoing takes back whole turns: the player's turn is the AI's reply away") {
  // what PlayScreen does for Undo: after the player's turn and the computer's reply, drop two turns
  const VsAi vs{PieceColor::PIECEWHITE, AiLevel::Easy, 5};
  auto game = test::newGame("standard");
  for (int round = 0; round < 3; ++round) {
    // the player's turn (stood in for by another seed of the engine) ...
    REQUIRE(ai::playTurn(*game, {AiLevel::Easy, static_cast<std::uint64_t>(900 + round)}));
    // ... and the computer's reply, seeded like the screen does
    REQUIRE(ai::playTurn(*game, {vs.level, searchSeed(vs.seed, game->history().size())}));
  }
  REQUIRE(game->history().size() == 6);
  const std::string atFour = positionOf(*replayPrefix(*game, 4));
  const auto back = replayPrefix(*game, game->history().size() - 2);
  REQUIRE(back);
  CHECK(positionOf(*back) == atFour);
  CHECK(back->getCurrentTurnColor() == vs.human); // the player to move again
  // and the computer, asked again from there, answers exactly as before (same seed, same position)
  back->resolveResult(50000);
  for (const auto& played : game->history()[4].moves) back->makeMove(played.move);
  back->submitTurn();
  back->resolveResult(50000);
  auto again = back->clone();
  REQUIRE(ai::playTurn(*again, {vs.level, searchSeed(vs.seed, 5)}));
  CHECK(again->history().back() == game->history()[5]);
}

TEST_CASE("vs-ai: the Versus choices live in the settings file by name, with a safe fallback") {
  Settings s;
  CHECK_FALSE(s.vsComputer);
  CHECK(s.vsSide == SideChoice::White);
  CHECK(s.vsLevel == AiLevel::Normal);
  SettingsStore::apply(settingsfile::parse("opponent=computer\nvs_side=random\nvs_level=hard\n"), s);
  CHECK(s.vsComputer);
  CHECK(s.vsSide == SideChoice::Random);
  CHECK(s.vsLevel == AiLevel::Hard);
  // unknown values: the side and level keep what they were, an unknown opponent is two players
  SettingsStore::apply(settingsfile::parse("opponent=robot\nvs_side=purple\nvs_level=impossible\n"), s);
  CHECK_FALSE(s.vsComputer);
  CHECK(s.vsSide == SideChoice::Random);
  CHECK(s.vsLevel == AiLevel::Hard);
  // and a file written by an older version has none of the keys
  Settings fresh;
  SettingsStore::apply(settingsfile::parse("sfx=off\n"), fresh);
  CHECK(fresh == Settings{[] { Settings d; d.sfx = false; return d; }()});
}

TEST_CASE("vs-ai: what Undo takes back") {
  const VsAi white{PieceColor::PIECEWHITE, AiLevel::Easy, 1}, black{PieceColor::PIECEBLACK, AiLevel::Easy, 1};
  const auto W = PieceColor::PIECEWHITE, B = PieceColor::PIECEBLACK;
  // playing White: the player to move with a full round behind (2), at the start (0), moves pending (left to the move-by-move undo)
  CHECK(takeBackCount(white, W, 2, false, false) == 2);
  CHECK(takeBackCount(white, W, 0, false, false) == 0);
  CHECK(takeBackCount(white, W, 4, true, false) == 0);
  // the computer thinking after the player's turn: only that turn
  CHECK(takeBackCount(white, B, 1, false, false) == 1);
  CHECK(takeBackCount(white, B, 3, false, false) == 1);
  // while it plays its moves, or has pending moves of its own: nothing
  CHECK(takeBackCount(white, B, 3, true, true) == 0);
  CHECK(takeBackCount(white, B, 3, false, true) == 0);
  // playing Black: the computer's opening stays
  CHECK(takeBackCount(black, B, 1, false, false) == 0); // its opening only
  CHECK(takeBackCount(black, B, 3, false, false) == 2);
  CHECK(takeBackCount(black, W, 2, false, false) == 1); // the computer thinks after the player's first turn
  CHECK(takeBackCount(black, W, 0, false, false) == 0); // about to open
  CHECK(takeBackCount(black, W, 1, false, false) == 0); // (cannot be: White is to move only after Black's turn)
}

TEST_CASE("vs-ai: records with CRLF line ends or without a final newline carry the mode too") {
  const auto game = playedByAi("standard", 2);
  const VsAi vs{PieceColor::PIECEBLACK, AiLevel::Hard, 99};
  std::string tagged = withMeta(writeRecord(*game), vs);
  std::string crlf;
  for (char c : tagged) {
    if (c == '\n') crlf += '\r';
    crlf += c;
  }
  REQUIRE(findMeta(crlf).has_value());
  CHECK(*findMeta(crlf) == vs);
  const auto loadedCrlf = savegame::loadText(crlf);
  REQUIRE(loadedCrlf);
  CHECK(loadedCrlf.vsAi == vs);

  while (!tagged.empty() && tagged.back() == '\n') tagged.pop_back();
  REQUIRE(findMeta(tagged).has_value());
  const auto loadedBare = savegame::loadText(tagged);
  REQUIRE(loadedBare);
  CHECK(loadedBare.vsAi == vs);

  // the magic alone, with no newline at all: the line is added on its own line
  const std::string magic = "5dchess-record 1";
  CHECK(findMeta(withMeta(magic, vs)) == vs);
}
