#pragma once
#include "Scene.h"
#include <string>
#include "ResourceManager.h"
#include "chess.h"
#include "Render/View.h"
#include "Render/RenModel.h"
#include "Render/Controller.h"
class TestingScene : public Scene {
public:
  TestingScene(const std::string& gameMode);
  ~TestingScene() override = default;

  void init(void) override;
  void update(float deltaTime) override;
  void render(void) override;
  void handleInput(void) override;
  void cleanup(void) override;

  bool isActive(void) const override;
  std::string getName(void) const override;
  std::string getGameStateName(void) const override;

  void onEnter(void) override;
  void onExit(void) override;

  bool shouldTransition(void) const override;

private:
  std::shared_ptr<Chess::IGame> _game;
  std::shared_ptr<ChessModel> _chessModel;
  std::shared_ptr<ChessView> _chessView;
  std::shared_ptr<ChessController> _chessController;
  std::string _gameModeSelected;
};

