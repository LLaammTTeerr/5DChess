// Puzzles: the metadata of a puzzle file, the progress text, the enumeration and proofs of puzzles/Solver.h, and the shipped set
// (assets/puzzles; the exhaustive proof of all of them also runs as the `puzzle_check` ctest).
#include <doctest/doctest.h>

#include "engine/Notation.h"
#include "puzzles/Progress.h"
#include "puzzles/Puzzle.h"
#include "puzzles/Solver.h"
#include "test_support.h"

#include <set>
#include <string>

using namespace Chess;

namespace {

const char* kBackRank =
    "5dchess-position 1\n"
    "title: Back rank\n"
    "size: 8\n"
    "rules: double-step\n"
    "to-move: white\n"
    "goal: mate-in-1\n"
    "difficulty: 1\n"
    "hint: Use the rook.\n"
    "solution: (L0T1)e1>(L0T1)e8\n"
    "L0 T1w: 6k1/5ppp/8/8/8/8/5PPP/4R1K1\n";

std::string without(const std::string& text, const std::string& line) {
  std::string out = text;
  const size_t at = out.find(line);
  REQUIRE(at != std::string::npos);
  out.erase(at, line.size());
  return out;
}

std::string replaced(std::string text, const std::string& from, const std::string& to) {
  const size_t at = text.find(from);
  REQUIRE(at != std::string::npos);
  text.replace(at, from.size(), to);
  return text;
}

const std::vector<puzzles::Puzzle>& shipped() {
  static const bool set = (puzzles::setDirectory(FDCHESS_PUZZLES_DIR), true);
  (void)set;
  return puzzles::all();
}

std::string notation(const puzzles::Turn& turn) {
  std::string s;
  for (const auto& m : turn) s += (s.empty() ? "" : " ") + toNotation(m, m.promotion != PieceType::Queen);
  return s;
}

} // namespace

TEST_CASE("puzzle: metadata of a puzzle file") {
  const puzzles::Puzzle p = puzzles::parse("back-rank", kBackRank);
  CHECK(p.id == "back-rank");
  CHECK(p.title == "Back rank");
  CHECK(p.hint == "Use the rook.");
  CHECK(p.goal == puzzles::Goal::MateIn1);
  CHECK(p.tier == 1);
  CHECK(p.banner() == "White to move - mate in 1");
  REQUIRE(p.solution.size() == 1);
  REQUIRE(p.solution[0].size() == 1);
  CHECK(notation(p.solution[0]) == "(L0T1)e1>(L0T1)e8");
  CHECK(p.start()->getCurrentTurnColor() == PieceColor::PIECEWHITE);
}

TEST_CASE("puzzle: a mate in 2 holds three turns, a turn may hold several moves") {
  std::string text = replaced(kBackRank, "goal: mate-in-1", "goal: mate-in-2");
  text = replaced(text, "solution: (L0T1)e1>(L0T1)e8", "solution: (L0T1)e1>(L0T1)e2 / (L0T1)g8>(L0T1)f8 (L0T1)h7>(L0T1)h6 / (L0T2)e2>(L0T2)e8");
  const puzzles::Puzzle p = puzzles::parse("x", text);
  CHECK(p.mateIn() == 2);
  REQUIRE(p.solution.size() == 3);
  CHECK(p.solution[1].size() == 2);
  CHECK(p.banner() == "White to move - mate in 2");
}

