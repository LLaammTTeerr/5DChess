#include "play/VsAi.h"
#include <charconv>
#include "engine/GameCatalog.h"
#include "engine/Position.h"

namespace play {

namespace {

std::uint64_t splitmix(std::uint64_t z) {
  z += 0x9e3779b97f4a7c15ull;
  z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ull;
  z = (z ^ (z >> 27)) * 0x94d049bb133111ebull;
  return z ^ (z >> 31);
}

std::string_view trim(std::string_view s) {
  while (!s.empty() && (s.front() == ' ' || s.front() == '\t' || s.front() == '\r')) s.remove_prefix(1);
  while (!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\r')) s.remove_suffix(1);
  return s;
}

constexpr std::string_view kMetaTag = "# vs-computer:";

} // namespace

const char* name(SideChoice side) {
  switch (side) {
    case SideChoice::White: return "white";
    case SideChoice::Black: return "black";
    case SideChoice::Random: return "random";
  }
  return "white";
}

const char* name(AiLevel level) {
  switch (level) {
    case AiLevel::Easy: return "easy";
    case AiLevel::Normal: return "normal";
    case AiLevel::Hard: return "hard";
  }
  return "normal";
}

bool fromName(std::string_view text, SideChoice& out) {
  for (SideChoice s : {SideChoice::White, SideChoice::Black, SideChoice::Random})
    if (text == name(s)) { out = s; return true; }
  return false;
}

bool fromName(std::string_view text, AiLevel& out) {
  for (AiLevel l : {AiLevel::Easy, AiLevel::Normal, AiLevel::Hard})
    if (text == name(l)) { out = l; return true; }
  return false;
}

const char* title(SideChoice side) {
  switch (side) {
    case SideChoice::White: return "White";
    case SideChoice::Black: return "Black";
    case SideChoice::Random: return "Random";
  }
  return "White";
}

const char* title(AiLevel level) {
  switch (level) {
    case AiLevel::Easy: return "Easy";
    case AiLevel::Normal: return "Normal";
    case AiLevel::Hard: return "Hard";
  }
  return "Normal";
}

const char* describe(AiLevel level) {
  switch (level) {
    case AiLevel::Easy: return "Easy: plays quickly and often misses tactics. Good for learning.";
    case AiLevel::Normal: return "Normal: looks a few turns ahead. A fair game.";
    case AiLevel::Hard: return "Hard: slowest, searches deepest; can take a while on big multiverses.";
  }
  return "";
}

Chess::PieceColor humanSide(SideChoice choice, std::uint64_t seed) {
  using Chess::PieceColor;
  switch (choice) {
    case SideChoice::White: return PieceColor::PIECEWHITE;
    case SideChoice::Black: return PieceColor::PIECEBLACK;
    case SideChoice::Random: break;
  }
  return (splitmix(seed ^ 0x51de5eedull) >> 33) & 1 ? PieceColor::PIECEBLACK : PieceColor::PIECEWHITE;
}

VsAi makeVsAi(SideChoice choice, AiLevel level, std::uint64_t seed) { return {humanSide(choice, seed), level, seed}; }

std::uint64_t searchSeed(std::uint64_t gameSeed, std::size_t turnsPlayed) {
  return splitmix(gameSeed + 0x9e3779b97f4a7c15ull * (static_cast<std::uint64_t>(turnsPlayed) + 1));
}

std::string formatMeta(const VsAi& vs) {
  return std::string(kMetaTag) + " you=" + (vs.human == Chess::PieceColor::PIECEWHITE ? "white" : "black") +
         " level=" + name(vs.level) + " seed=" + std::to_string(vs.seed);
}

std::optional<VsAi> parseMeta(std::string_view line) {
  line = trim(line);
  if (line.substr(0, kMetaTag.size()) != kMetaTag) return std::nullopt;
  line = trim(line.substr(kMetaTag.size()));
  VsAi vs;
  bool haveYou = false, haveLevel = false, haveSeed = false;
  while (!line.empty()) {
    const size_t sp = line.find(' ');
    const std::string_view field = line.substr(0, sp);
    line = sp == std::string_view::npos ? std::string_view() : trim(line.substr(sp + 1));
    const size_t eq = field.find('=');
    if (eq == std::string_view::npos) return std::nullopt;
    const std::string_view key = field.substr(0, eq), value = field.substr(eq + 1);
    if (key == "you" && !haveYou) {
      if (value == "white") vs.human = Chess::PieceColor::PIECEWHITE;
      else if (value == "black") vs.human = Chess::PieceColor::PIECEBLACK;
      else return std::nullopt;
      haveYou = true;
    } else if (key == "level" && !haveLevel) {
      if (!fromName(value, vs.level)) return std::nullopt;
      haveLevel = true;
    } else if (key == "seed" && !haveSeed) {
      if (value.empty() || value.size() > 20) return std::nullopt;
      const auto [end, ec] = std::from_chars(value.data(), value.data() + value.size(), vs.seed);
      if (ec != std::errc() || end != value.data() + value.size()) return std::nullopt;
      haveSeed = true;
    } else {
      return std::nullopt;
    }
  }
  if (!haveYou || !haveLevel || !haveSeed) return std::nullopt;
  return vs;
}

std::optional<VsAi> findMeta(std::string_view record) {
  bool first = true;
  size_t pos = 0;
  while (pos < record.size()) {
    size_t end = record.find('\n', pos);
    if (end == std::string_view::npos) end = record.size();
    const std::string_view line = trim(record.substr(pos, end - pos));
    pos = end + 1;
    if (line.empty()) continue;
    if (first) { first = false; continue; } // the magic
    if (line[0] != '#') break;               // the header: the comments are over
    if (const auto vs = parseMeta(line)) return vs;
  }
  return std::nullopt;
}

std::string withMeta(const std::string& record, const VsAi& vs) {
  const std::string line = formatMeta(vs) + "\n";
  const size_t eol = record.find('\n');
  if (eol == std::string::npos) return record + "\n" + line;
  return record.substr(0, eol + 1) + line + record.substr(eol + 1);
}

int takeBackCount(const VsAi& vs, Chess::PieceColor toMove, std::size_t historySize, bool pending, bool computerMoving) {
  const bool computersTurn = toMove == vs.aiColor();
  if (computersTurn && (computerMoving || pending)) return 0;
  if (!computersTurn && pending) return 0;
  const int turns = computersTurn ? 1 : 2;
  const std::size_t needed = static_cast<std::size_t>(turns) + (vs.human == Chess::PieceColor::PIECEBLACK ? 1 : 0);
  return historySize >= needed ? turns : 0;
}

std::shared_ptr<Chess::IGame> replayPrefix(const Chess::IGame& game, std::size_t turns) {
  if (turns > game.history().size()) return nullptr;
  std::shared_ptr<Chess::IGame> copy;
  try {
    if (!game.modeId().empty()) copy = Chess::GameCatalog::create(game.modeId());
    else if (!game.startPosition().empty()) copy = Chess::Core::parsePosition(game.startPosition()).makeGame();
  } catch (const std::exception&) {
    return nullptr;
  }
  if (!copy) return nullptr;
  for (std::size_t i = 0; i < turns; ++i) {
    for (const Chess::Core::PlayedMove& played : game.history()[i].moves) copy->makeMove(played.move);
    if (!copy->canSubmit()) return nullptr;
    copy->submitTurn();
  }
  return copy;
}

} // namespace play
