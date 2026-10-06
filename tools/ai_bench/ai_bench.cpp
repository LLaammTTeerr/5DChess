// ai_bench: AI speed and self-play (Release build, -DFDCHESS_BUILD_AIBENCH=ON; see docs/AI.md).
//
//   ai_bench speed [--mode ID]                      time per decision of each level on an opening and a midgame position
//   ai_bench big [--mode ID] [--games N]            node cap check on big multiverses: plays cheap turns until a game has many timelines
//                                                   / mandatory boards, then every level decides there (nodes vs its cap, time)
//   ai_bench file POSITION.5dp                      one decision of every level on a position file
//   ai_bench selfplay [--games N] [--turns T] [--a easy|normal|hard] [--b ...] [--mode ID]
//                                                   A vs B over N games (colours alternate, modes cycle unless --mode),
//                                                   at most T turns each; unfinished games are adjudicated by material
#include "ai/Eval.h"
#include "ai/Play.h"
#include "engine/GameCatalog.h"
#include "engine/Notation.h"
#include "engine/Position.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>

using namespace Chess;
using Clock = std::chrono::steady_clock;

namespace {

double secondsSince(Clock::time_point t0) { return std::chrono::duration<double>(Clock::now() - t0).count(); }

ai::Level parseLevel(const std::string& s) {
  if (s == "easy") return ai::Level::Easy;
  if (s == "hard") return ai::Level::Hard;
  return ai::Level::Normal;
}

const char* levelName(ai::Level l) { return l == ai::Level::Easy ? "easy" : l == ai::Level::Hard ? "hard" : "normal"; }

void speed(const std::string& mode) {
  GameCatalog::setDirectory(FDCHESS_POSITIONS_DIR);
  struct Pos { std::string name; std::shared_ptr<IGame> game; };
  std::vector<Pos> positions;
  positions.push_back({mode + " opening", GameCatalog::create(mode)});
  {
    auto g = GameCatalog::create(mode);
    for (int t = 0; t < 12 and g->result() == GameResult::Ongoing; ++t) ai::playTurn(*g, {ai::Level::Easy, std::uint64_t(100 + t)});
    positions.push_back({mode + " after 12 turns", g});
  }
  std::printf("%-26s %-7s %10s %12s %12s %10s\n", "position", "level", "nodes", "ms/decision", "nodes/s", "max step ms");
  for (const Pos& p : positions) {
    for (ai::Level level : {ai::Level::Easy, ai::Level::Normal, ai::Level::Hard}) {
      double total = 0, maxStep = 0;
      long long nodes = 0;
      const int reps = 5;
      for (int r = 0; r < reps; ++r) {
        ai::Search s(*p.game, {level, std::uint64_t(r + 1)});
        const auto t0 = Clock::now();
        for (;;) {
          const auto ts = Clock::now();
          const bool running = s.step(500) == ai::Search::Status::Running;
          maxStep = std::max(maxStep, secondsSince(ts) * 1000);
          if (!running) break;
        }
        total += secondsSince(t0);
        nodes += s.progress().nodes;
      }
      std::printf("%-26s %-7s %10lld %12.1f %12.0f %10.1f\n", p.name.c_str(), levelName(level), nodes / reps, total / reps * 1000,
                  double(nodes) / total, maxStep);
    }
  }
}

void big(const std::string& mode, int games) {
  GameCatalog::setDirectory(FDCHESS_POSITIONS_DIR);
  std::printf("%-5s %-7s %5s %5s %10s %10s %8s %10s\n", "game", "level", "tl", "mand", "nodes", "cap", "x cap", "ms");
  for (int g = 0; g < games; ++g) {
    auto game = GameCatalog::create(mode);
    for (int t = 0; t < 80 and game->result() == GameResult::Ongoing and game->timeLineCount() < 15; ++t) {
      if (!ai::playTurn(*game, {ai::Level::Normal, std::uint64_t(g * 977 + t + 1), 1500})) break;
    }
    if (game->result() != GameResult::Ongoing) continue;
    if (std::getenv("AI_DUMP") and std::atoi(std::getenv("AI_DUMP")) == g) std::fputs(Core::writePosition(Core::Position::fromGame(*game)).c_str(), stderr);
    for (ai::Level level : {ai::Level::Easy, ai::Level::Normal, ai::Level::Hard}) {
      ai::Search s(*game, {level, 1});
      const auto t0 = Clock::now();
      while (s.step(500) == ai::Search::Status::Running) {}
      const double ms = secondsSince(t0) * 1000;
      const long long cap = level == ai::Level::Easy ? 10000 : level == ai::Level::Normal ? 300000 : 1200000;
      std::printf("%-5d %-7s %5d %5zu %10lld %10lld %8.2f %10.0f\n", g, levelName(level), game->timeLineCount(), game->mandatoryBoards().size(),
                  s.progress().nodes, cap, double(s.progress().nodes) / double(cap), ms);
    }
    std::fflush(stdout);
  }
}

/// One decision of every level on a position file (see docs/POSITIONS.md): nodes, time, whether the fallback was needed.
void decide(const std::string& path) {
  std::ifstream in(path);
  std::stringstream text;
  text << in.rdbuf();
  const auto game = Core::parsePosition(text.str()).makeGame();
  for (ai::Level level : {ai::Level::Easy, ai::Level::Normal, ai::Level::Hard}) {
    ai::Search s(*game, {level, 1});
    const auto t0 = Clock::now();
    while (s.step(500) == ai::Search::Status::Running) {}
    std::printf("%-7s nodes %9lld  %8.0f ms  depth %d  score %d  mate %d  fallback %d  turn %zu moves\n", levelName(level), s.progress().nodes,
                secondsSince(t0) * 1000, s.progress().depth, s.progress().bestScore, s.progress().mateFound, s.progress().usedFallback,
                s.bestTurn().size());
  }
}

void selfplay(int games, int maxTurns, ai::Level a, ai::Level b, const std::string& onlyMode, int onlyGame) {
  GameCatalog::setDirectory(FDCHESS_POSITIONS_DIR);
  const auto& modes = GameCatalog::modes();
  double score = 0;
  int wins = 0, draws = 0, losses = 0, byMate = 0;
  const auto t0 = Clock::now();
  for (int g = 0; g < games; ++g) {
    if (onlyGame >= 0 and g != onlyGame) continue;
    const std::string mode = onlyMode.empty() ? modes[std::size_t(g / 2) % modes.size()].id : onlyMode;
    const bool aWhite = g % 2 == 0;
    auto game = GameCatalog::create(mode);
    int turn = 0;
    for (; turn < maxTurns and game->result() == GameResult::Ongoing; ++turn) {
      const bool whiteToMove = game->getCurrentTurnColor() == PieceColor::PIECEWHITE;
      const ai::Level level = whiteToMove == aWhite ? a : b;
      if (!ai::playTurn(*game, {level, std::uint64_t(g * 1000 + turn + 1)})) break;
      if (onlyGame >= 0) {
        std::string line = std::string(whiteToMove ? "w " : "b ") + levelName(level) + ":";
        for (const auto& pm : game->history().back().moves) line += " " + toNotation(pm.move, pm.promotes);
        std::printf("T%d %s   eval(white)=%d\n", turn + 1, line.c_str(), ai::evaluate(*game, PieceColor::PIECEWHITE));
      }
    }
    double s = 0.5;
    const char* how = "adjudicated";
    switch (game->result()) {
      case GameResult::WhiteWins: s = aWhite ? 1 : 0; how = "mate"; ++byMate; break;
      case GameResult::BlackWins: s = aWhite ? 0 : 1; how = "mate"; ++byMate; break;
      case GameResult::Draw: s = 0.5; how = "stalemate"; break;
      case GameResult::Ongoing: {
        const int e = ai::evaluate(*game, aWhite ? PieceColor::PIECEWHITE : PieceColor::PIECEBLACK);
        if (e > 300) s = 1;
        else if (e < -300) s = 0;
        break;
      }
    }
    (s == 1 ? wins : s == 0 ? losses : draws)++;
    score += s;
    std::printf("game %2d %-18s A=%s turns=%d %-11s -> %.1f\n", g, mode.c_str(), aWhite ? "white" : "black", turn, how, s);
    std::fflush(stdout);
  }
  std::printf("%s vs %s: score %.1f / %d (wins %d, draws %d, losses %d, %d by mate) in %.1f s\n", levelName(a), levelName(b), score, games,
              wins, draws, losses, byMate, secondsSince(t0));
}

} // namespace

