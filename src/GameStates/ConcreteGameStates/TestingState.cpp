#include "gameState.h"
#include <iostream>
#include "MenuComponent.h"
#include "MenuCommand.h"
#include "Scene/ConcreteScene/TestingScene.h"
#include "Scene/SceneManager.h"
// #include "GameStates/ConcreteGameStates/MainMenuState.h"
#include "Menu/MenuItemView.h"
#include "ResourceManager.h"
#include "TestingState.h"

#include "Render/Controller.h"

// MainMenuState implementation
void TestingState::onEnter(GameStateModel* context) {
    std::cout << "Entering Testing State" << std::endl;
}

void TestingState::onExit(GameStateModel* context) {
    std::cout << "Exiting Main Menu State" << std::endl;
}

void TestingState::update(GameStateModel* context, float deltaTime) {
    // Testing state specific update logic
}

std::unique_ptr<GameState> TestingState::clone() const {
    return std::make_unique<TestingState>();
}

std::unique_ptr<Scene> TestingState::createScene() const {
    return std::make_unique<TestingScene>();
}


std::shared_ptr<MenuComponent> TestingState::createNavigationMenu(GameStateModel* gameStateModel, SceneManager* sceneManager) {
  auto mainMenu = std::make_shared<Menu>("Playing", true);
  return mainMenu;
}

std::vector<std::shared_ptr<MenuItemView>> TestingState::createNavigationMenuButtonItemViews(std::shared_ptr<MenuComponent> menu) const {
    std::vector<std::shared_ptr<MenuItemView>> itemViews;

    int activeItems = 0;
    for (const auto& child : menu->getChildren()) {
        if (child) {
            ++activeItems;
        }
    }

    return itemViews;
}
