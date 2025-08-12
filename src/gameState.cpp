#include "gameState.h"
#include <iostream>
#include "MenuComponent.h"
#include "MenuCommand.h"
#include "Scene/ConcreteScene/MainMenuScene.h"
#include "Scene/ConcreteScene/VersusScene.h"
#include "Scene/SceneManager.h"


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

std::shared_ptr<MenuComponent> VersusState::createMenu(GameStateModel* gameStateModel, SceneManager* sceneManager) {
    auto mainMenu = std::make_shared<Menu>("Main Menu", true);

//   std::shared_ptr<MenuComponent> Versus = std::make_shared<MenuItem>("Versus", true);
//   Versus->setCommand(std::make_unique<VersusCommand>(gameStateModel, sceneManager)); // Set command for Versus

  std::shared_ptr<MenuComponent> Puzzles = std::make_shared<MenuItem>("Puzzles", true);
  std::shared_ptr<MenuComponent> Guide = std::make_shared<MenuItem>("Guide", true);
  std::shared_ptr<MenuComponent> Settings = std::make_shared<MenuItem>("Settings", true);
  std::shared_ptr<MenuComponent> Exit = std::make_shared<MenuItem>("Exit", true); 

//   mainMenu->addItem(Versus);
  mainMenu->addItem(Puzzles);
  mainMenu->addItem(Guide);
  mainMenu->addItem(Settings);
  mainMenu->addItem(Exit);

  return mainMenu;
}

std::unique_ptr<Scene> VersusState::createScene() const {
    return std::make_unique<VersusScene>();
}

// VersusState implementation
void VersusState::onEnter(GameStateModel* context) {
    std::cout << "Entering Versus State" << std::endl;
}

void VersusState::onExit(GameStateModel* context) {
    std::cout << "Exiting Versus State" << std::endl;
}

void VersusState::update(GameStateModel* context, float deltaTime) {
    // Versus mode specific update logic
}

std::unique_ptr<GameState> VersusState::clone() const {
    return std::make_unique<VersusState>();
}

// std::unique_ptr<GameState> VersusState::createState() const {
//     return std::make_unique<VersusState>();
// }

// GameStateModel implementation
GameStateModel::GameStateModel() {
    // Initialize with Main Menu state
    _currentState = std::make_unique<MainMenuState>();
    _currentState->onEnter(this);
}

void GameStateModel::setState(std::unique_ptr<GameState> newState) {
    if (_currentState) {
        _currentState->onExit(this);
    }
    _currentState = std::move(newState);
    if (_currentState) {
        _currentState->onEnter(this);
    }
}

void GameStateModel::setStateByName(const std::string& stateName) {
    auto newState = createState(stateName);
    if (newState) {
        setState(std::move(newState));
    }
}

std::string GameStateModel::getCurrentStateName() const {
    if (_currentState) {
        return _currentState->getName();
    }
    return "NONE";
}

void GameStateModel::update(float deltaTime) {
    if (_currentState) {
        _currentState->update(this, deltaTime);
    }
}

std::unique_ptr<GameState> GameStateModel::createState(const std::string& stateName) {
    if (stateName == "MAIN_MENU") {
        return std::make_unique<MainMenuState>();
    } 
    else if (stateName == "VERSUS") {
        return std::make_unique<VersusState>();
    } 
    // else if (stateName == "PUZZLES") {
    //     return std::make_unique<PuzzlesState>();
    // } else if (stateName == "GUIDE") {
    //     return std::make_unique<GuideState>();
    // } else if (stateName == "SETTINGS_MENU") {
    //     return std::make_unique<SettingsMenuState>();
    // } else if (stateName == "IN_GAME") {
    //     return std::make_unique<InGameState>();
    // } else if (stateName == "PAUSED") {
    //     return std::make_unique<PausedState>();
    // }
    return nullptr;
}



std::shared_ptr<MenuComponent> GameStateModel::createMenuForCurrentState(SceneManager* sceneManager) {
    if (_currentState) {
        return _currentState->createMenu(this, sceneManager);
    }
    return nullptr;
}