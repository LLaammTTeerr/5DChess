#pragma once
#include <vector>
#include <memory>
#include <string>
#include <iostream>
#include "MenuView.h" // Include the full header for IMenuView

// forward declaration
class MenuComponent;
class GameStateModel;
class GameState;
class SceneManager;
class ChessView;
class ChessModel;

class MenuController {
public:
  virtual ~MenuController() = default;
  
  // Set the view strategy for the menu and create item views immediately
  virtual void setViewStrategy(std::unique_ptr<IMenuView> view) = 0;
  virtual void setCurrentMenu(std::shared_ptr<MenuComponent> menu) { _currentMenuModel = menu; }

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

class InGameMenuController : public MenuController {
private:
  ChessModel* _chessModel;
  ChessView* _chessView;
public:
  InGameMenuController(ChessModel* chessModel, ChessView* chessView, std::shared_ptr<MenuComponent> menuSystem);
  void setViewStrategy(std::unique_ptr<IMenuView> view) override;
  void handleInput() override; // Handle user input for in-game menu navigation
  void update() override; // Update the in-game menu controller
  void draw() const override; // Draw the in-game menu
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

class SettingMenuController : public MenuController {
public:
  SettingMenuController(std::shared_ptr<MenuComponent> menuSystem);
  void setViewStrategy(std::unique_ptr<IMenuView> view); 
  void updateNavigationMenuForCurrentState();

  void handleInput(); // Handle user input for menu navigation
  void update();
  void draw() const;

  IMenuView* getMenuView() const;

private:
  int _selectedSettingIndex = -1; // Track selected setting index
private:
  std::shared_ptr<MenuComponent> _workerMenu = nullptr;
  std::shared_ptr<ButtonMenuView> _workerMenuView = nullptr;
  void computeWorkerMenuView();
};

class VersusScene; // Forward declaration

class VersusMenuController : public MenuController {
private:
  VersusScene* _versusScene;
  int _selectedGameModeIndex = -1; // Track selected game mode index
  
public:
  VersusMenuController(VersusScene* versusScene);
  void setViewStrategy(std::unique_ptr<IMenuView> view) override;
  void handleInput() override;
  void update() override;
  void draw() const override;
  
  // Specific methods for VersusScene menu
  void createGameModeMenu();
  void selectGameMode(const std::string& mode);
  
private:
  std::string _selectedGameMode = "None";
};
