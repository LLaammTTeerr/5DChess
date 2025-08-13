#include "gameState.h"
#include <iostream>
#include "MenuComponent.h"
#include "MenuCommand.h"
#include "Scene/ConcreteScene/MainMenuScene.h"
#include "Scene/ConcreteScene/VersusScene.h"
#include "Scene/SceneManager.h"
#include "GameStates/ConcreteGameStates/VersusState.h"


std::shared_ptr<MenuComponent> VersusState::createMenu(GameStateModel* gameStateModel, SceneManager* sceneManager) {
  auto versusMenu = std::make_shared<Menu>("Versus Menu", true);

  std::shared_ptr<MenuComponent> Back = std::make_shared<MenuItem>("Back", true);
  Back->setCommand(std::make_unique<VersusBackCommand>(gameStateModel, sceneManager)); // Set command for Back
  versusMenu->addItem(Back);

  return versusMenu;
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