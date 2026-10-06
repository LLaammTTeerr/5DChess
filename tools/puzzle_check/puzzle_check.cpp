// puzzle_check: the author's and CI's proof of the puzzles (docs/PUZZLES.md).
//
//   puzzle_check <dir|file.5dp>...      validate puzzles: legal solution, exhaustive proof, tier rules; exit 1 on any failure
//   puzzle_check -v <dir|file>...       also list every winning first turn
//   puzzle_check --mates <file.5dp>     authoring: every mating turn of a position (the puzzle lines are optional)
//   puzzle_check --mate2 <file.5dp>     authoring: every first turn that wins by force in two, with Black's defences
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include "engine/Notation.h"
#include "puzzles/Puzzle.h"
#include "puzzles/Solver.h"

namespace fs = std::filesystem;

namespace {

std::string slurp(const fs::path& path) {
  std::ifstream in(path, std::ios::binary);
  if (!in) throw std::runtime_error("cannot read " + path.string());
  std::ostringstream text;
  text << in.rdbuf();
  return text.str();
}

std::string text(const puzzles::Turn& turn) {
  std::string s;
  for (const auto& m : turn) s += (s.empty() ? "" : " ") + Chess::toNotation(m, m.promotion != Chess::PieceType::Queen);
  return s;
}

int validateFiles(const std::vector<fs::path>& files, bool verbose) {
  size_t failures = 0;
  for (const fs::path& file : files) {
    try {
      const puzzles::Puzzle puzzle = puzzles::parse(file.stem().string(), slurp(file));
      const puzzles::Report report = puzzles::validate(puzzle);
      std::cout << puzzles::describe(puzzle, report) << "\n";
      if (verbose)
        for (const auto& turn : report.winners) std::cout << "       wins: " << text(turn) << "\n";
      failures += !report.ok;
    } catch (const std::exception& e) {
      std::cout << "FAIL " << file.stem().string() << "\n       " << e.what() << "\n";
      ++failures;
    }
  }
  std::cout << (files.size() - failures) << " of " << files.size() << " puzzles valid\n";
  return failures ? 1 : 0;
}

int explore(const fs::path& file, bool mateIn2) {
  const puzzles::Puzzle puzzle = puzzles::parse(file.stem().string(), slurp(file), false);
  const auto game = puzzle.start();
  std::cout << puzzle.title << ": " << game->timeLineCount() << " timeline(s), " << game->getMoveableBoards().size()
            << " board(s) to move on\n";
  size_t turns = 0;
  if (!mateIn2) {
    std::vector<puzzles::Turn> mates;
    puzzles::forEachTurn(*game, [&](const puzzles::Turn& turn, const Chess::IGame& pending) {
      ++turns;
      if (puzzles::mates(pending)) mates.push_back(turn);
      return true;
    });
    std::cout << turns << " legal turns, " << mates.size() << " mating\n";
    for (const auto& turn : mates) std::cout << "  mate: " << text(turn) << "\n";
    return 0;
  }
  size_t mateInOne = 0;
  std::vector<puzzles::Turn> candidates;
  puzzles::forEachTurn(*game, [&](const puzzles::Turn& turn, const Chess::IGame& pending) {
    ++turns;
    auto g = pending.clone();
    g->submitTurn();
    g->resolveResult(50'000'000);
    if (g->result() == Chess::GameResult::WhiteWins) ++mateInOne;
    else if (g->result() == Chess::GameResult::Ongoing) candidates.push_back(turn);
    return true;
  });
  std::cout << turns << " legal turns, " << mateInOne << " mate in 1\n";
  size_t wins = 0;
  for (const auto& turn : candidates) {
    const auto after = puzzles::submitted(*game, turn);
    puzzles::DefenceProver prover(*after);
    while (prover.step(1e9) == puzzles::DefenceProver::Status::Running) {}
    if (prover.status() != puzzles::DefenceProver::Status::Proven) continue;
    ++wins;
    std::cout << "  wins: " << text(turn) << "   (" << prover.defenceCount() << " defences; e.g. " << text(prover.defences().front())
              << ")\n";
    // a complete line for the solution: header, e.g.   solution: <first> / <reply> / <mate>
    const auto reply = puzzles::submitted(*after, prover.defences().front());
    const auto mate = puzzles::findMate(*reply);
    std::cout << "        solution: " << text(turn) << " / " << text(prover.defences().front()) << " / " << (mate ? text(*mate) : "?") << "\n";
  }
  std::cout << wins << " first turns win by force\n";
  return 0;
}

} // namespace

int main(int argc, char** argv) {
  const std::vector<std::string> args(argv + 1, argv + argc);
  bool verbose = false;
  std::vector<fs::path> files;
  try {
    for (size_t i = 0; i < args.size(); ++i) {
      if (args[i] == "-v") {
        verbose = true;
      } else if (args[i] == "--mates" || args[i] == "--mate2") {
        if (i + 2 != args.size()) throw std::runtime_error(args[i] + " takes exactly one file");
        return explore(args[i + 1], args[i] == "--mate2");
      } else if (fs::is_directory(args[i])) {
        std::vector<fs::path> found;
        for (const auto& entry : fs::directory_iterator(args[i]))
          if (entry.path().extension() == ".5dp") found.push_back(entry.path());
        std::sort(found.begin(), found.end());
        files.insert(files.end(), found.begin(), found.end());
      } else {
        files.push_back(args[i]);
      }
    }
    if (files.empty()) {
      std::cerr << "usage: puzzle_check [-v] <dir|file.5dp>...\n       puzzle_check --mates|--mate2 <file.5dp>\n";
      return 2;
    }
    return validateFiles(files, verbose);
  } catch (const std::exception& e) {
    std::cerr << "puzzle_check: " << e.what() << "\n";
    return 2;
  }
}
