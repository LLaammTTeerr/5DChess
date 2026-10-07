#pragma once
#include <optional>
#include <string>
#include <vector>
#include "ui/Screen.h"

// Settings: three tabs (Piece Theme, Music, Display), each showing its options below.
class SettingsScreen : public Screen {
public:
  SettingsScreen();
  void update(App& app, float dt) override;
  void draw(App& app) const override;
  void back(App& app) override; // Esc: what the Back button does

private:
  enum Tab { Theme, Music, Display };

  ui::ButtonList _tabs;
  ui::Button _back = backButton();
  int _tab = -1;                    // -1: none opened yet
  int _themeIndex = -1;             // highlighted theme (the one in use, found when the tab first opens)
  ui::ButtonList _options;          // theme names or music tracks
  std::optional<ui::Toggle> _toggle;  // Sound effects (Music tab) or Motion (Display tab)
  std::optional<ui::Cycle> _boardView; // Display tab: Board view: Deep space / Atlas / Blueprint

  mutable std::string _noteText;               // the description shown at the right and its wrapped lines (draw() caches them)
  mutable std::vector<std::string> _noteLines;

  void openTab(App& app, int tab);
};
