#pragma once
#include <set>
#include <string>

// Which puzzles the player has solved. The text form uses the settings file format (services/SettingsFile.h):
//
//   # 5DChess settings
//   solved=t1-01-back-rank,t1-02-ladder
//
// Desktop: puzzles.txt next to settings.txt; web: localStorage "5dchess.puzzles"; the UI test harness keeps it in memory
// only (src/puzzles/ProgressStore.cpp). The class itself does no I/O, so it is unit tested.
namespace puzzles {

class Progress {
public:
  bool solved(const std::string& id) const { return _solved.count(id) != 0; }
  /// Marks a puzzle solved; returns whether that was news (an unchanged set needs no save). Ignores ids that are not file-name safe.
  bool markSolved(const std::string& id);
  size_t count() const { return _solved.size(); }
  void clear() { _solved.clear(); }

  std::string toText() const;
  /// Reads what toText wrote; anything malformed (unknown keys, odd ids, too many) is skipped, never an error.
  static Progress fromText(const std::string& text);

  bool operator==(const Progress& o) const { return _solved == o._solved; }

  /// A plain word: lower-case letters, digits, '-' and '_', 1..64 characters.
  static bool validId(const std::string& id);

private:
  std::set<std::string> _solved;
};

/// The player's progress, loaded from the platform store on first use. markSolved() through this saves at once.
Progress& progress();
void markSolved(const std::string& id);
/// Wipes the in-memory progress and the stored one (developer tools, tests of the screens).
void resetProgress();

} // namespace puzzles
