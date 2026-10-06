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
  v["opponent"] = s.vsComputer ? "computer" : "two_players";
  v["vs_side"] = play::name(s.vsSide);
  v["vs_level"] = play::name(s.vsLevel);
  return v;
}

void applyValues(const settingsfile::Values& v, Settings& s) {
  apply(v, s, [](const std::string& music) {
    if (music == AudioManager::offName()) return true;
    for (const auto& track : AudioManager::tracks())
      if (track.name == music) return true;
    return false;
  });
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
  std::error_code ec;
  if (!std::filesystem::is_regular_file(path, ec) || std::filesystem::file_size(path, ec) > 64 * 1024) return; // a settings file is tiny
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
