#pragma once
#include "Scene.h"
#include <string>
#include "Render/Motion.h"

class MainMenuScene : public Scene {
public:
  MainMenuScene() = default;
  ~MainMenuScene() override = default;

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

private:
  float _time = 0.0f;        // drives the field drift, bob and blink (frozen under Reduce motion)
  float _enterClock = 0.0f;  // seconds since entering (creature stagger)
  UI::Motion::Tween _enter;  // title / subtitle entrance
};