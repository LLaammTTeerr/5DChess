#include "gameState.h"
#include <iostream>
#include "MenuComponent.h"
#include "MenuCommand.h"
#include "Scene/ConcreteScene/MainMenuScene.h"
#include "Scene/ConcreteScene/VersusScene.h"
#include "Scene/SceneManager.h"
#include "GameStates/ConcreteGameStates/VersusState.h"
#include "ResourceManager.h"
#include "Menu/MenuItemView.h"



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

std::vector<std::shared_ptr<MenuItemView>> VersusState::createMenuButtonItemViews(std::shared_ptr<MenuComponent> menu) const {
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