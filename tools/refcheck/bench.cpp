// bench: engine micro-benchmark (Release build, -DFDCHESS_BUILD_REFCHECK=ON).
//
//   bench [--games N] [--reps R]
//
// 1. Turn search throughput: the constructed mates of turnbench (1..4 boards, with and without bystander pawns) are proven
//    R times; reports nodes per second.
// 2. Random games: N seeded random legal games (default 1000, at most 30 turns each) spread over every catalog mode;
//    reports the total time and a checksum of all moves played (equal checksums = the same games were played, so two
//    builds can be compared like for like).
#include "chess.h"
#include "engine/GameCatalog.h"
#include "engine/Position.h"

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <random>
#include <string>

using namespace Chess;
using Clock = std::chrono::steady_clock;

namespace {

double secondsSince(Clock::time_point t0) { return std::chrono::duration<double>(Clock::now() - t0).count(); }

// The mate of turnbench as a position file: Black king h8 boxed in by two pawns, White rook a8 giving check.
std::string mateText(int boards, bool bystanders) {
  std::string text = "5dchess-position 1\nsize: 8\nrules: double-step castling\nto-move: black\n";
  const char* rows = bystanders ? "R6k/6pp/8/8/8/1ppppp2/8/K7" : "R6k/6pp/8/8/8/8/8/K7";
  for (int id = 0; id < boards; ++id) {
    const std::string l = "L" + std::to_string(id);
    text += l + " T1w: " + rows + "\n" + l + " T1b: " + rows + "\n";
  }
  return text;
}

bool buildTurn(IGame& game, std::mt19937& rng, std::uint64_t& checksum) {
  auto uni = [&](int lo, int hi) { return int(lo + int(rng() % std::uint32_t(hi - lo + 1))); };
  for (int attempt = 0; attempt < 12; ++attempt) {
    for (int step = 0; step < 10; ++step) {
      if (game.canSubmit() and uni(0, 2) != 0) return true;
      auto moves = game.allPseudoLegalMoves();
      if (moves.empty()) break;
      const Move m = moves[std::size_t(uni(0, int(moves.size()) - 1))];
      const Core::Coord a = m.from.coord(), b = m.to.coord();
      for (int v : std::initializer_list<int>{a.x, a.y, a.t, a.l, b.x, b.y, b.t, b.l}) checksum = checksum * 1099511628211ull + std::uint64_t(v + 1000);
      game.makeMove(m);
    }
    if (game.canSubmit()) return true;
    while (game.undoable()) game.undo();
  }
  return false;
}

} // namespace

int main(int argc, char** argv) {
  int games = 1000, reps = 20000;
  for (int i = 1; i < argc; ++i) {
    const std::string a = argv[i];
    if (a == "--games" and i + 1 < argc) games = std::stoi(argv[++i]);
    else if (a == "--reps" and i + 1 < argc) reps = std::stoi(argv[++i]);
  }
  GameCatalog::setDirectory(FDCHESS_POSITIONS_DIR);

  long long nodes = 0;
  double searchSeconds = 0;
  for (int rep = 0; rep < reps; ++rep) {
    for (int bystanders = 0; bystanders < 2; ++bystanders) {
      for (int boards = 1; boards <= 4; ++boards) {
        auto game = Core::parsePosition(mateText(boards, bystanders != 0)).makeGame();
        const auto t0 = Clock::now();
        TurnSearch search(*game);
        while (search.step(5000) == TurnSearch::Status::Running) {}
        searchSeconds += secondsSince(t0);
        nodes += search.nodes();
      }
    }
  }
  std::printf("turn search: %lld nodes in %.3f s = %.0f nodes/s\n", nodes, searchSeconds, double(nodes) / searchSeconds);

  const auto& modes = GameCatalog::modes();
  std::uint64_t checksum = 1469598103934665603ull;
  long long turns = 0;
  const auto t0 = Clock::now();
  for (int g = 0; g < games; ++g) {
    std::mt19937 rng(unsigned(g) * 7919u + 1);
    auto game = GameCatalog::create(modes[std::size_t(g) % modes.size()].id);
    for (int t = 0; t < 30 and game->result() == GameResult::Ongoing; ++t) {
      if (!buildTurn(*game, rng, checksum)) break;
      game->submitTurn();
      game->resolveResult(50000);
      ++turns;
    }
    checksum = checksum * 31 + std::uint64_t(game->result());
  }
  const double s = secondsSince(t0);
  std::printf("random games: %d games, %lld turns in %.3f s (checksum %016llx)\n", games, turns, s, (unsigned long long)checksum);
  return 0;
}
