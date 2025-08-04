#pragma once
#include <memory>
#include <stack>
#include <string>
#include <iostream>

class Scene;

class SceneManager {
public:
  SceneManager();
  ~SceneManager();

  // Scene management
  void pushScene(const std::string &sceneName);
  void popScene();
  void changeScene(const std::string &sceneName);

  // Core update loop
  void update(float deltaTime);
  void render();

  // Scene queries
  Scene* getCurrentScene() const;
  bool isEmpty() const;

  // Transition management
  // void requestTransition(const std::string& sceneName);
  // bool isTransitioning() const { return _pendingTransition; }

private:
  std::stack<std::unique_ptr<Scene>> _sceneStack;

  /*
   Name of the next scene to transition to
  */
  std::string _nextSceneName;

  /** Transition handling
   *  Handles the transition between scenes, including pushing and popping scenes
   */
  bool _pendingTransition = false;
  bool _isChangeScene = false;

  void processTransition(); // helper function to handle scene transitions -- called in update
  std::unique_ptr<Scene> createScene(const std::string &name);
};