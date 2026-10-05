#pragma once
#include "Render/PieceTheme.h"
#include <string>

// User-adjustable options. In memory only for now (no persistence); App owns the one instance and the
// services read it (ThemeManager: theme, AudioManager: music + sfx, UI::Motion: reduceMotion).
struct Settings {
  PieceTheme theme = Themes::modern;
  std::string music = "Off"; // selected track's display name, or AudioManager::offName()
  bool sfx = true;           // sound effects on
  bool reduceMotion = false;
};
