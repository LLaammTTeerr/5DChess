#pragma once
#include <string>
#include <memory>
#include <iostream>
#include <raylib.h>
#include "gameState.h"

// forward declaration
class MenuComponent;

class CompositeMenuFactory {
private:
  GameStateModel* gameState;
public:
  CompositeMenuFactory(GameStateModel* state);

  void activateMenu(std::shared_ptr<MenuComponent> root, const std::string& title);
  std::shared_ptr<MenuComponent> createMenuForState(GameStateModel::State state);
  std::shared_ptr<MenuComponent> createMenuSystem(); 

private:
  std::shared_ptr<MenuComponent> createMainMenu(); 
  std::shared_ptr<MenuComponent> createSettingsMenu();
  std::shared_ptr<MenuComponent> createInGameMenu();
  // std::shared_ptr<MenuComponent> createPausedMenu();
  // std::shared_ptr<MenuComponent> createGameOverMenu();
};