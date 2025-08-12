#pragma once
#include "Scene.h"
#include <string>

class GameState;

class VersusScene : public Scene {
public:
  VersusScene() = default;
  ~VersusScene() override = default;

  void init(void) override;
  void update(float deltaTime) override;
  void render(void) override;
  void cleanup(void) override;

  bool isActive(void) const override;
  std::string getName(void) const override;
  std::string getGameStateName(void) const override;

  void onEnter(void) override;
  void onExit(void) override;

  bool shouldTransition(void) const override;

  void loadResources(void) override;
  void unloadResources(void) override;
};