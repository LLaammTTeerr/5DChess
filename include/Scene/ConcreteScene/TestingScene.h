#pragma once
#include "Scene.h"
#include <string>
#include "ResourceManager.h"

class TestingScene : public Scene {
public:
  TestingScene() = default;
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
};