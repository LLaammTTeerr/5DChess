#pragma once
#include <memory>
#include <stack>
#include <string>
#include <iostream>
#include <raylib.h>
#include "Render/Motion.h"

// Forward declarations
class Scene;
class GameStateModel;
class NavigationMenuController;
class MenuComponent;

class SceneManager {
public:
  SceneManager(GameStateModel* gameStateModel); // Constructor with dependency injection (required)
  ~SceneManager();

  // Scene management
  void pushScene(std::unique_ptr<Scene> scene);
  void popScene();
  void changeScene(std::unique_ptr<Scene> scene);

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
  void forceMenuRefresh(); // Force navigation menu to refresh
  
  // Game state access
  GameStateModel* getGameStateModel() const { return _gameStateModel; }

  // Quit request (e.g. from the Exit menu item), honoured by the main loop
  static void requestQuit() { _quitRequested = true; }
  static bool isQuitRequested() { return _quitRequested; }

private:
  static inline bool _quitRequested = false;
  std::stack<std::unique_ptr<Scene>> _sceneStack;

  GameStateModel* _gameStateModel;
  std::shared_ptr<NavigationMenuController> _navigationMenuController;
  std::shared_ptr<MenuComponent> _navigationMenuSystem;
  bool _menuActive = false;

  // ---- Motion: scene cross-fade + nav overlay fade (visuals only, never delay state or input) ----
  UI::Motion::Tween _menuFade;     // nav overlay alpha
  float _menuAlpha = 1.0f;
  UI::Motion::Tween _sceneFade;    // 0 -> 1 progress of the cross-fade
  RenderTexture2D _snapshot{};     // outgoing scene, drawn over the new one with alpha 1 - progress
  bool _hasSnapshot = false;
  void captureSnapshot();
  void releaseSnapshot();

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
  
  // Menu system helpers
  void initializeNavigationMenuSystem();
  void updateMenuSystem(float deltaTime);
  void renderMenuSystem();
};