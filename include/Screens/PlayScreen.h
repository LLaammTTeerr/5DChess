#pragma once
#include <memory>
#include <string>
#include "chess.h"
#include "Render/Controller.h"
#include "Render/RenModel.h"
#include "Render/View.h"
#include "ui/Screen.h"

// The game: wraps the ChessModel / ChessView / ChessController triple of one game mode (by GameCatalog id).
class PlayScreen : public Screen {
public:
  explicit PlayScreen(const std::string& modeId);
  void update(App& app, float dt) override;
  void draw(App& app) const override;

private:
  std::shared_ptr<Chess::IGame> _game;
  ChessModel _model;
  ChessView _view;
  ChessController _controller;
  ui::Button _back = backButton();
};
