#pragma once
#include <vector>
#include <memory>
#include <string>
#include <iostream>

// forward declaration
class MenuComponent;
class IMenuView;
class GameStateModel;
class GameState;
class SceneManager;
class MenuController {
private:
  SceneManager* _sceneManager; 
  GameStateModel* _gameStateModel;
  std::unique_ptr<GameState> _lastState;

  std::shared_ptr<MenuComponent> _menuSystem; // root menu component

  std::shared_ptr<MenuComponent> _currentMenuModel;  // Changed to shared_ptr
  std::unique_ptr<IMenuView> _menuView;

public:
  MenuController(GameStateModel* gameStateModel, std::shared_ptr<MenuComponent> menuSystem, 
                 SceneManager* sceneManager);
  void setViewStrategy(std::unique_ptr<IMenuView> view); 
  void updateMenuForCurrentState();

  void handleInput(); // Handle user input for menu navigation
  void update();
  void draw() const;

  IMenuView* getMenuView() const;
};
