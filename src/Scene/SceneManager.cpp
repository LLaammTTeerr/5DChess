#include "SceneManager.h"
#include "Scene.h"
#include "MainMenuScene.h"
#include "VersusScene.h"
#include "gameState.h"
#include "MenuController.h"
#include "MenuView.h"
#include "MenuItemView.h"
#include "Render/UITheme.h"

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
  initializeNavigationMenuSystem();
  pushScene(std::make_unique<MainMenuScene>()); // Default scene
  _menuActive = true; // Show menu by default
}

SceneManager::~SceneManager() {
  releaseSnapshot();
  while (!isEmpty()) {
    popScene();
  }
}


void SceneManager::pushScene(std::unique_ptr<Scene> scene) {
  // Check if the scene was created successfully
  if (scene) {
    // Inject dependencies for VersusScene
    if (auto* versusScene = dynamic_cast<VersusScene*>(scene.get())) {
      versusScene->setDependencies(_gameStateModel, this);
    }
    
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


void SceneManager::update(float deltaTime) {
  // Visual-only fades advance first; a click while the cross-fade runs finishes it instantly
  // That click is consumed: it must not also reach the new scene.
  bool clickConsumed = false;
  if (_hasSnapshot && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) { _sceneFade.finish(); clickConsumed = true; }
  _sceneFade.update(deltaTime);
  if (_hasSnapshot && _sceneFade.done()) releaseSnapshot();
  _menuFade.update(deltaTime);
  _menuAlpha = _menuFade.active ? _menuFade.value() : (_menuActive ? 1.0f : 0.0f);

  // Handle scene transitions
  if (_pendingTransition) {
    processTransition();
  } 
  else if (!_sceneStack.empty()) {
    auto &currentEntry = _sceneStack.top();
    if (currentEntry->isActive()) {
      // The navigation menu is drawn on top: don't let clicks on it reach the scene below
      bool mouseOverMenu = _menuActive && _navigationMenuController && _navigationMenuController->isMouseOverMenu();
      if (clickConsumed) {
        currentEntry->onPointerBlocked();
      } else if (!mouseOverMenu) {
        currentEntry->handleInput();
      } else {
        currentEntry->onPointerBlocked();
      }
      currentEntry->update(deltaTime);
    }
  }
  
  // Update menu system if active
  if (_menuActive && _navigationMenuController) {
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

  // Outgoing scene fades out on top of the new one (the new scene already takes input)
  if (_hasSnapshot) {
    const float a = 1.0f - _sceneFade.progress();
    if (a > 0.003f) {
      const Texture2D& t = _snapshot.texture;
      DrawTextureRec(t, {0, 0, static_cast<float>(t.width), -static_cast<float>(t.height)}, {0, 0},
                     UI::withAlpha(WHITE, static_cast<unsigned char>(255.0f * a)));
    }
  }
  
  // Render menu overlay (fades in/out on toggle)
  if (_menuAlpha > 0.003f && _navigationMenuController) {
    MenuItemView::setGlobalAlpha(_menuAlpha);
    renderMenuSystem();
    MenuItemView::setGlobalAlpha(1.0f);
  }
}

void SceneManager::captureSnapshot() {
  releaseSnapshot();
  {
    if (_sceneStack.empty() || !_sceneStack.top()->isActive()) return;
    const int w = GetScreenWidth(), h = GetScreenHeight();
    _snapshot = LoadRenderTexture(w, h);
    if (_snapshot.id == 0) return;
    BeginTextureMode(_snapshot);
    _sceneStack.top()->render();
    EndTextureMode();
    _hasSnapshot = true;
  }
}

void SceneManager::releaseSnapshot() {
  if (_hasSnapshot) UnloadRenderTexture(_snapshot);
  _snapshot = RenderTexture2D{};
  _hasSnapshot = false;
}

Scene* SceneManager::getCurrentScene() const {
  if (!_sceneStack.empty()) {
    return _sceneStack.top().get();
  }
  return nullptr;
}

void SceneManager::processTransition() {
  assert(_pendingTransition && _nextScene != nullptr);

  std::string sceneName = _nextScene->getName(); // Store name before moving

  captureSnapshot(); // cross-fade from what is on screen now
  if (_isChangeScene) {
    popScene(); // Pop current scene if changing
    _isChangeScene = false;
  } 
  
  // Create and push the new scene
  pushScene(std::move(_nextScene));
  _nextScene = nullptr; // Clear after processing (now safe to uncomment)
  _pendingTransition = false;
  if (_hasSnapshot) _sceneFade.start(0.0f, 1.0f, UI::Motion::base, UI::Motion::easeOutCubic, 0.0f, true);
}


bool SceneManager::isEmpty() const {
  return _sceneStack.empty();
}

// // Menu management methods
void SceneManager::showMenu() {
  _menuActive = true;
  _menuFade.start(_menuAlpha, 1.0f, UI::Motion::base, UI::Motion::easeOutCubic, 0.0f, true);
  std::cout << "Menu activated" << std::endl;
}

void SceneManager::hideMenu() {
  _menuActive = false;
  _menuFade.start(_menuAlpha, 0.0f, UI::Motion::exitDuration(UI::Motion::base), UI::Motion::easeInCubic, 0.0f, true);
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

void SceneManager::forceMenuRefresh() {
  if (_navigationMenuController) {
    std::cout << "SceneManager: Forcing navigation menu refresh" << std::endl;
    _navigationMenuController->updateNavigationMenuForCurrentState();
  }
}

// // Menu system initialization and management
void SceneManager::initializeNavigationMenuSystem() {
  // GameState should be provided via constructor
  if (!_gameStateModel) {
    std::cerr << "Error: GameState not available for menu system initialization" << std::endl;
    return;
  }

  _gameStateModel->setStateByName("MAIN_MENU");


  _navigationMenuSystem = _gameStateModel->createNavigationMenuForCurrentState(this);
  // Create menu controller
  _navigationMenuController = std::make_shared<NavigationMenuController>(_gameStateModel, _navigationMenuSystem, this);

  // Set default view strategy (can be changed later)
  _navigationMenuController->setViewStrategy(std::make_unique<ButtonMenuView>());
  
  std::cout << "Menu system initialized" << std::endl;
}

void SceneManager::updateMenuSystem(float deltaTime) {
  if (_navigationMenuController) {
    _navigationMenuController->update();
  }
}



void SceneManager::renderMenuSystem() {
  if (_navigationMenuController) {
    // Render menu
    _navigationMenuController->draw();
  }
}
