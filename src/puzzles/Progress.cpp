#include "puzzles/Progress.h"
#include "services/SettingsFile.h"

namespace puzzles {

namespace {
constexpr size_t kMaxIds = 1000;
constexpr size_t kMaxText = 64 * 1024;
}

bool Progress::validId(const std::string& id) {
  if (id.empty() || id.size() > 64) return false;
  for (char c : id)
    if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_')) return false;
  return true;
}

bool Progress::markSolved(const std::string& id) {
  if (!validId(id) || _solved.size() >= kMaxIds) return false;
  return _solved.insert(id).second;
}

std::string Progress::toText() const {
  std::string ids;
  for (const std::string& id : _solved) ids += (ids.empty() ? "" : ",") + id;
  settingsfile::Values values;
  values["solved"] = ids;
  return settingsfile::format(values);
}

Progress Progress::fromText(const std::string& text) {
  Progress p;
  if (text.size() > kMaxText) return p;
  const settingsfile::Values values = settingsfile::parse(text);
  const auto it = values.find("solved");
  if (it == values.end()) return p;
  const std::string& list = it->second;
  size_t start = 0;
  while (start <= list.size()) {
    size_t comma = list.find(',', start);
    if (comma == std::string::npos) comma = list.size();
    p.markSolved(list.substr(start, comma - start));
    start = comma + 1;
  }
  return p;
}

} // namespace puzzles
