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


std::shared_ptr<MenuComponent> TestingState::createMenu(GameStateModel* gameStateModel, SceneManager* sceneManager) {
  auto mainMenu = std::make_shared<Menu>("Playing", true);

  std::shared_ptr<MenuComponent> UndoMove = std::make_shared<MenuItem>("Undo", true);

  std::shared_ptr<MenuComponent> Deselect = std::make_shared<MenuItem>("Deselect", true);

  std::shared_ptr<MenuComponent> SubmitMove = std::make_shared<MenuItem>("Submit", true);

  mainMenu->addItem(UndoMove);
  mainMenu->addItem(Deselect);
  mainMenu->addItem(SubmitMove);
  // std::shared_ptr<MenuComponent> Versus = std::make_shared<MenuItem>("Versus", true);
  // Versus->setCommand(std::make_unique<VersusCommand>(gameStateModel, sceneManager)); // Set command for Versus

  // std::shared_ptr<MenuComponent> Puzzles = std::make_shared<MenuItem>("Puzzles", true);
  // std::shared_ptr<MenuComponent> Guide = std::make_shared<MenuItem>("Guide", true);
  // std::shared_ptr<MenuComponent> Settings = std::make_shared<MenuItem>("Settings", true);
  // std::shared_ptr<MenuComponent> Exit = std::make_shared<MenuItem>("Exit", true); 

  

  // mainMenu->addItem(Versus);
  // mainMenu->addItem(Puzzles);
  // mainMenu->addItem(Guide);
  // mainMenu->addItem(Settings);
  // mainMenu->addItem(Exit);

  return mainMenu;
  // return nullptr;
}

std::vector<std::shared_ptr<MenuItemView>> TestingState::createMenuButtonItemViews(std::shared_ptr<MenuComponent> menu) const {
    std::vector<std::shared_ptr<MenuItemView>> itemViews;

    int activeItems = 0;
    for (const auto& child : menu->getChildren()) {
        if (child) {
            ++activeItems;
        }
    }

    const float horizontalSpacing = 20.0f; // spacing between items
    const float itemHeight = 30.0f;
    const float itemWidth = 100.0f;
    const Rectangle menuArea = {0, 0, (float)GetScreenWidth(), 100.0f}; // Example menu area
    const float startX = menuArea.x + (menuArea.width - (activeItems * itemWidth + (activeItems - 1) * horizontalSpacing)) / 2;
    const float startY = menuArea.y + (menuArea.height - itemHeight) / 2;
    // const float verticalSpacing = 20.0f; // spacing between items
    // const float itemHeight = 40.0f;
    // const float itemWidth = 200;
    // const Rectangle menuArea = {0, 0, 300, (float)GetScreenHeight()}; // Example menu area

    // const float startX = menuArea.x + (menuArea.width - itemWidth) / 2;
    // const float startY = menuArea.y + (menuArea.height - (activeItems * itemHeight + (activeItems - 1) * verticalSpacing)) / 2;

    itemViews.reserve(activeItems); // Reserve space for active items
    for (int i = 0; i < activeItems; ++i) {
        Vector2 position = {startX + i * (itemWidth + horizontalSpacing), startY};
        Vector2 size = {itemWidth, itemHeight};
        auto itemView = std::make_shared<MenuItemView>(position, size);
        itemView->setFont(ResourceManager::getInstance().getFont("public_sans_bold"));
        itemViews.push_back(itemView);
    }
    return itemViews;
}
