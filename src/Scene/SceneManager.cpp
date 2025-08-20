#include "SceneManager.h"
#include "Scene.h"
#include "MainMenuScene.h"
#include "gameState.h"
#include "MenuController.h"
#include "MenuView.h"

#include <raylib.h>
#include <iostream>
#include <cassert>

SceneManager::SceneManager(GameStateModel* gameStateModel) 
  : _gameStateModel(gameStateModel) {
  // Constructor with dependency injection (gameState is required)
  if (!_gameStateModel) {
    std::cerr << "Error: GameState is required for SceneManager" << std::endl;
    // You might want to throw an exception here
    return;
  }
  initializeMenuSystem();
  pushScene(std::make_unique<MainMenuScene>()); // Default scene
  _menuActive = true; // Show menu by default
}

SceneManager::~SceneManager() {
  while (!isEmpty()) {
    popScene();
  }
}


void SceneManager::pushScene(std::unique_ptr<Scene> scene) {
  // Check if the scene was created successfully
  if (scene) {
    // std::cout << "Pushing scene: " << scene->getName() << std::endl;
    _sceneStack.emplace(std::move(scene));
    auto &entry = _sceneStack.top();
    entry->init();
    entry->onEnter();
    std::cout << "Scene pushed: " << entry->getName() << std::endl;
  }
}

void SceneManager::popScene() {
  if (!_sceneStack.empty()) {
    auto &currentEntry = _sceneStack.top();
    currentEntry->onExit();
    currentEntry->cleanup();
    _sceneStack.pop();
  }
}

void SceneManager::changeScene(std::unique_ptr<Scene> scene) {
  // just update the next scene name and set flags
  _nextScene = std::move(scene);
  _isChangeScene = true;
  _pendingTransition = true;
}

void SceneManager::pushSceneDeferred(std::unique_ptr<Scene> scene) {
  // Set up deferred push - will push new scene on top of current scene in next loop
  _nextScene = std::move(scene);
  _isChangeScene = false; // Don't pop current scene
  _pendingTransition = true;
  std::cout << "Deferred push scene: " << _nextScene->getName() << std::endl;
}


void SceneManager::update(float deltaTime) {
  // Handle scene transitions
  if (_pendingTransition) {
    processTransition();
  } 
  else if (!_sceneStack.empty()) {
    auto &currentEntry = _sceneStack.top();
    if (currentEntry->isActive()) {
      currentEntry->handleInput();
      currentEntry->update(deltaTime);
    }
  }
  
  // Update menu system if active
  if (_menuActive && _menuController) {
    updateMenuSystem(deltaTime);
  }
  
  // Handle menu toggle (ESC key)
  if (IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ESCAPE)) {
    toggleMenu();
  }
}

void SceneManager::render() {
  if (!_sceneStack.empty()) {
    auto &currentEntry = _sceneStack.top();
    if (currentEntry->isActive()) {
      currentEntry->render();
    }
  } else {
    std::cerr << "Scene stack is empty, nothing to render." << std::endl;
  }
  
  // Render menu overlay if active
  if (_menuActive && _menuController) {
    renderMenuSystem();
  }
}

Scene* SceneManager::getCurrentScene() const {
  if (!_sceneStack.empty()) {
    return _sceneStack.top().get();
  }
  return nullptr;
}

std::unique_ptr<Scene> SceneManager::createScene(const std::string &name) {
  return nullptr;
}

void SceneManager::processTransition() {
  assert(_pendingTransition && _nextScene != nullptr);

  std::string sceneName = _nextScene->getName(); // Store name before moving
  // std::cout << "Processing transition to scene: " << sceneName << std::endl;

  if (_isChangeScene) {
    popScene(); // Pop current scene if changing
    _isChangeScene = false;
  } 
  
  // Create and push the new scene
  pushScene(std::move(_nextScene));
  // std::cout << "Transitioning to scene: " << sceneName << std::endl;
  _nextScene = nullptr; // Clear after processing (now safe to uncomment)
  _pendingTransition = false;
}


bool SceneManager::isEmpty() const {
  return _sceneStack.empty();
}

// // Menu management methods
void SceneManager::showMenu() {
  _menuActive = true;
  std::cout << "Menu activated" << std::endl;
}

void SceneManager::hideMenu() {
  _menuActive = false;
  std::cout << "Menu deactivated" << std::endl;
}

bool SceneManager::isMenuActive() const {
  return _menuActive;
}

void SceneManager::toggleMenu() {
  if (_menuActive) {
    hideMenu();
  } else {
    showMenu();
  }
}

// // Menu system initialization and management
void SceneManager::initializeMenuSystem() {
  // GameState should be provided via constructor
  if (!_gameStateModel) {
    std::cerr << "Error: GameState not available for menu system initialization" << std::endl;
    return;
  }

  _gameStateModel->setStateByName("MAIN_MENU");
  
  _menuSystem = _gameStateModel->createMenuForCurrentState(this);
   
  // Create menu controller
  _menuController = std::make_shared<MenuController>(_gameStateModel, _menuSystem, this);

  // Set default view strategy (can be changed later)
  _menuController->setViewStrategy(std::make_unique<ButtonMenuView>());
  
  std::cout << "Menu system initialized" << std::endl;
}

void SceneManager::updateMenuSystem(float deltaTime) {
  if (_menuController) {
    _menuController->update();
  }

}

void SceneManager::renderMenuSystem() {
  if (_menuController) {
    // Render menu
    _menuController->draw();
  }
}
