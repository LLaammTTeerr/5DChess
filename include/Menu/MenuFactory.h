// #pragma once
// #include <string>
// #include <memory>
// #include <iostream>
// #include <raylib.h>
// #include "gameState.h"

// // forward declaration
// class MenuComponent;
// class SceneManager;

// class CompositeMenuFactory {
// private:
//   GameStateModel* _gameStateModel;
//   SceneManager* _sceneManager; // Optional, if you want to notify SceneManager
// public:
//   CompositeMenuFactory(GameStateModel* gameStateModel, SceneManager* sceneManager);

//   std::shared_ptr<MenuComponent> createMenuForState(GameStateModel::State state);
//   std::shared_ptr<MenuComponent> createMenuSystem(); 

// private:
//   std::shared_ptr<MenuComponent> createMainMenu(); 
//   std::shared_ptr<MenuComponent> createSettingsMenu();
//   std::shared_ptr<MenuComponent> createInGameMenu();
//   // std::shared_ptr<MenuComponent> createPausedMenu();
//   // std::shared_ptr<MenuComponent> createGameOverMenu();
// };
