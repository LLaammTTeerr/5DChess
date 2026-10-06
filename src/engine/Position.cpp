#include "engine/Position.h"

#include <algorithm>
#include <charconv>
#include <climits>
#include <fstream>
#include <map>
#include <sstream>

namespace Chess::Core {

namespace {

constexpr int kMaxSize = Board::MAX_DIM;
// Bounds (see docs/POSITIONS.md): timeline ids must fit Core::Coord (int16) with room to spare, and the turn search sizes
// its arena by the id span and the number of boards, so both are capped far above anything a real game reaches.
constexpr int kMaxTimelineId = 1000;   // |id|; at most 2001 timelines
constexpr int kMaxHalfTurn = 20000;    // fits Coord::t (int16)
constexpr int kMaxBoards = 20000;      // across all timelines; at most 20000 * 64 bytes of search arena

[[noreturn]] void fail(int line, const std::string& what) {
  throw ParseError("line " + std::to_string(line) + ": " + what);
}

std::string_view trim(std::string_view s) {
  while (!s.empty() and (s.front() == ' ' or s.front() == '\t' or s.front() == '\r')) s.remove_prefix(1);
  while (!s.empty() and (s.back() == ' ' or s.back() == '\t' or s.back() == '\r')) s.remove_suffix(1);
  return s;
}

int parseInt(std::string_view s, int line, const char* what, int lo = INT_MIN, int hi = INT_MAX) {
  s = trim(s);
  int value = 0;
  const auto [end, ec] = std::from_chars(s.data(), s.data() + s.size(), value);
  if (s.empty() or ec == std::errc::invalid_argument or end != s.data() + s.size()) fail(line, std::string("bad number for ") + what);
  if (ec != std::errc() or value < lo or value > hi)
    fail(line, std::string(what) + " out of range " + std::to_string(lo) + ".." + std::to_string(hi));
  return value;
}

// ---- pieces ---------------------------------------------------------------------------------------------------------

char letterOf(PieceType type) {
  switch (type) {
    case PieceType::King: return 'k';
    case PieceType::Queen: return 'q';
    case PieceType::Rook: return 'r';
    case PieceType::Bishop: return 'b';
    case PieceType::Knight: return 'n';
    case PieceType::Pawn: return 'p';
  }
  return '?';
}

std::optional<PieceType> typeOf(char lower) {
  for (PieceType t : {PieceType::King, PieceType::Queen, PieceType::Rook, PieceType::Bishop, PieceType::Knight, PieceType::Pawn})
    if (letterOf(t) == lower) return t;
  return std::nullopt;
}

// ---- rows -----------------------------------------------------------------------------------------------------------

std::string writeBoard(const BoardData& board, int size) {
  std::string out;
  for (int y = size - 1; y >= 0; --y) {
    int empty = 0;
    for (int x = 0; x < size; ++x) {
      const auto& cell = board.cells[y * size + x];
      if (!cell) { ++empty; continue; }
      if (empty > 0) { out += std::to_string(empty); empty = 0; }
      char c = letterOf(cell->type);
      if (cell->color == PieceColor::PIECEWHITE) c = char(c - 'a' + 'A');
      out += c;
      if (cell->moved) out += '*';
    }
    if (empty > 0) out += std::to_string(empty);
    if (y > 0) out += '/';
  }
  return out;
}

BoardData parseBoard(std::string_view rows, int size, int halfTurn, int line) {
  BoardData board;
  board.halfTurn = halfTurn;
  board.cells.assign(size_t(size) * size, std::nullopt);
  int y = size - 1, x = 0;
  bool rowDone = false;
  for (size_t i = 0; i <= rows.size(); ++i) {
    const char c = i < rows.size() ? rows[i] : '/';
    if (c == '/') {
      if (x != size) fail(line, "row has " + std::to_string(x) + " squares, expected " + std::to_string(size));
      if (i == rows.size()) { rowDone = true; break; }
      if (--y < 0) fail(line, "too many rows");
      x = 0;
    } else if (c >= '0' and c <= '9') {
      int n = 0;
      while (i < rows.size() and rows[i] >= '0' and rows[i] <= '9') {
        n = n * 10 + (rows[i++] - '0');
        if (n > size) fail(line, "row is longer than " + std::to_string(size));
      }
      --i;
      x += n;
      if (x > size) fail(line, "row is longer than " + std::to_string(size));
    } else if (c == '*') {
      if (x == 0 or !board.cells[y * size + x - 1] or rows[i - 1] == '*') fail(line, "'*' must follow a piece");
      board.cells[y * size + x - 1]->moved = true;
    } else {
      const bool white = c >= 'A' and c <= 'Z';
      const auto type = typeOf(white ? char(c - 'A' + 'a') : c);
      if (!type) fail(line, std::string("unknown piece '") + c + "'");
      if (x >= size) fail(line, "row is longer than " + std::to_string(size));
      board.cells[y * size + x++] = PieceCell{*type, white ? PieceColor::PIECEWHITE : PieceColor::PIECEBLACK, false};
    }
  }
  if (!rowDone or y != 0) fail(line, "expected " + std::to_string(size) + " rows");
  return board;
}

// ---- game <-> position ----------------------------------------------------------------------------------------------

class LoadedGame : public IGame {
public:
  explicit LoadedGame(const Position& p) : IGame(p.size) {
    _rule.pawnCanMakeTwoMoveOnFirstTurn = p.doubleStep;
    _rule.castling = p.castling;
    _presentHalfTurn = p.present;
    _currentTurnColor = p.toMove;
    _startPosition = writePosition(p);
    // The timelines the game started with come first: they define the original ID range (see IGame::_addTimeLine).
    for (int pass = 0; pass < 2; ++pass) {
      for (const TimelineData& data : p.timelines) {
        if (data.parent.has_value() != (pass == 1)) continue;
        const int forkAt = data.boards.front().halfTurn - 1;
        auto line = _addTimeLine(std::make_shared<TimeLine>(p.size, data.id, forkAt, data.parent.value_or(TimeLine::NO_PARENT)));
        for (const BoardData& b : data.boards) line->pushBack(buildBoard(p.size, data.id, b));
      }
      if (pass == 0) _setupDone = true;
    }
  }

private:
  static std::shared_ptr<Board> buildBoard(int size, int timeline, const BoardData& data) {
    auto board = std::make_shared<Board>(size, timeline, data.halfTurn);
    for (int y = 0; y < size; ++y) {
      for (int x = 0; x < size; ++x) {
        const auto& cell = data.cells[y * size + x];
        if (!cell) continue;
        board->place(Position2D(x, y), Piece{cell->type, cell->color, !cell->moved});
      }
    }
    return board;
  }
};

} // namespace

Position Position::fromGame(const IGame& game, std::string title) {
  Position p;
  p.title = std::move(title);
  p.size = game.dim();
  p.doubleStep = game.rule().pawnCanMakeTwoMoveOnFirstTurn;
  p.castling = game.rule().castling;
  p.toMove = game.getCurrentTurnColor();
  p.present = game.presentHalfTurn();
  for (const auto& line : game.getTimeLines()) {
    TimelineData data;
    data.id = line->ID();
    if (line->hasParent()) data.parent = line->parentId();
    for (const auto& board : line->getBoards()) {
      BoardData b;
      b.halfTurn = board->halfTurnNumber();
      b.cells.resize(size_t(p.size) * p.size);
      for (int y = 0; y < p.size; ++y) {
        for (int x = 0; x < p.size; ++x) {
          if (const auto piece = board->at(Position2D(x, y)))
            b.cells[y * p.size + x] = PieceCell{piece->type, piece->color, !piece->unmoved};
        }
      }
      data.boards.push_back(std::move(b));
    }
    p.timelines.push_back(std::move(data));
  }
  return p;
}

std::shared_ptr<IGame> Position::makeGame() const {
  return std::make_shared<LoadedGame>(*this);
}

// ---- text format ----------------------------------------------------------------------------------------------------

namespace {

// The present is the lowest half-turn among the latest boards of the ACTIVE timelines; the activity rule is
// IGame::isTimeLineActive: the original timelines (those without a parent) are active, and the n-th timeline a player
// created (above the original ids: White, below: Black) is active iff the opponent has created at least n-1.
int defaultPresent(const Position& p) {
  int origMin = INT_MAX, origMax = INT_MIN, minId = INT_MAX, maxId = INT_MIN;
  for (const auto& t : p.timelines) {
    minId = std::min(minId, t.id);
    maxId = std::max(maxId, t.id);
    if (!t.parent) {
      origMin = std::min(origMin, t.id);
      origMax = std::max(origMax, t.id);
    }
  }
  const bool hasOriginal = origMin <= origMax; // false only for a hand-built Position the parser would reject
  const int whiteCreated = hasOriginal ? std::max(0, maxId - origMax) : 0;
  const int blackCreated = hasOriginal ? std::max(0, origMin - minId) : 0;
  int present = INT_MAX;
  for (const auto& t : p.timelines) {
    bool active = true;
    if (hasOriginal and (t.id < origMin or t.id > origMax))
      active = t.id > origMax ? t.id - origMax <= blackCreated + 1 : origMin - t.id <= whiteCreated + 1;
    if (active) present = std::min(present, t.boards.back().halfTurn);
  }
  return present;
}

std::string turnLabel(int halfTurn) {
  return "T" + std::to_string(halfTurn / 2 + 1) + (halfTurn % 2 == 0 ? "w" : "b");
}

} // namespace

std::string writePosition(const Position& p) {
  std::string out = "5dchess-position 1\n";
  out += "title: " + p.title + "\n";
  out += "size: " + std::to_string(p.size) + "\n";
  out += "rules:";
  if (!p.doubleStep and !p.castling) out += " none";
  if (p.doubleStep) out += " double-step";
  if (p.castling) out += " castling";
  out += "\n";
  out += std::string("to-move: ") + (p.toMove == PieceColor::PIECEWHITE ? "white" : "black") + "\n";
  if (!p.timelines.empty() and p.present != defaultPresent(p)) out += "present: " + std::to_string(p.present) + "\n";
  for (const auto& t : p.timelines) {
    if (t.parent) out += "L" + std::to_string(t.id) + " parent: L" + std::to_string(*t.parent) + "\n";
    for (const auto& b : t.boards)
      out += "L" + std::to_string(t.id) + " " + turnLabel(b.halfTurn) + ": " + writeBoard(b, p.size) + "\n";
  }
  return out;
}

Position parsePosition(std::string_view text) {
  Position p;
  p.present = INT_MIN; // "not given"
  bool sawMagic = false, sawSize = false, sawToMove = false, sawBoard = false;
  int totalBoards = 0;
  std::map<int, TimelineData> lines;
  std::string_view rest = text;
  int lineNo = 0;
  while (!rest.empty()) {
    const size_t nl = rest.find('\n');
    std::string_view line = trim(rest.substr(0, nl));
    rest = nl == std::string_view::npos ? std::string_view() : rest.substr(nl + 1);
    ++lineNo;
    if (line.empty() or line.front() == '#') continue;

    if (!sawMagic) {
      if (line != "5dchess-position 1") fail(lineNo, "expected the header '5dchess-position 1'");
      sawMagic = true;
      continue;
    }
    const size_t colon = line.find(':');
    if (colon == std::string_view::npos) fail(lineNo, "expected 'key: value'");
    const std::string_view key = trim(line.substr(0, colon));
    const std::string_view value = trim(line.substr(colon + 1));

    if (key.size() > 1 and key[0] == 'L' and key.find(' ') != std::string_view::npos) {
      // "L<id> T<turn><w|b>: <rows>"  or  "L<id> parent: L<id>"
      if (!sawSize or !sawToMove) fail(lineNo, "'size' and 'to-move' must come before the boards");
      const size_t sp = key.find(' ');
      const int id = parseInt(key.substr(1, sp - 1), lineNo, "timeline id", -kMaxTimelineId, kMaxTimelineId);
      sawBoard = true;
      const std::string_view what = trim(key.substr(sp + 1));
      TimelineData& tl = lines.try_emplace(id).first->second;
      tl.id = id;
      if (what == "parent") {
        if (value.size() < 2 or value[0] != 'L') fail(lineNo, "expected 'parent: L<id>'");
        tl.parent = parseInt(value.substr(1), lineNo, "parent id", -kMaxTimelineId, kMaxTimelineId);
      } else if (what.size() >= 3 and what[0] == 'T' and (what.back() == 'w' or what.back() == 'b')) {
        const int turn = parseInt(what.substr(1, what.size() - 2), lineNo, "turn", 1, kMaxHalfTurn / 2);
        const int half = 2 * (turn - 1) + (what.back() == 'b' ? 1 : 0);
        if (++totalBoards > kMaxBoards) fail(lineNo, "more than " + std::to_string(kMaxBoards) + " boards");
        if (!tl.boards.empty() and half != tl.boards.back().halfTurn + 1)
          fail(lineNo, "timeline L" + std::to_string(id) + ": boards must have consecutive half-turns");
        tl.boards.push_back(parseBoard(value, p.size, half, lineNo));
      } else {
        fail(lineNo, "expected T<turn>w, T<turn>b or parent after the timeline id");
      }
      continue;
    }

    if (sawBoard) fail(lineNo, "'" + std::string(key) + "' must come before the first board line");
    if (key == "title") {
      p.title = std::string(value);
    } else if (key == "size") {
      if (sawSize) fail(lineNo, "duplicate 'size'");
      p.size = parseInt(value, lineNo, "size", 1, kMaxSize);
      sawSize = true;
    } else if (key == "rules") {
      p.doubleStep = p.castling = false;
      std::istringstream in{std::string(value)};
      for (std::string rule; in >> rule;) {
        if (rule == "double-step") p.doubleStep = true;
        else if (rule == "castling") p.castling = true;
        else if (rule != "none") fail(lineNo, "unknown rule '" + rule + "'");
      }
    } else if (key == "to-move") {
      if (value == "white") p.toMove = PieceColor::PIECEWHITE;
      else if (value == "black") p.toMove = PieceColor::PIECEBLACK;
      else fail(lineNo, "to-move must be white or black");
      sawToMove = true;
    } else if (key == "present") {
      p.present = parseInt(value, lineNo, "present", 0, kMaxHalfTurn);
    } else {
      fail(lineNo, "unknown key '" + std::string(key) + "'");
    }
  }
  if (!sawMagic) fail(lineNo, "empty input");
  if (!sawSize or !sawToMove) fail(lineNo, "missing 'size' or 'to-move'");

  if (lines.empty()) fail(lineNo, "no boards");
  // Timeline structure (the engine's activity rule relies on it): the timelines without a parent are the originals and
  // have consecutive ids; every other id lies outside that range, all ids are consecutive, and parents form no cycle.
  int origMin = INT_MAX, origMax = INT_MIN, previous = 0;
  bool first = true;
  for (const auto& [id, tl] : lines) {
    const std::string name = "timeline L" + std::to_string(id);
    if (tl.boards.empty()) fail(lineNo, name + " has no boards");
    if (!first and id != previous + 1) fail(lineNo, name + ": timeline ids must be consecutive");
    first = false;
    previous = id;
    if (tl.parent) {
      if (!lines.count(*tl.parent)) fail(lineNo, name + ": unknown parent");
      continue;
    }
    if (origMin <= origMax and id != origMax + 1) fail(lineNo, name + ": original timelines (no parent) must have consecutive ids");
    origMin = std::min(origMin, id);
    origMax = std::max(origMax, id);
  }
  if (origMin > origMax) fail(lineNo, "no original timeline (every timeline has a parent)");
  for (const auto& [id, tl] : lines) {
    if (!tl.parent) continue;
    const std::string name = "timeline L" + std::to_string(id);
    if (id >= origMin and id <= origMax) fail(lineNo, name + ": a timeline with a parent must lie outside the original ids");
    const TimelineData* up = &tl;
    for (size_t steps = 0; up->parent; ++steps) {
      if (steps > lines.size()) fail(lineNo, name + ": parent chain is a cycle");
      up = &lines.at(*up->parent);
    }
  }
  for (auto& [id, tl] : lines) p.timelines.push_back(std::move(tl));
  const int computed = defaultPresent(p);
  if (p.present == INT_MIN) p.present = computed;
  else if (p.present != computed)
    fail(lineNo, "present " + std::to_string(p.present) + " is not the lowest latest half-turn of the active timelines (" +
                     std::to_string(computed) + ")");
  if ((p.present % 2 != 0) != (p.toMove == PieceColor::PIECEBLACK) or p.present < 0)
    fail(lineNo, "present half-turn does not match to-move");
  return p;
}

Position loadPositionFile(const std::string& path) {
  std::ifstream in(path, std::ios::binary);
  if (!in) throw ParseError("cannot read " + path);
  std::stringstream buffer;
  buffer << in.rdbuf();
  try {
    return parsePosition(buffer.str());
  } catch (const ParseError& e) {
    throw ParseError(path + ": " + e.what());
  }
}

} // namespace Chess::Core
