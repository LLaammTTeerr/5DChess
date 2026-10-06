#pragma once
#include "ui/Screen.h"

// "Select Game Mode": a scrolling list of the GameCatalog modes; once one is selected a Play button appears
// next to Back.
class ModeSelectScreen : public Screen {
public:
  ModeSelectScreen();
  void update(App& app, float dt) override;
  void draw(App& app) const override;

private:
  ui::ButtonList _modes;
  ui::Button _back, _play;
  bool _canPlay = false;

  void layoutNavRow();
};
