#include "services/SettingsFile.h"
#include <sstream>

namespace settingsfile {

namespace {
std::string trim(const std::string& s) {
  const auto b = s.find_first_not_of(" \t\r");
  if (b == std::string::npos) return "";
  const auto e = s.find_last_not_of(" \t\r");
  return s.substr(b, e - b + 1);
}
} // namespace

Values parse(const std::string& text) {
  Values values;
  std::istringstream in(text);
  std::string line;
  while (std::getline(in, line)) {
    line = trim(line);
    if (line.empty() || line[0] == '#') continue;
    const auto eq = line.find('=');
    if (eq == std::string::npos || eq == 0) continue;
    const std::string key = trim(line.substr(0, eq));
    if (!key.empty()) values[key] = trim(line.substr(eq + 1));
  }
  return values;
}

std::string format(const Values& values) {
  std::string out = "# 5DChess settings\n";
  for (const auto& [key, value] : values) out += key + "=" + value + "\n";
  return out;
}

bool toBool(const std::string& value, bool fallback) {
  if (value == "on" || value == "true" || value == "1") return true;
  if (value == "off" || value == "false" || value == "0") return false;
  return fallback;
}

const char* fromBool(bool value) { return value ? "on" : "off"; }

Platform currentPlatform() {
#if defined(_WIN32)
  return Platform::Windows;
#elif defined(__APPLE__)
  return Platform::MacOS;
#else
  return Platform::Linux;
#endif
}

std::string pathFor(Platform platform, const std::function<std::string(const char*)>& getenv, const std::string& file) {
  switch (platform) {
    case Platform::Windows: {
      const std::string appData = getenv("APPDATA");
      return appData.empty() ? "" : appData + "\\5DChess\\" + file;
    }
    case Platform::MacOS: {
      const std::string home = getenv("HOME");
      return home.empty() ? "" : home + "/Library/Application Support/5DChess/" + file;
    }
    case Platform::Linux: {
      const std::string xdg = getenv("XDG_CONFIG_HOME");
      if (!xdg.empty() && xdg[0] == '/') return xdg + "/5dchess/" + file;
      const std::string home = getenv("HOME");
      return home.empty() ? "" : home + "/.config/5dchess/" + file;
    }
  }
  return "";
}

} // namespace settingsfile
