// Where the puzzle progress lives (see puzzles/Progress.h). Desktop: puzzles.txt next to settings.txt in the config
// directory, written to a temporary file first and renamed over the old one; web: localStorage "5dchess.puzzles"; the UI
// test harness (TestMode::active) never touches either: progress lives in memory for the run.
#include <cstdlib>
#include <string>
#include "TestMode.h"
#include "puzzles/Progress.h"
#include "services/SettingsFile.h"
#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#else
#include <filesystem>
#include <fstream>
#include <sstream>
#endif

namespace puzzles {

namespace {

#ifndef __EMSCRIPTEN__
std::string progressPath() {
  return settingsfile::pathFor(settingsfile::currentPlatform(),
                               [](const char* name) {
                                 const char* value = std::getenv(name);
                                 return std::string(value ? value : "");
                               },
                               "puzzles.txt");
}
#endif

std::string readStored() {
  if (TestMode::get().active) return "";
#ifdef __EMSCRIPTEN__
  const char* text = emscripten_run_script_string(
      "(function(){try{return localStorage.getItem('5dchess.puzzles')||'';}catch(e){return '';}})()");
  return text ? text : "";
#else
  const std::string path = progressPath();
  std::error_code ec;
  if (path.empty() || !std::filesystem::is_regular_file(path, ec) || std::filesystem::file_size(path, ec) > 64 * 1024) return "";
  std::ifstream in(path);
  std::ostringstream text;
  text << in.rdbuf();
  return text.str();
#endif
}

void writeStored(const std::string& text) {
  if (TestMode::get().active) return;
#ifdef __EMSCRIPTEN__
  EM_ASM({ try { localStorage.setItem('5dchess.puzzles', UTF8ToString($0)); } catch (e) {} }, text.c_str());
#else
  const std::string path = progressPath();
  if (path.empty()) return;
  namespace fs = std::filesystem;
  std::error_code ec;
  fs::create_directories(fs::path(path).parent_path(), ec);
  if (ec) return;
  const std::string temp = path + ".tmp";
  {
    std::ofstream out(temp, std::ios::trunc);
    if (!out) return;
    out << text;
    if (!out) return;
  }
  fs::rename(temp, path, ec);
  if (ec) fs::remove(temp, ec);
#endif
}

Progress& instance() {
  static Progress p = Progress::fromText(readStored());
  return p;
}

} // namespace

Progress& progress() { return instance(); }

void markSolved(const std::string& id) {
  if (instance().markSolved(id)) writeStored(instance().toText());
}

void resetProgress() {
  instance().clear();
  writeStored(instance().toText());
}

} // namespace puzzles
