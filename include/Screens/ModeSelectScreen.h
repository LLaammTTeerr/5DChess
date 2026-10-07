#pragma once
#include "play/VsAi.h"
#include "ui/Screen.h"

// "Select Game Mode", in two columns: a Setup panel at the left (who to play: two players at this screen, or the computer; then your
// side and its level, with a few lines describing the level) and a panel with the scrolling list of the GameCatalog modes at the
// right. Once a mode is selected a Play button appears next to Back. The choices are remembered in the settings.
class ModeSelectScreen : public Screen {
public:
  ModeSelectScreen();
  void update(App& app, float dt) override;
  void draw(App& app) const override;
  void back(App& app) override; // Esc: what the Back button does

private:
  ui::ButtonList _opponent;         // Two players | Computer
  ui::ButtonList _sides, _levels;   // only with the computer
  ui::ButtonList _modes;
  ui::Button _back, _play;
  UI::Motion::Tween _vsFade;        // the side and level rows fade in
  Rectangle _setupPanel{}, _modesPanel{};  // the two columns' cards
  bool _canPlay = false;
  bool _vs = false;
  play::SideChoice _side = play::SideChoice::White;
  play::AiLevel _level = play::AiLevel::Normal;

  void layoutNavRow();
  void buildList();                 // the mode list starts lower while the side and level rows are shown
};
