#include "engine/Notation.h"

#include "engine/GameCatalog.h"

#include <charconv>
#include <stdexcept>

namespace Chess {

using Core::Coord;
using Core::ParseError;
using Core::PlayedMove;
using Core::PlayedTurn;

namespace {

// Bounds, so that a hostile record cannot make the replay run (or allocate) without limit; real games stay far below them.
constexpr size_t kMaxRecordBytes = 4u << 20;
constexpr int kMaxTurns = 5000;          // each submitted turn arms a search that copies every board
constexpr int kMaxMovesPerTurn = 256;
constexpr int kMaxTotalMoves = 50000;
constexpr int kMaxTimelineId = 1000;     // |L|, the same cap as the position format
constexpr int kMaxTurnNumber = 10000;
constexpr long long kResultSearchNodes = 2000000;

[[noreturn]] void fail(const std::string& what) { throw ParseError(what); }

std::string_view trim(std::string_view s) {
  while (!s.empty() and (s.front() == ' ' or s.front() == '\t' or s.front() == '\r')) s.remove_prefix(1);
  while (!s.empty() and (s.back() == ' ' or s.back() == '\t' or s.back() == '\r')) s.remove_suffix(1);
  return s;
}

// ---- moves ----------------------------------------------------------------------------------------------------------

struct Cursor {
  std::string_view text;
  size_t pos = 0;
  bool done() const { return pos >= text.size(); }
  char peek() const { return done() ? '\0' : text[pos]; }
  void expect(char c) {
    if (peek() != c) fail(std::string("expected '") + c + "' at position " + std::to_string(pos + 1));
    ++pos;
  }
  // [sign] digits, at most 6 digits, within lo..hi; the sign is one optional '-' (or '+' when allowPlus)
  int number(const char* what, int lo, int hi, bool allowPlus = false) {
    const size_t start = pos;
    if (peek() == '-' or (allowPlus and peek() == '+')) ++pos;
    while (!done() and text[pos] >= '0' and text[pos] <= '9') ++pos;
    int value = 0;
    const std::string_view digits = text.substr(start, pos - start);
    const bool signedNumber = !digits.empty() and (digits.front() == '-' or digits.front() == '+');
    const std::string_view number = signedNumber and digits.front() == '+' ? digits.substr(1) : digits;
    if (digits.size() - (signedNumber ? 1 : 0) == 0 or digits.size() - (signedNumber ? 1 : 0) > 6)
      fail(std::string("bad number for ") + what);
    std::from_chars(number.data(), number.data() + number.size(), value);
    if (value < lo or value > hi) fail(std::string(what) + " out of range " + std::to_string(lo) + ".." + std::to_string(hi));
    return value;
  }
};

Coord parseCoord(Cursor& c, PieceColor mover) {
  c.expect('(');
  c.expect('L');
  const int timeline = c.number("timeline", -kMaxTimelineId, kMaxTimelineId, true);
  c.expect('T');
  const int turn = c.number("turn", 1, kMaxTurnNumber);
  c.expect(')');
  const char file = c.peek();
  if (file < 'a' or file >= 'a' + Board::MAX_DIM) fail("file must be a letter a.." + std::string(1, char('a' + Board::MAX_DIM - 1)));
  ++c.pos;
  const char rank = c.peek();
  if (rank < '1' or rank >= '1' + Board::MAX_DIM) fail("rank must be a digit 1.." + std::to_string(Board::MAX_DIM));
  ++c.pos;
  return Coord{int8_t(file - 'a'), int8_t(rank - '1'), int16_t(2 * (turn - 1) + (mover == PieceColor::PIECEBLACK ? 1 : 0)),
               int16_t(timeline)};
}

struct ParsedMove {
  Core::Move move;
  bool suffix = false;
};

ParsedMove parseMoveText(std::string_view text, PieceColor mover) {
  Cursor c{text};
  ParsedMove out;
  out.move.from = parseCoord(c, mover);
  c.expect('>');
  out.move.to = parseCoord(c, mover);
  if (c.peek() == '=') {
    ++c.pos;
    switch (c.peek()) {
      case 'Q': out.move.promotion = PieceType::Queen; break;
      case 'R': out.move.promotion = PieceType::Rook; break;
      case 'B': out.move.promotion = PieceType::Bishop; break;
      case 'N': out.move.promotion = PieceType::Knight; break;
      default: fail("promotion must be =Q, =R, =B or =N");
    }
    ++c.pos;
    out.suffix = true;
  }
  if (!c.done()) fail("unexpected '" + std::string(1, c.peek()) + "' after the move");
  return out;
}

std::string squareText(const Coord& c) {
  return "(L" + std::to_string(c.l) + "T" + std::to_string(c.t / 2 + 1) + ")" + char('a' + c.x) + char('1' + c.y);
}

// ---- records --------------------------------------------------------------------------------------------------------

std::string turnLabel(int halfTurn) { return "T" + std::to_string(halfTurn / 2 + 1) + (halfTurn % 2 == 0 ? "w" : "b"); }

} // namespace

std::string toNotation(const Core::Move& m, bool promotes) {
  std::string out = squareText(m.from) + ">" + squareText(m.to);
  if (promotes or m.promotion != PieceType::Queen) out += std::string("=") + pieceSymbol(m.promotion);
  return out;
}

Core::Move parseMove(std::string_view text, PieceColor mover) {
  try {
    return parseMoveText(text, mover).move;
  } catch (const ParseError& e) {
    throw ParseError("move '" + std::string(text) + "': " + e.what());
  }
}

std::string writeRecord(const IGame& game, bool* droppedPending) {
  if (droppedPending) *droppedPending = !game.pendingMoves().empty();
  std::string out = "5dchess-record 1\n";
  if (!game.modeId().empty()) {
    out += "mode: " + game.modeId() + "\n";
  } else {
    if (game.startPosition().empty()) throw std::logic_error("writeRecord: the game has no recorded starting position");
    out += "position:\n" + game.startPosition();
    if (out.back() != '\n') out += '\n';
    out += "end-position\n";
  }
  for (const Core::PlayedTurn& turn : game.history()) {
    out += turnLabel(turn.presentHalfTurn) + ":";
    for (const PlayedMove& m : turn.moves) out += " " + toNotation(m.move, m.promotes);
    out += "\n";
  }
  return out;
}

std::shared_ptr<IGame> loadRecord(std::string_view text) {
  if (text.size() > kMaxRecordBytes) fail("record is larger than " + std::to_string(kMaxRecordBytes) + " bytes");
  std::shared_ptr<IGame> game;
  bool sawMagic = false;
  int lineNo = 0, turns = 0, totalMoves = 0;
  std::string_view rest = text;

  auto failAt = [&](const std::string& what) { fail("line " + std::to_string(lineNo) + ": " + what); };

  while (!rest.empty()) {
    const size_t nl = rest.find('\n');
    std::string_view line = trim(rest.substr(0, nl));
    rest = nl == std::string_view::npos ? std::string_view() : rest.substr(nl + 1);
    ++lineNo;
    if (line.empty() or line.front() == '#') continue;

    if (!sawMagic) {
      if (line != "5dchess-record 1") failAt("expected the header '5dchess-record 1'");
      sawMagic = true;
      continue;
    }

    if (!game) {
      // Header: the starting position.
      if (line.substr(0, 5) == "mode:") {
        const std::string id(trim(line.substr(5)));
        game = GameCatalog::create(id);
        if (!game) failAt("unknown game mode '" + id + "'");
      } else if (line == "position:") {
        std::string block;
        const int first = lineNo;
        bool closed = false;
        while (!rest.empty()) {
          const size_t end = rest.find('\n');
          const std::string_view inner = rest.substr(0, end);
          rest = end == std::string_view::npos ? std::string_view() : rest.substr(end + 1);
          ++lineNo;
          if (trim(inner) == "end-position") { closed = true; break; }
          block.append(inner).append("\n");
        }
        if (!closed) failAt("'position:' without 'end-position'");
        try {
          game = Core::parsePosition(block).makeGame();
        } catch (const ParseError& e) {
          fail("position block starting at line " + std::to_string(first) + ": " + e.what());
        }
      } else {
        failAt("expected 'mode: <id>' or 'position:'");
      }
      continue;
    }

    // A turn: "T<n><w|b>: move move ..."
    const size_t colon = line.find(':');
    if (colon == std::string_view::npos) failAt("expected 'T<turn><w|b>: moves'");
    const std::string_view label = trim(line.substr(0, colon));
    if (++turns > kMaxTurns) failAt("more than " + std::to_string(kMaxTurns) + " turns");
    if (game->result() != GameResult::Ongoing) failAt("the game was already decided");
    const std::string expected = turnLabel(game->presentHalfTurn());
    if (label != expected) failAt("turn label '" + std::string(label) + "', expected '" + expected + "'");

    const PieceColor mover = game->getCurrentTurnColor();
    std::string_view moves = trim(line.substr(colon + 1));
    int inTurn = 0;
    while (!moves.empty()) {
      const size_t sp = moves.find(' ');
      const std::string_view token = moves.substr(0, sp);
      moves = sp == std::string_view::npos ? std::string_view() : trim(moves.substr(sp + 1));
      if (++inTurn > kMaxMovesPerTurn or ++totalMoves > kMaxTotalMoves) failAt("too many moves");
      ParsedMove parsed;
      try {
        parsed = parseMoveText(token, mover);
      } catch (const ParseError& e) {
        failAt("move '" + std::string(token) + "': " + e.what());
      }
      // Legal for the side to move? The engine lists every (target, promotion choice) of the piece on the source square.
      int variants = 0;
      bool legal = false;
      for (const Core::Move& m : game->legalMovesFrom(parsed.move.from)) {
        if (m.to != parsed.move.to) continue;
        ++variants;
        legal = legal or m.promotion == parsed.move.promotion;
      }
      if (!legal) failAt("illegal move '" + std::string(token) + "'");
      if (parsed.suffix != (variants > 1)) failAt(variants > 1 ? "promotion needs a suffix (=Q, =R, =B or =N)" : "'" + std::string(token) + "' does not promote");
      game->makeMove(parsed.move);
    }
    if (inTurn == 0) failAt("a turn needs at least one move");
    if (!game->canSubmit()) failAt("the moves of this turn do not make a legal turn (a board was left unmoved or a king can be captured)");
    game->submitTurn();
  }
  if (!sawMagic) fail("line 1: empty input");
  if (!game) fail("line " + std::to_string(lineNo) + ": missing 'mode:' or 'position:'");
  if (game->resultPending()) game->resolveResult(kResultSearchNodes);
  return game;
}

} // namespace Chess
