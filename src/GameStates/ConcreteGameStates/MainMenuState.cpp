#include "gameState.h"
#include <iostream>
#include "MenuComponent.h"
#include "MenuCommand.h"
#include "Scene/ConcreteScene/MainMenuScene.h"
#include "Scene/ConcreteScene/VersusScene.h"
#include "Scene/SceneManager.h"
#include "GameStates/ConcreteGameStates/MainMenuState.h"

// MainMenuState implementation
void MainMenuState::onEnter(GameStateModel* context) {
    std::cout << "Entering Main Menu State" << std::endl;
}

void MainMenuState::onExit(GameStateModel* context) {
    std::cout << "Exiting Main Menu State" << std::endl;
}

void MainMenuState::update(GameStateModel* context, float deltaTime) {
    // Main menu specific update logic
}

std::unique_ptr<GameState> MainMenuState::clone() const {
    return std::make_unique<MainMenuState>();
}

std::unique_ptr<Scene> MainMenuState::createScene() const {
    return std::make_unique<MainMenuScene>();
}


std::shared_ptr<MenuComponent> MainMenuState::createMenu(GameStateModel* gameStateModel, SceneManager* sceneManager) {
  auto mainMenu = std::make_shared<Menu>("Main Menu", true);

  std::shared_ptr<MenuComponent> Versus = std::make_shared<MenuItem>("Versus", true);
  Versus->setCommand(std::make_unique<VersusCommand>(gameStateModel, sceneManager)); // Set command for Versus

  std::shared_ptr<MenuComponent> Puzzles = std::make_shared<MenuItem>("Puzzles", true);
  std::shared_ptr<MenuComponent> Guide = std::make_shared<MenuItem>("Guide", true);
  std::shared_ptr<MenuComponent> Settings = std::make_shared<MenuItem>("Settings", true);
  std::shared_ptr<MenuComponent> Exit = std::make_shared<MenuItem>("Exit", true); 

  

  mainMenu->addItem(Versus);
  mainMenu->addItem(Puzzles);
  mainMenu->addItem(Guide);
  mainMenu->addItem(Settings);
  mainMenu->addItem(Exit);

  return mainMenu;
}