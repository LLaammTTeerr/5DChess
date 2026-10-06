#include "Screens/PlayScreen.h"
#include "App.h"
#include "Screens/ModeSelectScreen.h"
#include "engine/GameCatalog.h"

PlayScreen::PlayScreen(const std::string& modeId)
    : _game(Chess::GameCatalog::create(modeId)), _model(_game), _view(Vector3{5000, 5000, 1}), _controller(_model, _view) {
  // Build the board views once so the first frame's input already has them to map clicks onto
  _controller.update(0.0f);
}

void PlayScreen::update(App& app, float dt) {
  // The buttons come first: whatever has the pointer is not a click on the board
  if (app.screens.navShown() && _back.update(dt)) app.screens.replace(std::make_unique<ModeSelectScreen>());
  _controller.handleInput(dt);
  _controller.update(dt);
}

void PlayScreen::draw(App& app) const {
  _controller.render();
  _back.draw(app.screens.navAlpha());
}