TEST_CASE("puzzle: bad puzzle files are rejected with a ParseError that names the line") {
  CHECK_THROWS_AS(puzzles::parse("x", without(kBackRank, "goal: mate-in-1\n")), Core::ParseError);
  CHECK_THROWS_AS(puzzles::parse("x", without(kBackRank, "difficulty: 1\n")), Core::ParseError);
  CHECK_THROWS_AS(puzzles::parse("x", without(kBackRank, "solution: (L0T1)e1>(L0T1)e8\n")), Core::ParseError);
  CHECK_THROWS_AS(puzzles::parse("x", replaced(kBackRank, "goal: mate-in-1", "goal: mate-in-3")), Core::ParseError);
  CHECK_THROWS_AS(puzzles::parse("x", replaced(kBackRank, "difficulty: 1", "difficulty: 4")), Core::ParseError);
  CHECK_THROWS_AS(puzzles::parse("x", replaced(kBackRank, "difficulty: 1", "difficulty: 0")), Core::ParseError);
  CHECK_THROWS_AS(puzzles::parse("x", replaced(kBackRank, "to-move: white", "to-move: black")), Core::ParseError);
  // a mate in 1 with a three-turn line, a mate in 2 with one turn
  CHECK_THROWS_AS(puzzles::parse("x", replaced(kBackRank, "e8\n", "e8 / (L0T1)g8>(L0T1)f8 / (L0T2)e1>(L0T2)e2\n")), Core::ParseError);
  CHECK_THROWS_AS(puzzles::parse("x", replaced(kBackRank, "mate-in-1", "mate-in-2")), Core::ParseError);
  CHECK_THROWS_AS(puzzles::parse("x", replaced(kBackRank, "(L0T1)e1>(L0T1)e8", "e1e8")), Core::ParseError);
  CHECK_THROWS_AS(puzzles::parse("x", kBackRank + std::string("goal: mate-in-1\n")), Core::ParseError); // after the boards it is an unknown key
  try {
    puzzles::parse("x", replaced(kBackRank, "size: 8", "size: 9"));
    FAIL("size 9 must not parse");
  } catch (const Core::ParseError& e) {
    CHECK(std::string(e.what()).find("line 3") != std::string::npos); // the blanked puzzle lines keep the numbering
  }
}

TEST_CASE("puzzle: a bare position can be examined without the puzzle lines") {
  const puzzles::Puzzle p = puzzles::parse("x", "5dchess-position 1\nsize: 4\nrules: none\nto-move: white\nL0 T1w: 3k/4/4/K2R\n", false);
  CHECK(p.start()->dim() == 4);
}

TEST_CASE("puzzle: progress text round trips and survives garbage") {
  puzzles::Progress p;
  CHECK(p.markSolved("t1-01-back-rank"));
  CHECK_FALSE(p.markSolved("t1-01-back-rank")); // not news
  CHECK(p.markSolved("t3-02-x_y"));
  CHECK_FALSE(p.markSolved("Bad Id"));
  CHECK_FALSE(p.markSolved(""));
  CHECK(p.count() == 2);
  CHECK(puzzles::Progress::fromText(p.toText()) == p);
  CHECK(puzzles::Progress::fromText(p.toText()).solved("t3-02-x_y"));

  CHECK(puzzles::Progress::fromText("").count() == 0);
  CHECK(puzzles::Progress::fromText("solved=\n").count() == 0);
  CHECK(puzzles::Progress::fromText("nonsense\nsolved=a,,b,../x,C\nother=1\n").count() == 2); // a and b
  CHECK(puzzles::Progress::fromText(std::string(70000, 'x')).count() == 0);
  std::string many = "solved=";
  for (int i = 0; i < 3000; ++i) many += "p" + std::to_string(i) + ",";
  CHECK(puzzles::Progress::fromText(many).count() == 1000); // bounded
}

TEST_CASE("puzzle: the shipped set") {
  const auto& all = shipped();
  REQUIRE(all.size() >= 12);
  std::set<std::string> ids;
  int perTier[puzzles::kTiers] = {};
  int mateInTwo = 0, branching = 0;
  for (const auto& p : all) {
    INFO(p.id);
    CHECK(ids.insert(p.id).second);
    CHECK(puzzles::Progress::validId(p.id));
    CHECK(p.title.size() > 2);
    CHECK_FALSE(p.hint.empty());
    for (char c : p.title + p.hint) CHECK((c >= 32 && c < 127)); // the UI fonts hold ASCII only
    ++perTier[p.tier - 1];
    mateInTwo += p.goal == puzzles::Goal::MateIn2;
  }
  CHECK(perTier[0] >= 4);
  CHECK(perTier[1] >= 3);
  CHECK(perTier[2] >= 4);
  CHECK(mateInTwo >= 3);
  for (size_t i = 1; i < all.size(); ++i) CHECK(all[i - 1].tier <= all[i].tier); // ordered by tier
  (void)branching;
}

