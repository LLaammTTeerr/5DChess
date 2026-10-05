// turnbench: how long does it take to decide, turn after turn, whether the side to move has a legal turn?
//
//   turnbench [--games N] [--turns T] [--frame-nodes F]
//
// 1. Random legal games over all nine modes: after every submitted turn the legal-turn search is stepped to the end;
//    reports the total time per decision, the slowest decision and the slowest single step(F) call.
// 2. Constructed mates on 1..4 identical 8x8 boards (Black king h8 boxed in by pawns, White rook a8 giving check), without
//    and with "bystander" pawns that have many harmless moves: the time and node count to prove that no legal turn exists.
// Build with -DFDCHESS_BUILD_REFCHECK=ON in a Release configuration.
#include "chess.h"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <cstdio>
#include <random>
#include <string>
#include <vector>

using namespace Chess;
using Clock = std::chrono::steady_clock;

namespace {

double msSince(Clock::time_point t0) { return std::chrono::duration<double, std::milli>(Clock::now() - t0).count(); }

class Mate : public IGame {
public:
  Mate(int n, int boards, bool bystanders) : IGame(n) {
    for (int id = 0; id < boards; ++id) {
      auto line = _addTimeLine(std::make_shared<TimeLine>(n, id));
      for (int h = 0; h < 2; ++h) line->pushBack(std::make_shared<Board>(n, id, h));
    }
    _presentHalfTurn = 1;
    _currentTurnColor = PieceColor::PIECEBLACK;
    for (int id = 0; id < boards; ++id) {
      auto b = timeLine(id)->back();
      b->placePiece({7, 7}, std::make_shared<King>(PieceColor::PIECEBLACK));
      b->placePiece({6, 6}, std::make_shared<Pawn>(PieceColor::PIECEBLACK));
      b->placePiece({7, 6}, std::make_shared<Pawn>(PieceColor::PIECEBLACK));
      if (bystanders) {
        for (int x = 1; x <= 5; ++x) b->placePiece({x, 2}, std::make_shared<Pawn>(PieceColor::PIECEBLACK));
      }
      b->placePiece({0, 7}, std::make_shared<Rook>(PieceColor::PIECEWHITE));
      b->placePiece({0, 0}, std::make_shared<King>(PieceColor::PIECEWHITE));
    }
  }
};

bool buildTurn(IGame& game, std::mt19937& rng) {
  auto uni = [&](int lo, int hi) { return std::uniform_int_distribution<int>(lo, hi)(rng); };
  for (int attempt = 0; attempt < 12; ++attempt) {
    for (int step = 0; step < 10; ++step) {
      if (game.canSubmit() and uni(0, 2) != 0) return true;
      auto moves = game.allPseudoLegalMoves();
      if (moves.empty()) break;
      game.makeMove(moves[std::size_t(uni(0, int(moves.size()) - 1))]);
    }
    if (game.canSubmit()) return true;
    while (game.undoable()) game.undo();
  }
  return false;
}

struct Stats {
  std::vector<double> perTurn;
  double worstStep = 0;
  long long nodes = 0;
  int decided = 0;
  int unresolved = 0;
};

template <class G>
void play(const char* name, int games, int turns, int frameNodes, Stats& stats) {
  for (int seed = 1; seed <= games; ++seed) {
    std::mt19937 rng(unsigned(seed) * 7919u);
    G game;
    for (int t = 0; t < turns and game.result() == GameResult::Ongoing; ++t) {
      if (!buildTurn(game, rng)) break;
      const auto t0 = Clock::now();
      game.submitTurn();
      bool pending = game.resultPending();
      while (pending) {
        const auto s0 = Clock::now();
        pending = game.stepResultSearch(frameNodes);
        stats.worstStep = std::max(stats.worstStep, msSince(s0));
        if (msSince(t0) > 60000) { ++stats.unresolved; break; }
      }
      stats.perTurn.push_back(msSince(t0));
      if (std::getenv("TURNBENCH_SLOW") and msSince(t0) > 50) {
        std::printf("  slow: %s seed %d turn %d: %.1f ms, %d timelines, %zu mandatory, result %d\n", name, seed, t, msSince(t0),
                    game.timeLineCount(), game.mandatoryBoards().size(), int(game.result()));
      }
      if (game.result() != GameResult::Ongoing) ++stats.decided;
    }
  }
}

} // namespace

int main(int argc, char** argv) {
  int games = 6, turns = 25, frameNodes = 5000;
  for (int i = 1; i < argc; ++i) {
    const std::string a = argv[i];
    if (a == "--games" && i + 1 < argc) games = std::stoi(argv[++i]);
    else if (a == "--turns" && i + 1 < argc) turns = std::stoi(argv[++i]);
    else if (a == "--frame-nodes" && i + 1 < argc) frameNodes = std::stoi(argv[++i]);
  }
  Stats st;
  play<StandardGame>("standard", games, turns, frameNodes, st);
  play<CustomGameEmitBishop>("emit_bishop", games, turns, frameNodes, st);
  play<CustomGameEmitKnight>("emit_knight", games, turns, frameNodes, st);
  play<CustomGameEmitQueen>("emit_queen", games, turns, frameNodes, st);
  play<CustomGameEmitRook>("emit_rook", games, turns, frameNodes, st);
  play<CustomGameKVB>("kvb", games, turns, frameNodes, st);
  play<MiscGameTimeLineInvasion>("invasion", games, turns, frameNodes, st);
  play<MiscGameTimeLineBattle>("battle", games, turns, frameNodes, st);
  play<MiscGameTimeLineFragment>("fragment", games, turns, frameNodes, st);
  std::sort(st.perTurn.begin(), st.perTurn.end());
  const auto pct = [&](double p) { return st.perTurn[std::min(st.perTurn.size() - 1, std::size_t(double(st.perTurn.size()) * p))]; };
  std::printf("random games: %zu turns decided, %d ended in a win/draw, %d unresolved after 60 s\n", st.perTurn.size(), st.decided, st.unresolved);
  std::printf("  per turn: p50 %.4f ms  p99 %.3f ms  max %.3f ms;  slowest step(%d): %.3f ms\n", pct(0.5), pct(0.99), st.perTurn.back(),
              frameNodes, st.worstStep);

  for (int bystanders = 0; bystanders < 2; ++bystanders) {
    for (int boards = 1; boards <= 4; ++boards) {
      Mate mate(8, boards, bystanders != 0);
      TurnSearch search(mate);
      const auto t0 = Clock::now();
      TurnSearch::Status s = TurnSearch::Status::Running;
      double worst = 0;
      while (s == TurnSearch::Status::Running) {
        const auto s0 = Clock::now();
        s = search.step(frameNodes);
        worst = std::max(worst, msSince(s0));
        if (msSince(t0) > 120000) break;
      }
      std::printf("mate, %d board(s), bystanders=%d: %s in %.2f ms, %lld nodes (%.2f us/node), slowest step(%d) %.3f ms\n", boards,
                  bystanders, s == TurnSearch::Status::None ? "None (mate proven)" : s == TurnSearch::Status::Found ? "Found" : "RUNNING",
                  msSince(t0), search.nodes(), msSince(t0) * 1000.0 / double(std::max<long long>(1, search.nodes())), frameNodes, worst);
    }
  }
  return 0;
}
