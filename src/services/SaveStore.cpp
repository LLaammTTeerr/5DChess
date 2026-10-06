#include "services/SaveStore.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>
#include "engine/GameCatalog.h"
#include "engine/Notation.h"
#include "engine/Position.h"

namespace savegame {

// ---- FileStorage -------------------------------------------------------------------------------------------------------

std::string FileStorage::pathOf(const std::string& name) const { return (std::filesystem::path(_dir) / (name + ".5dr")).string(); }

ReadStatus FileStorage::read(const std::string& name, std::string& text) const {
  namespace fs = std::filesystem;
  text.clear();
  const std::string path = pathOf(name);
  std::error_code ec;
  if (!fs::exists(path, ec)) return ReadStatus::Missing;
  if (!fs::is_regular_file(path, ec)) return ReadStatus::Error;
  const auto size = fs::file_size(path, ec);
  if (ec) return ReadStatus::Error;
  if (size > kMaxBytes) return ReadStatus::TooLarge;
  std::ifstream in(path, std::ios::binary);
  if (!in) return ReadStatus::Error;
  std::ostringstream buffer;
  buffer << in.rdbuf();
  text = buffer.str();
  if (text.size() > kMaxBytes) { // the file grew meanwhile
    text.clear();
    return ReadStatus::TooLarge;
  }
  return ReadStatus::Ok;
}

bool FileStorage::exists(const std::string& name) const {
  std::error_code ec;
  return std::filesystem::is_regular_file(pathOf(name), ec);
}

bool FileStorage::write(const std::string& name, const std::string& text) {
  namespace fs = std::filesystem;
  std::error_code ec;
  fs::create_directories(_dir, ec);
  if (ec) return false;
  const std::string path = pathOf(name), temp = path + ".tmp";
  {
    std::ofstream out(temp, std::ios::binary | std::ios::trunc);
    if (!out) return false;
    out << text;
    out.flush();
    if (!out) {
      out.close();
      fs::remove(temp, ec);
      return false;
    }
  }
  fs::rename(temp, path, ec); // a failure leaves the old file
  if (ec) {
    fs::remove(temp, ec);
    return false;
  }
  return true;
}

void FileStorage::remove(const std::string& name) {
  std::error_code ec;
  std::filesystem::remove(pathOf(name), ec);
}

// ---- MemoryStorage -----------------------------------------------------------------------------------------------------

ReadStatus MemoryStorage::read(const std::string& name, std::string& text) const {
  text.clear();
  for (const auto& blob : blobs)
    if (blob.first == name) {
      if (blob.second.size() > kMaxBytes) return ReadStatus::TooLarge;
      text = blob.second;
      return ReadStatus::Ok;
    }
  return ReadStatus::Missing;
}

bool MemoryStorage::exists(const std::string& name) const {
  return std::any_of(blobs.begin(), blobs.end(), [&](const auto& b) { return b.first == name; });
}

bool MemoryStorage::write(const std::string& name, const std::string& text) {
  remove(name);
  blobs.emplace_back(name, text);
  return true;
}

void MemoryStorage::remove(const std::string& name) {
  blobs.erase(std::remove_if(blobs.begin(), blobs.end(), [&](const auto& b) { return b.first == name; }), blobs.end());
}

// ---- Record text -------------------------------------------------------------------------------------------------------

LoadResult loadText(std::string_view text) {
  LoadResult result;
  try {
    result.game = Chess::loadRecord(text);
    if (!result.game) result.error = "empty result";
  } catch (const std::exception& e) {
    result.game.reset();
    result.error = e.what();
  }
  return result;
}

namespace {

std::string_view trim(std::string_view s) {
  while (!s.empty() && (s.front() == ' ' || s.front() == '\t' || s.front() == '\r')) s.remove_prefix(1);
  while (!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\r')) s.remove_suffix(1);
  return s;
}

bool isTurnLine(std::string_view line) { // T<digits><w|b>:
  if (line.size() < 4 || line[0] != 'T') return false;
  size_t i = 1;
  while (i < line.size() && line[i] >= '0' && line[i] <= '9') ++i;
  return i > 1 && i + 1 < line.size() && (line[i] == 'w' || line[i] == 'b') && line[i + 1] == ':';
}

} // namespace

SlotSummary summarize(std::string_view text) {
  SlotSummary s;
  s.state = SlotSummary::State::Unreadable;
  bool magic = false, inPosition = false;
  size_t pos = 0;
  while (pos < text.size()) {
    size_t end = text.find('\n', pos);
    if (end == std::string_view::npos) end = text.size();
    const std::string_view line = trim(text.substr(pos, end - pos));
    pos = end + 1;
    if (line.empty()) continue;
    if (line[0] == '#') {
      constexpr std::string_view tag = "# saved:";
      if (line.substr(0, tag.size()) == tag) s.date = std::string(trim(line.substr(tag.size())).substr(0, 32));
      continue;
    }
    if (!magic) {
      if (line != "5dchess-record 1") return s;
      magic = true;
      continue;
    }
    if (inPosition) { // the board text of an embedded position is none of our business
      if (line == "end-position") inPosition = false;
      continue;
    }
    if (line.substr(0, 5) == "mode:") {
      const std::string id(trim(line.substr(5)));
      const Chess::ModeInfo* mode = Chess::GameCatalog::findById(id);
      s.title = mode ? mode->title : id.substr(0, 40);
    } else if (line == "position:") {
      s.title = "Custom position";
      inPosition = true;
    } else if (isTurnLine(line)) {
      ++s.turns;
    }
  }
  if (!magic || s.title.empty()) {
    s.turns = 0;
    return s;
  }
  s.state = SlotSummary::State::Ready;
  return s;
}

std::string SlotSummary::describe() const {
  switch (state) {
    case State::Empty: return "Empty";
    case State::Unreadable: return "Unreadable save";
    case State::Ready: break;
  }
  std::string out = title + " - " + std::to_string(turns) + (turns == 1 ? " turn" : " turns");
  if (!date.empty()) out += " - " + date;
  return out;
}

// ---- SaveStore ---------------------------------------------------------------------------------------------------------

bool SaveStore::autosave(const Chess::IGame& game) {
  if (game.history().empty()) return false;
  try {
    const std::string text = Chess::writeRecord(game);
    if (text.size() > kMaxBytes) return false;
    return _storage->write("autosave", text);
  } catch (const std::exception&) { // a game without a start position (a test sandbox)
    return false;
  }
}

bool SaveStore::saveSlot(int slot, const Chess::IGame& game, const std::string& stamp, bool* droppedPending) {
  if (slot < 0 || slot >= kSlots) return false;
  try {
    std::string text = Chess::writeRecord(game, droppedPending);
    // The date is a comment line: a record reader skips it, the slot list shows it
    const size_t eol = text.find('\n');
    std::string safeStamp = stamp;
    std::replace(safeStamp.begin(), safeStamp.end(), '\n', ' ');
    text.insert(eol == std::string::npos ? text.size() : eol + 1, "# saved: " + safeStamp + "\n");
    if (text.size() > kMaxBytes) return false;
    return _storage->write(slotName(slot), text);
  } catch (const std::exception&) {
    return false;
  }
}

SlotSummary SaveStore::slot(int slot) const {
  SlotSummary s;
  if (slot < 0 || slot >= kSlots) return s;
  std::string text;
  switch (_storage->read(slotName(slot), text)) {
    case ReadStatus::Missing: return s;
    case ReadStatus::Ok: return summarize(text);
    case ReadStatus::TooLarge:
    case ReadStatus::Error: s.state = SlotSummary::State::Unreadable; return s;
  }
  return s;
}

bool SaveStore::anySlot() const {
  for (int i = 0; i < kSlots; ++i)
    if (_storage->exists(slotName(i))) return true;
  return false;
}

LoadResult SaveStore::load(const std::string& name) const {
  std::string text;
  switch (_storage->read(name, text)) {
    case ReadStatus::Ok: return loadText(text);
    case ReadStatus::Missing: return {nullptr, "no such save"};
    case ReadStatus::TooLarge: return {nullptr, "file too large"};
    case ReadStatus::Error: return {nullptr, "file unreadable"};
  }
  return {nullptr, "file unreadable"};
}

} // namespace savegame
