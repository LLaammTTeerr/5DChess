#include "services/SettingsStore.h"
#include <cstdlib>
#include <string>
#include "Audio/AudioManager.h"
#include "services/SettingsFile.h"
#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#else
#include <filesystem>
#include <fstream>
#include <sstream>
#endif

namespace SettingsStore {

namespace {

settingsfile::Values toValues(const Settings& s) {
  settingsfile::Values v;
  v["theme"] = Themes::nameOf(s.theme);
  v["board_view"] = boardview::name(s.boardView);
  v["music"] = s.music;
  v["sfx"] = settingsfile::fromBool(s.sfx);
  v["reduce_motion"] = settingsfile::fromBool(s.reduceMotion);
  return v;
}

void applyValues(const settingsfile::Values& v, Settings& s) {
  auto find = [&v](const char* key) -> const std::string* {
    const auto it = v.find(key);
    return it == v.end() ? nullptr : &it->second;
  };
  if (const auto* theme = find("theme"))
    if (const PieceTheme* t = Themes::byName(*theme)) s.theme = *t;
  if (const auto* view = find("board_view")) boardview::fromName(*view, s.boardView);
  if (const auto* music = find("music")) {
    bool known = *music == AudioManager::offName();
    for (const auto& track : AudioManager::tracks()) known = known || track.name == *music;
    if (known) s.music = *music; // a track that no longer exists means "Off"
  }
  if (const auto* sfx = find("sfx")) s.sfx = settingsfile::toBool(*sfx, s.sfx);
  if (const auto* reduce = find("reduce_motion")) s.reduceMotion = settingsfile::toBool(*reduce, s.reduceMotion);
}

#ifndef __EMSCRIPTEN__
std::string settingsPath() {
  return settingsfile::pathFor(settingsfile::currentPlatform(), [](const char* name) {
    const char* value = std::getenv(name);
    return std::string(value ? value : "");
  });
}
#endif

} // namespace

#ifdef __EMSCRIPTEN__

void load(Settings& settings) {
  const char* text = emscripten_run_script_string("(function(){try{return localStorage.getItem('5dchess.settings')||'';}catch(e){return '';}})()");
  if (text && *text) applyValues(settingsfile::parse(text), settings);
}

void save(const Settings& settings) {
  const std::string text = settingsfile::format(toValues(settings));
  EM_ASM({ try { localStorage.setItem('5dchess.settings', UTF8ToString($0)); } catch (e) {} }, text.c_str());
}

#else

void load(Settings& settings) {
  const std::string path = settingsPath();
  if (path.empty()) return;
  std::ifstream in(path);
  if (!in) return;
  std::ostringstream text;
  text << in.rdbuf();
  applyValues(settingsfile::parse(text.str()), settings);
}

void save(const Settings& settings) {
  const std::string path = settingsPath();
  if (path.empty()) return;
  namespace fs = std::filesystem;
  std::error_code ec;
  fs::create_directories(fs::path(path).parent_path(), ec);
  if (ec) return;
  const std::string temp = path + ".tmp";
  {
    std::ofstream out(temp, std::ios::trunc);
    if (!out) return;
    out << settingsfile::format(toValues(settings));
    if (!out) return;
  }
  fs::rename(temp, path, ec); // atomic on the platforms we ship for; a failure leaves the old file
  if (ec) fs::remove(temp, ec);
}

#endif

} // namespace SettingsStore