int main(int argc, char** argv) {
  const std::string cmd = argc > 1 ? argv[1] : "speed";
  int games = 20, turns = 30, only = -1;
  std::string mode, a = "hard", b = "easy";
  for (int i = 2; i < argc; ++i) {
    const std::string arg = argv[i];
    if (arg == "--games" and i + 1 < argc) games = std::atoi(argv[++i]);
    else if (arg == "--turns" and i + 1 < argc) turns = std::atoi(argv[++i]);
    else if (arg == "--mode" and i + 1 < argc) mode = argv[++i];
    else if (arg == "--game" and i + 1 < argc) only = std::atoi(argv[++i]);
    else if (arg == "--a" and i + 1 < argc) a = argv[++i];
    else if (arg == "--b" and i + 1 < argc) b = argv[++i];
  }
  if (cmd == "speed") speed(mode.empty() ? "standard" : mode);
  else if (cmd == "big") big(mode.empty() ? "timeline-invasion" : mode, games);
  else if (cmd == "file" and argc > 2) decide(argv[2]);
  else if (cmd == "selfplay") selfplay(games, turns, parseLevel(a), parseLevel(b), mode, only);
  else {
    std::fprintf(stderr, "usage: ai_bench speed|big|selfplay [options]\n");
    return 1;
  }
  return 0;
}
