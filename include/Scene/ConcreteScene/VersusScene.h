#pragma once
#include "Scene.h"
#include "MenuController.h" // Include the full header instead of forward declaration
#include <string>
#include <memory>

class GameState;

class VersusScene : public Scene {
private:
  std::unique_ptr<VersusMenuController> menuController;
  std::string selectedGameMode;

public:
  VersusScene() = default;
  ~VersusScene() override = default;

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