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
  GameStateModel _gameState;
  std::unique_ptr<MenuController> _menuController;
  std::shared_ptr<MenuComponent> _menuSystem;
public:
  void init() {
    // create menu model
    _gameState.setState(GameStateModel::State::MAIN_MENU);
    _menuSystem = CompositeMenuFactory(&_gameState).createMenuSystem();
    

    _menuController = std::make_unique<MenuController>(&_gameState, _menuSystem);
    _menuController->setViewStrategy(std::make_unique<ButtonMenuView>());
  } 

  void update() {
    if (_menuController) {
      std::cout << "Updating MenuController..." << std::endl;
      _menuController->update();
    }
  }

  void render() {
    if (_menuController) {
      _menuController->draw();
    }
  }
};