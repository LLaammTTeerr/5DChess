#pragma once
#include "Scene.h"
#include "MenuController.h" // Include the full header instead of forward declaration
#include <string>
#include <memory>

class GameState;
class GameStateModel;
class SceneManager;

class VersusScene : public Scene {
private:
  std::unique_ptr<VersusMenuController> menuController;
  std::string selectedGameMode;
  GameStateModel* gameStateModel = nullptr;
  SceneManager* sceneManager = nullptr;

public:
  VersusScene() = default;
  ~VersusScene() override = default;
  
  // Set dependencies for state communication
  void setDependencies(GameStateModel* stateModel, SceneManager* sceneMgr);

  void init(void) override;
  void handleInput(void) override;
  void update(float deltaTime) override;
  void render(void) override;
  void cleanup(void) override;

  bool isActive(void) const override;
  std::string getName(void) const override;
  std::string getGameStateName(void) const override;

  void onEnter(void) override;
  void onExit(void) override;

  bool shouldTransition(void) const override;

  // Public method for game mode selection
  void selectGameMode(const std::string& mode);

private:
  void initializeMenuController();
};