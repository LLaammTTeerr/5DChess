#include "SceneManager.h"
#include "SceneFactoryManager.h"
#include <iostream>
#include <cassert>
#include "Scene.h"
#include "WelcomeScene.h"
SceneManager::SceneManager() {
  // pushScene("WelcomeScene"); // Default scene
}

SceneManager::~SceneManager() {
  while (!isEmpty()) {
    popScene();
  }
}


void SceneManager::pushScene(const std::string &sceneName) {
  // Create the scene 
  auto newScene = createScene(sceneName);
  
  // Check if the scene was created successfully
  if (newScene) {
    _sceneStack.emplace(std::move(newScene));
    auto &entry = _sceneStack.top();
    entry->loadResources();
    entry->init();
    entry->onEnter();
  }
}

void SceneManager::popScene() {
  if (!_sceneStack.empty()) {
    auto &currentEntry = _sceneStack.top();
    currentEntry->onExit();
    currentEntry->cleanup();
    currentEntry->unloadResources();
    _sceneStack.pop();
  }
}

void SceneManager::changeScene(const std::string &sceneName) {
  // just update the next scene name and set flags
  _nextSceneName = sceneName;
  _isChangeScene = true;
  _pendingTransition = true;
}


void SceneManager::update(float deltaTime) {
  if (_pendingTransition) {
    processTransition();
  } 
  else if (!_sceneStack.empty()) {
    auto &currentEntry = _sceneStack.top();
    if (currentEntry->isActive()) {
      currentEntry->update(deltaTime);
    }
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
}

Scene* SceneManager::getCurrentScene() const {
  if (!_sceneStack.empty()) {
    return _sceneStack.top().get();
  }
  return nullptr;
}

std::unique_ptr<Scene> SceneManager::createScene(const std::string &name) {
  return SceneFactoryManager::getInstance().createScene(name);
}

void SceneManager::processTransition() {
  assert(_pendingTransition && !_nextSceneName.empty());

  if (_isChangeScene) {
    popScene(); // Pop current scene if changing
    _isChangeScene = false;
  } 
  
  // Create and push the new scene
  pushScene(_nextSceneName);
  _nextSceneName.clear(); // Clear after processing
  _pendingTransition = false;
}


bool SceneManager::isEmpty() const {
  return _sceneStack.empty();
}