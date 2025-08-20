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
public:
  virtual ~MenuController() = default;
  
  // Set the view strategy for the menu and create item views immediately
  virtual void setViewStrategy(std::unique_ptr<IMenuView> view) = 0;

  // Handle user input for menu navigation
  virtual void handleInput() = 0;

  // Update the menu controller
  virtual void update() = 0;

  // Draw the menu
  virtual void draw() const = 0;

protected:
  std::shared_ptr<MenuComponent> _menuSystem; // root menu component
  std::shared_ptr<MenuComponent> _currentMenuModel; 
  std::unique_ptr<IMenuView> _menuView;
};
class NavigationMenuController : public MenuController {
private:
  SceneManager* _sceneManager; 
  GameStateModel* _gameStateModel;
  std::unique_ptr<GameState> _lastState;

public:
  NavigationMenuController(GameStateModel* gameStateModel, std::shared_ptr<MenuComponent> menuSystem, 
                 SceneManager* sceneManager);
  void setViewStrategy(std::unique_ptr<IMenuView> view); 
  void updateNavigationMenuForCurrentState();

  void handleInput(); // Handle user input for menu navigation
  void update();
  void draw() const;

  IMenuView* getMenuView() const;
};
