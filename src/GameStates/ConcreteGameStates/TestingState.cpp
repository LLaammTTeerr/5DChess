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
    return std::make_unique<TestingScene>(_gameModeSelected);
}


std::shared_ptr<MenuComponent> TestingState::createNavigationMenu(GameStateModel* gameStateModel, SceneManager* sceneManager) {
  auto mainMenu = std::make_shared<Menu>("Playing", true);
  auto BackToVersus = std::make_shared<MenuItem>("Back", true);
  BackToVersus->setCommand(createBackToVersusCommand(gameStateModel, sceneManager)); // Set command for Back to Versus
  
  mainMenu->addItem(BackToVersus);
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

    const float verticalSpacing = 20.0f; // spacing between items
    const float itemHeight = 40.0f;
    const float itemWidth = 200;
    const Rectangle menuArea = {0, 0, 300, (float)GetScreenHeight()}; // Example menu area

    const float startX = menuArea.x + (menuArea.width - itemWidth) / 2;
    const float startY = menuArea.y + (menuArea.height - (activeItems * itemHeight + (activeItems - 1) * verticalSpacing)) / 2;

    itemViews.reserve(activeItems); // Reserve space for active items
    for (int i = 0; i < activeItems; ++i) {
        Vector2 position = {startX, startY + i * (itemHeight + verticalSpacing)};
        Vector2 size = {itemWidth, itemHeight};
        auto itemView = std::make_shared<MenuItemView>(position, size);
        itemView->setFont(ResourceManager::getInstance().getFont("public_sans_bold"));
        itemViews.push_back(itemView);
    }
    return itemViews;
}
