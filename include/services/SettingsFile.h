#pragma once
#include <functional>
#include <map>
#include <string>

// The text form of the settings file and where it lives. No graphics or engine dependency, so it is unit tested.
//
//   # 5DChess settings
//   theme=Pixel
//   board_view=Deep space
//   music=Off
//   sfx=on
//   reduce_motion=off
//
// One `key=value` per line; blank lines and lines starting with '#' are ignored; unknown keys are kept out by the
// caller, malformed lines are skipped, a later duplicate wins.
namespace settingsfile {

using Values = std::map<std::string, std::string>;

Values parse(const std::string& text);
std::string format(const Values& values);

bool toBool(const std::string& value, bool fallback);
const char* fromBool(bool value);

enum class Platform { Linux, MacOS, Windows };
Platform currentPlatform();

/// The settings file of a platform:
///   Linux    $XDG_CONFIG_HOME/5dchess/settings.txt, else $HOME/.config/5dchess/settings.txt
///   macOS    $HOME/Library/Application Support/5DChess/settings.txt
///   Windows  %APPDATA%\5DChess\settings.txt
/// `file` names another file of the same directory (the saved games use it). `getenv` returns the variable's value or an empty string. Empty result: no usable location (no HOME / APPDATA).
std::string pathFor(Platform platform, const std::function<std::string(const char*)>& getenv, const std::string& file = "settings.txt");

} // namespace settingsfile
