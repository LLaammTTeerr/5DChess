#pragma once
#include <string>
#include <memory>
#include <iostream>
#include <raylib.h>


#include "gameState.h" 
#include "MenuComponent.h"
#include "MenuFactory.h"
#include "MenuView.h"
#include "MenuController.h"

class TestApp {
private:
  GameStateModel* _gameState;
  std::unique_ptr<NavigationMenuController> _menuController;
  std::shared_ptr<MenuComponent> _menuSystem;
public:
  TestApp(GameStateModel* gameState) : _gameState(gameState) {}
  
  void init() {
    if (!_gameState) {
      std::cerr << "Error: GameState not provided to TestApp" << std::endl;
      return;
    }
    
    // create menu model
    _gameState->setState(GameStateModel::State::MAIN_MENU);
    _menuSystem = CompositeMenuFactory(_gameState).createMenuSystem();
    

    _menuController = std::make_unique<NavigationMenuController>(_gameState, _menuSystem);
    _menuController->setViewStrategy(std::make_unique<ButtonMenuView>());
  } 

  void update() {
    if (_menuController) {
      _menuController->update();
    }
  }

  void render() {
    if (_menuController) {
      _menuController->draw();
    }
  }
};