TEST_CASE("puzzle: every mate in 1 of the set is proven, tier 2 needs travel, the stored line mates") {
  size_t branching = 0;
  for (const auto& p : shipped()) {
    if (p.goal != puzzles::Goal::MateIn1) continue;
    INFO(p.id);
    const puzzles::Report r = puzzles::validate(p);
    for (const auto& e : r.errors) MESSAGE(e);
    CHECK(r.ok);
    CHECK(r.winningFirstTurns >= 1);
    if (p.tier == 2) {
      CHECK(r.timelines >= 2);
      CHECK(r.travelWins == r.winningFirstTurns);
    }
    if (p.tier == 3 && r.branchingWins == r.winningFirstTurns) ++branching;
  }
  CHECK(branching >= 2); // the "branching" mates: every mating turn creates a timeline
}

TEST_CASE("puzzle: any mating turn counts, not only the stored one") {
  const puzzles::Puzzle p = puzzles::parse("promotion", "5dchess-position 1\ntitle: P\nsize: 8\nrules: double-step\nto-move: white\n"
                                                         "goal: mate-in-1\ndifficulty: 1\nhint: h\nsolution: (L0T1)c7>(L0T1)c8=Q\n"
                                                         "L0 T1w: k7/2P5/1K6/8/8/8/8/8\n");
  const auto game = p.start();
  const auto mates = puzzles::matingTurns(*game);
  CHECK(mates.size() >= 2); // c8=Q and c8=R (and what else the engine finds)
  for (const auto& turn : mates) {
    const auto after = puzzles::submitted(*game, turn);
    REQUIRE(after);
    CHECK(after->result() == GameResult::WhiteWins);
  }
  // a turn that does not mate is not accepted: Kb6-a6 is stalemate
  const auto stale = puzzles::submitted(*game, {Core::Move{{1, 5, 0, 0}, {0, 5, 0, 0}}});
  REQUIRE(stale);
  CHECK(stale->result() != GameResult::WhiteWins);
}

TEST_CASE("puzzle: the turn enumeration is exact (pruned and unpruned agree, and agree with the mate proof)") {
  for (const char* id : {"t1-01-back-rank", "t1-05-promotion", "t2-02-bishop-in-time"}) {
    shipped();
    const puzzles::Puzzle* p = puzzles::find(id);
    REQUIRE(p);
    INFO(id);
    const auto game = p->start();
    size_t pruned = 0, full = 0, matesPruned = 0, matesProof = 0;
    puzzles::forEachTurn(*game, [&](const puzzles::Turn&, const IGame& pending) {
      ++pruned;
      matesPruned += puzzles::mates(pending);
      return true;
    });
    puzzles::forEachTurn(*game, [&](const puzzles::Turn& turn, const IGame&) {
      ++full;
      const auto after = puzzles::submitted(*game, turn); // no shortcut: the full legal-turn proof
      REQUIRE(after);
      matesProof += after->result() == GameResult::WhiteWins;
      return true;
    }, 50'000'000, false);
    CHECK(pruned == full);
    CHECK(matesPruned == matesProof);
  }
}

TEST_CASE("puzzle: a mate in 2 is proven for the stored first turn and refuted for another") {
  shipped();
  const puzzles::Puzzle* p = puzzles::find("t3-03-small-step");
  REQUIRE(p);
  const puzzles::Report r = puzzles::validate(*p);
  for (const auto& e : r.errors) MESSAGE(e);
  CHECK(r.ok);
  CHECK(r.defences >= 2); // the engine's reply is a real choice

  const auto game = p->start();
  size_t refuted = 0, proven = 0;
  puzzles::forEachTurn(*game, [&](const puzzles::Turn& turn, const IGame&) {
    const auto after = puzzles::submitted(*game, turn);
    REQUIRE(after);
    if (after->result() != GameResult::Ongoing) return true;
    puzzles::DefenceProver prover(*after);
    while (prover.step(1000.0) == puzzles::DefenceProver::Status::Running) {}
    if (prover.status() == puzzles::DefenceProver::Status::Proven) ++proven;
    else {
      ++refuted;
      CHECK_FALSE(prover.refutation().empty());
    }
    return true;
  });
  CHECK(proven == r.winningFirstTurns);
  CHECK(refuted > 10);
}
