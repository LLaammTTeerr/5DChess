#pragma once
#include <vector>
#include <memory>
#include <string>
#include <iostream>
#include "gameState.h"
#include "MenuFactory.h"
// forward declaration

class MenuComponent;
class IMenuView;

class MenuController {
private:
  GameStateModel* _gameState;
  GameStateModel::State _lastState;

  std::shared_ptr<MenuComponent> _menuSystem;

  std::shared_ptr<MenuComponent> _currentMenuModel;  // Changed to shared_ptr
  std::unique_ptr<IMenuView> _menuView;
  CompositeMenuFactory _menuFactory;

public:
  MenuController(GameStateModel* gameState, std::shared_ptr<MenuComponent> menuSystem);
  void setViewStrategy(std::unique_ptr<IMenuView> view); 
  void updateMenuForCurrentState();
  // void updateSettingsDisplay();
  void handleInput(); // Handle user input for menu navigation

  void update();

  void draw() const;

  IMenuView* getMenuView() const;
};
