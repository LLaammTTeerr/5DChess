#include "puzzles/Puzzle.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include "engine/Notation.h"

namespace puzzles {

namespace {

using Chess::Core::ParseError;

std::string g_directory = "assets/puzzles";
std::optional<std::vector<Puzzle>> g_all;

std::string_view trim(std::string_view s) {
  while (!s.empty() && (s.front() == ' ' || s.front() == '\t' || s.front() == '\r')) s.remove_prefix(1);
  while (!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\r')) s.remove_suffix(1);
  return s;
}

[[noreturn]] void fail(int line, const std::string& what) { throw ParseError("line " + std::to_string(line) + ": " + what); }

constexpr size_t kMaxHint = 200, kMaxTitle = 60, kMaxValue = 2000;

Turn parseTurn(std::string_view text, Chess::PieceColor mover, int line) {
  Turn turn;
  std::istringstream in{std::string(text)};
  for (std::string word; in >> word;) turn.push_back(Chess::parseMove(word, mover));
  if (turn.empty()) fail(line, "solution: a turn without moves");
  return turn;
}

} // namespace

const char* tierName(int tier) {
  switch (tier) {
    case 1: return "Warm-up";
    case 2: return "Time travel";
    case 3: return "Deep";
    default: return "";
  }
}

std::string Puzzle::goalText() const { return mateIn() == 1 ? "Mate in 1" : "Mate in 2"; }

std::string Puzzle::banner() const { return "White to move - mate in " + std::to_string(mateIn()); }

std::shared_ptr<Chess::IGame> Puzzle::start() const { return position.makeGame(); }

Puzzle parse(const std::string& id, std::string_view text, bool strict) {
  Puzzle puzzle;
  puzzle.id = id;
  std::string rest; // the text without the puzzle's own lines (kept as blank lines, so the line numbers of errors stay true)
  std::string solutionText;
  int solutionLine = 0;
  bool sawGoal = false, sawTier = false, sawSolution = false, sawBoard = false;
  int lineNo = 0;
  std::string_view in = text;
  while (!in.empty()) {
    const size_t nl = in.find('\n');
    const std::string_view raw = in.substr(0, nl);
    in = nl == std::string_view::npos ? std::string_view() : in.substr(nl + 1);
    ++lineNo;
    const std::string_view line = trim(raw);
    const size_t colon = line.find(':');
    const std::string_view key = colon == std::string_view::npos ? std::string_view() : trim(line.substr(0, colon));
    const std::string_view value = colon == std::string_view::npos ? std::string_view() : trim(line.substr(colon + 1));
    if (!line.empty() && line.front() == 'L' && key.find(' ') != std::string_view::npos) sawBoard = true;
    const bool ours = !sawBoard && (key == "goal" || key == "hint" || key == "difficulty" || key == "solution");
    if (!ours) {
      rest.append(raw);
      rest.push_back('\n');
      continue;
    }
    rest.push_back('\n');
    if (value.size() > kMaxValue) fail(lineNo, std::string(key) + ": too long");
    if (key == "goal") {
      if (sawGoal) fail(lineNo, "duplicate 'goal'");
      sawGoal = true;
      if (value == "mate-in-1") puzzle.goal = Goal::MateIn1;
      else if (value == "mate-in-2") puzzle.goal = Goal::MateIn2;
      else fail(lineNo, "goal must be mate-in-1 or mate-in-2");
    } else if (key == "hint") {
      if (value.size() > kMaxHint) fail(lineNo, "hint is longer than " + std::to_string(kMaxHint) + " characters");
      puzzle.hint = std::string(value);
    } else if (key == "difficulty") {
      if (sawTier) fail(lineNo, "duplicate 'difficulty'");
      sawTier = true;
      if (value.size() != 1 || value[0] < '1' || value[0] > '0' + kTiers)
        fail(lineNo, "difficulty must be 1.." + std::to_string(kTiers));
      puzzle.tier = value[0] - '0';
    } else {
      if (sawSolution) fail(lineNo, "duplicate 'solution'");
      sawSolution = true;
      solutionText = std::string(value);
      solutionLine = lineNo;
    }
  }
  if (strict) {
    if (!sawGoal) fail(lineNo, "missing 'goal'");
    if (!sawTier) fail(lineNo, "missing 'difficulty'");
    if (!sawSolution) fail(lineNo, "missing 'solution'");
  }

  puzzle.position = Chess::Core::parsePosition(rest);
  puzzle.title = puzzle.position.title;
  if (strict && puzzle.title.empty()) fail(lineNo, "missing 'title'");
  if (puzzle.title.size() > kMaxTitle) fail(lineNo, "title is longer than " + std::to_string(kMaxTitle) + " characters");
  if (strict && puzzle.position.toMove != Chess::PieceColor::PIECEWHITE) fail(lineNo, "a puzzle starts with White to move");

  if (!sawSolution) return puzzle; // only without `strict`
  // solution: <turn> for a mate in 1, <turn> / <turn> / <turn> for a mate in 2; a turn is its moves separated by spaces
  std::vector<std::string_view> parts;
  for (std::string_view s = solutionText;;) {
    const size_t slash = s.find('/');
    parts.push_back(trim(s.substr(0, slash)));
    if (slash == std::string_view::npos) break;
    s = s.substr(slash + 1);
  }
  const size_t want = puzzle.goal == Goal::MateIn1 ? 1 : 3;
  if (parts.size() != want)
    fail(solutionLine, "solution needs " + std::to_string(want) + " turn(s) separated by '/' for " + puzzle.goalText());
  for (size_t i = 0; i < parts.size(); ++i)
    puzzle.solution.push_back(
        parseTurn(parts[i], i == 1 ? Chess::PieceColor::PIECEBLACK : Chess::PieceColor::PIECEWHITE, solutionLine));
  return puzzle;
}

void setDirectory(std::string directory) {
  g_directory = std::move(directory);
  g_all.reset();
}

const std::string& directory() { return g_directory; }

const std::vector<Puzzle>& all() {
  if (!g_all) {
    std::vector<Puzzle> list;
    namespace fs = std::filesystem;
    std::error_code ec;
    std::vector<fs::path> files;
    for (const auto& entry : fs::directory_iterator(g_directory, ec))
      if (entry.is_regular_file(ec) && entry.path().extension() == ".5dp") files.push_back(entry.path());
    std::sort(files.begin(), files.end());
    for (const fs::path& file : files) {
      try {
        std::ifstream in(file, std::ios::binary);
        std::ostringstream text;
        text << in.rdbuf();
        list.push_back(parse(file.stem().string(), text.str()));
      } catch (const std::exception& e) {
        std::cerr << "puzzle '" << file.filename().string() << "' unavailable: " << e.what() << std::endl;
      }
    }
    std::stable_sort(list.begin(), list.end(), [](const Puzzle& a, const Puzzle& b) { return a.tier < b.tier; });
    g_all = std::move(list);
  }
  return *g_all;
}

const Puzzle* find(const std::string& id) {
  for (const Puzzle& p : all())
    if (p.id == id) return &p;
  return nullptr;
}

} // namespace puzzles
