#include "gameState.h"
#include <iostream>
#include "MenuComponent.h"
#include "MenuCommand.h"
#include "Scene/ConcreteScene/MainMenuScene.h"
#include "Scene/ConcreteScene/VersusScene.h"
#include "Scene/SceneManager.h"
#include "GameStates/ConcreteGameStates/MainMenuState.h"
#include "Menu/MenuItemView.h"
#include "ResourceManager.h"
// MainMenuState implementation
void MainMenuState::onEnter(GameStateModel* context) {
    std::cout << "Entering Main Menu State" << std::endl;
}

void MainMenuState::onExit(GameStateModel* context) {
    std::cout << "Exiting Main Menu State" << std::endl;
}

void MainMenuState::update(GameStateModel* context, float deltaTime) {
    // Main menu specific update logic
}

std::unique_ptr<GameState> MainMenuState::clone() const {
    return std::make_unique<MainMenuState>();
}

std::unique_ptr<Scene> MainMenuState::createScene() const {
    return std::make_unique<MainMenuScene>();
}


std::shared_ptr<MenuComponent> MainMenuState::createNavigationMenu(GameStateModel* gameStateModel, SceneManager* sceneManager) {
  auto mainMenu = std::make_shared<Menu>("Main Menu", true);

  std::shared_ptr<MenuComponent> Versus = std::make_shared<MenuItem>("Versus", true);
  Versus->setCommand(createVersusCommand(gameStateModel, sceneManager)); // Set command for Versus

  std::shared_ptr<MenuComponent> Puzzles = std::make_shared<MenuItem>("Puzzles", true);
  std::shared_ptr<MenuComponent> Guide = std::make_shared<MenuItem>("Guide", true);
  std::shared_ptr<MenuComponent> Settings = std::make_shared<MenuItem>("Settings", true);
  std::shared_ptr<MenuComponent> Exit = std::make_shared<MenuItem>("Exit", true); 

  

  mainMenu->addItem(Versus);
  mainMenu->addItem(Puzzles);
  mainMenu->addItem(Guide);
  mainMenu->addItem(Settings);
  mainMenu->addItem(Exit);

  return mainMenu;
}

std::vector<std::shared_ptr<MenuItemView>> MainMenuState::createNavigationMenuButtonItemViews(std::shared_ptr<MenuComponent> menu) const {
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
