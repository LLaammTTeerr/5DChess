// Where the saved games live (see services/SaveStore.h). Desktop: next to settings.txt in the config directory
// (autosave.5dr, slot1.5dr ... slot3.5dr); web: localStorage keys 5dchess.autosave, 5dchess.slot1 ...; the UI test
// harness (TestMode::active) never touches either, it gets an empty in-process storage.
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <string>
#include "TestMode.h"
#include "services/SaveStore.h"
#include "services/SettingsFile.h"
#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#endif

namespace savegame {

#ifdef __EMSCRIPTEN__

namespace {

class LocalStorage : public Storage {
public:
  ReadStatus read(const std::string& name, std::string& text) const override {
    text.clear();
    const std::string key = "5dchess." + name;
    const int length = EM_ASM_INT({
      try { const v = localStorage.getItem(UTF8ToString($0)); return v === null ? -1 : v.length; } catch (e) { return -2; }
    }, key.c_str());
    if (length == -1) return ReadStatus::Missing;
    if (length < 0) return ReadStatus::Error;
    if (static_cast<size_t>(length) > kMaxBytes) return ReadStatus::TooLarge;
    const std::string script = "(function(){try{return localStorage.getItem('" + key + "')||'';}catch(e){return '';}})()";
    if (const char* value = emscripten_run_script_string(script.c_str())) text = value;
    return text.size() > kMaxBytes ? ReadStatus::TooLarge : ReadStatus::Ok;
  }
  bool exists(const std::string& name) const override {
    const std::string key = "5dchess." + name;
    return EM_ASM_INT({ try { return localStorage.getItem(UTF8ToString($0)) !== null ? 1 : 0; } catch (e) { return 0; } }, key.c_str()) != 0;
  }
  bool write(const std::string& name, const std::string& text) override {
    const std::string key = "5dchess." + name;
    return EM_ASM_INT({ try { localStorage.setItem(UTF8ToString($0), UTF8ToString($1)); return 1; } catch (e) { return 0; } },
                      key.c_str(), text.c_str()) != 0;
  }
  void remove(const std::string& name) override {
    const std::string key = "5dchess." + name;
    EM_ASM({ try { localStorage.removeItem(UTF8ToString($0)); } catch (e) {} }, key.c_str());
  }
};

} // namespace

#endif

std::unique_ptr<Storage> defaultStorage() {
  if (TestMode::get().active) return std::make_unique<MemoryStorage>();
#ifdef __EMSCRIPTEN__
  return std::make_unique<LocalStorage>();
#else
  const std::string path = settingsfile::pathFor(settingsfile::currentPlatform(),
                                                 [](const char* name) {
                                                   const char* value = std::getenv(name);
                                                   return std::string(value ? value : "");
                                                 },
                                                 "autosave.5dr");
  if (path.empty()) return std::make_unique<MemoryStorage>(); // no HOME: saves last for this run only
  return std::make_unique<FileStorage>(std::filesystem::path(path).parent_path().string());
#endif
}

std::string timestamp() {
  if (TestMode::get().active) return "2026-01-01 12:00";
  const std::time_t now = std::time(nullptr);
  std::tm local{};
#ifdef _WIN32
  localtime_s(&local, &now);
#else
  localtime_r(&now, &local);
#endif
  char buffer[32];
  if (std::strftime(buffer, sizeof buffer, "%Y-%m-%d %H:%M", &local) == 0) return "";
  return buffer;
}

} // namespace savegame
