#pragma once
#include <memory>
#include <string>
#include <string_view>
#include <vector>
#include "chess.h"
#include "engine/Position.h"

// Puzzles: a position file (assets/puzzles/*.5dp, see docs/PUZZLES.md) plus metadata. Nothing here draws anything or needs
// a window, so the loader, the catalog and the proof (puzzles/Solver.h) are unit tested and shared with tools/puzzle_check.
namespace puzzles {

enum class Goal { MateIn1, MateIn2 };

/// The moves of one turn, in the order they are played.
using Turn = std::vector<Chess::Core::Move>;

constexpr int kTiers = 3;
/// "Warm-up", "Time travel", "Deep" (1-based; anything else gives "").
const char* tierName(int tier);

struct Puzzle {
  std::string id;    ///< the file name without extension, e.g. "t1-02-back-rank"; stable (progress is stored by id)
  std::string title;
  std::string hint;
  Goal goal = Goal::MateIn1;
  int tier = 1;      ///< 1..kTiers (the `difficulty:` line)
  /// The stored line: one turn (a mating turn) for a mate in 1; White, Black's reply, White's mating turn for a mate in 2.
  std::vector<Turn> solution;
  Chess::Core::Position position;

  int mateIn() const { return goal == Goal::MateIn1 ? 1 : 2; }
  /// "Mate in 1"
  std::string goalText() const;
  /// "White to move - mate in 1"
  std::string banner() const;
  /// A fresh game of the starting position.
  std::shared_ptr<Chess::IGame> start() const;
};

/// Parses puzzle text: the position format of docs/POSITIONS.md plus the header lines `goal`, `hint`, `difficulty` and
/// `solution`. Throws Chess::Core::ParseError ("line N: ..."). Only the syntax of the solution is checked here (every move
/// parses, the number of turns matches the goal); whether it is legal and wins is puzzles::validate()'s job.
///
/// `strict = false` (the authoring tool's --mates): the puzzle lines may be missing, so a bare position can be examined.
Puzzle parse(const std::string& id, std::string_view text, bool strict = true);

/// Where the puzzle files are looked up (default "assets/puzzles", relative to the working directory). Forgets the loaded list.
void setDirectory(std::string directory);
const std::string& directory();

/// Every puzzle of the directory, ordered by (tier, id); files that fail to load are reported on stderr and left out. Loaded once.
const std::vector<Puzzle>& all();
const Puzzle* find(const std::string& id);

} // namespace puzzles
