#pragma once
#include <memory>
#include <stack>
#include <string>
#include <iostream>

// Forward declarations
class Scene;
class GameStateModel;
class MenuController;
class MenuComponent;

class SceneManager {
public:
  SceneManager(GameStateModel* gameStateModel); // Constructor with dependency injection (required)
  ~SceneManager();

  // Scene management
  void pushScene(std::unique_ptr<Scene> scene);
  void popScene();
  void changeScene(std::unique_ptr<Scene> scene);
  void pushSceneDeferred(std::unique_ptr<Scene> scene); // Push scene in next loop iteration

  // Core update loop
  void update(float deltaTime);
  void render();

  // Scene queries
  Scene* getCurrentScene() const;
  bool isEmpty() const;

  // Menu management
  void showMenu();
  void hideMenu();
  bool isMenuActive() const;
  void toggleMenu();
  
  // Game state access
  GameStateModel* getGameStateModel() const { return _gameStateModel; }

  // Transition management
  // void requestTransition(const std::string& sceneName);
  // bool isTransitioning() const { return _pendingTransition; }

private:
  std::stack<std::unique_ptr<Scene>> _sceneStack;

  // Menu system integration
  GameStateModel* _gameStateModel;
  std::unique_ptr<MenuController> _menuController;
  std::shared_ptr<MenuComponent> _menuSystem;
  bool _menuActive = false;

  /*
   Name of the next scene to transition to
  */
  std::unique_ptr<Scene> _nextScene;

  /** Transition handling
   *  Handles the transition between scenes, including pushing and popping scenes
   */
  bool _pendingTransition = false;
  bool _isChangeScene = false;

  void processTransition(); // helper function to handle scene transitions -- called in update
  std::unique_ptr<Scene> createScene(const std::string &name);
  
  // Menu system helpers
  void initializeMenuSystem();
  void updateMenuSystem(float deltaTime);
  void renderMenuSystem();
};