#pragma once
#include "Render/PieceThemes.h"
#include "play/BoardView.h"
#include "play/VsAi.h"
#include <cstring>
#include <string>

// User-adjustable options. App owns the one instance and the services read it (ThemeManager: theme, AudioManager:
// music + sfx, UI::Motion: reduceMotion, PlayScreen: boardView). Persisted by services/SettingsStore.h.
struct Settings {
  PieceTheme theme = Themes::pixel;
  BoardView boardView = BoardView::Atlas;
  std::string music = "Off"; // selected track's display name, or AudioManager::offName()
  bool sfx = true;           // sound effects on
  bool reduceMotion = false;
  bool startInPlayView = false; // a game opens (and every turn starts) in the Play view instead of the multiverse (docs/CAMERA.md)
  // The last choices on the Versus screen
  bool vsComputer = false;
  play::SideChoice vsSide = play::SideChoice::White;
  play::AiLevel vsLevel = play::AiLevel::Normal;

  bool operator==(const Settings& o) const {
    return std::strcmp(theme.prefix, o.theme.prefix) == 0 && boardView == o.boardView && music == o.music && sfx == o.sfx &&
           reduceMotion == o.reduceMotion && startInPlayView == o.startInPlayView && vsComputer == o.vsComputer && vsSide == o.vsSide && vsLevel == o.vsLevel;
  }
};
