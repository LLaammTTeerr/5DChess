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



std::shared_ptr<MenuComponent> VersusState::createNavigationMenu(GameStateModel* gameStateModel, SceneManager* sceneManager) {
  auto versusMenu = std::make_shared<Menu>("Versus Menu", true);

  // Always add Back button
  std::shared_ptr<MenuComponent> Back = std::make_shared<MenuItem>("Back", true);
  Back->setCommand(std::make_unique<VersusBackCommand>(gameStateModel, sceneManager)); // Set command for Back
  versusMenu->addItem(Back);

  // Only add Play button if game mode is selected
  if (gameModeSelected) {
    std::shared_ptr<MenuComponent> Play = std::make_shared<MenuItem>("Play", true);
    Play->setCommand(std::make_unique<VersusPlayCommand>(gameStateModel, sceneManager, selectedGameMode)); // Set command for Play
    versusMenu->addItem(Play);
  }
  
  return versusMenu;
}

std::unique_ptr<Scene> VersusState::createScene() const {
    auto scene = std::make_unique<VersusScene>();
    // Note: Dependencies will be set by SceneManager after scene creation
    return scene;
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
    auto cloned = std::make_unique<VersusState>();
    // Copy the current state
    cloned->gameModeSelected = this->gameModeSelected;
    cloned->selectedGameMode = this->selectedGameMode;
    cloned->menuVersion = this->menuVersion;
    return cloned;
}

// std::unique_ptr<GameState> VersusState::createState() const {
//     return std::make_unique<VersusState>();
// }

std::vector<std::shared_ptr<MenuItemView>> VersusState::createNavigationMenuButtonItemViews(std::shared_ptr<MenuComponent> menu) const {
    std::vector<std::shared_ptr<MenuItemView>> itemViews;

    int activeItems = 0;
    for (const auto& child : menu->getChildren()) {
        if (child) {
            ++activeItems;
        }
    }
   
    const float horizontalSpacing = 20.0f; // spacing between items
    const float itemHeight = 40.0f;
    const float itemWidth = 200.0f;
    const float startX = (GetScreenWidth() - activeItems * itemWidth + (activeItems - 1) * horizontalSpacing) / 2.0f;
    const float startY = 650.0f;

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

void VersusState::setGameModeSelected(bool selected, const std::string& mode) {
    gameModeSelected = selected;
    if (selected && !mode.empty()) {
        selectedGameMode = mode;
        std::cout << "VersusState: Game mode set to " << mode << std::endl;
    } else {
        selectedGameMode = "None";
        std::cout << "VersusState: Game mode cleared" << std::endl;
    }
    
    // Increment menu version to force menu refresh
    ++menuVersion;
    std::cout << "VersusState: Menu version incremented to " << menuVersion << std::endl;
}