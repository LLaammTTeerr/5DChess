#include "services/SettingsStore.h"

namespace SettingsStore {

void apply(const settingsfile::Values& v, Settings& s, const std::function<bool(const std::string&)>& isKnownMusic) {
  auto find = [&v](const char* key) -> const std::string* {
    const auto it = v.find(key);
    return it == v.end() ? nullptr : &it->second;
  };
  if (const auto* theme = find("theme"))
    if (const PieceTheme* t = Themes::byName(*theme)) s.theme = *t;
  if (const auto* view = find("board_view")) boardview::fromName(*view, s.boardView);
  if (const auto* music = find("music"))
    if (!isKnownMusic || isKnownMusic(*music)) s.music = *music; // a track that no longer exists means "Off"
  if (const auto* sfx = find("sfx")) s.sfx = settingsfile::toBool(*sfx, s.sfx);
  if (const auto* reduce = find("reduce_motion")) s.reduceMotion = settingsfile::toBool(*reduce, s.reduceMotion);
  if (const auto* opponent = find("opponent")) s.vsComputer = *opponent == "computer"; // anything else: two players
  if (const auto* side = find("vs_side")) play::fromName(*side, s.vsSide);               // an unknown name keeps the value
  if (const auto* level = find("vs_level")) play::fromName(*level, s.vsLevel);
}

} // namespace SettingsStore
