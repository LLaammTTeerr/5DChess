#include "gameState.h"
#include <iostream>
#include "MenuComponent.h"
#include "MenuCommand.h"
#include "Scene/ConcreteScene/SettingsScene.h"
#include "Scene/SceneManager.h"
#include "GameStates/ConcreteGameStates/SettingsState.h"
#include "Menu/MenuItemView.h"
#include "ResourceManager.h"
#include "PieceTheme.h"

// SettingsState implementation
void SettingsState::onEnter(GameStateModel* context) {
    std::cout << "Entering Settings State" << std::endl;
}

void SettingsState::onExit(GameStateModel* context) {
    std::cout << "Exiting Settings State" << std::endl;
}

void SettingsState::update(GameStateModel* context, float deltaTime) {
    // Settings specific update logic
}

std::unique_ptr<GameState> SettingsState::clone() const {
    return std::make_unique<SettingsState>();
}

std::unique_ptr<Scene> SettingsState::createScene() const {
    return std::make_unique<SettingsScene>();
}

std::shared_ptr<MenuComponent> SettingsState::createNavigationMenu(GameStateModel* gameStateModel, SceneManager* sceneManager) {
    auto settingsMenu = std::make_shared<Menu>("Settings Menu", true);

    // Back button
    std::shared_ptr<MenuComponent> Back = std::make_shared<MenuItem>("Back", true);
    Back->setCommand(createSettingsBackCommand(gameStateModel, sceneManager));
    settingsMenu->addItem(Back);

    return settingsMenu;
}

std::vector<std::shared_ptr<MenuItemView>> SettingsState::createNavigationMenuButtonItemViews(std::shared_ptr<MenuComponent> menu) const {
    std::vector<std::shared_ptr<MenuItemView>> itemViews;

    int activeItems = 0;
    for (const auto& child : menu->getChildren()) {
        if (child) {
            ++activeItems;
        }
    }

    const float verticalSpacing = UI::Space::md;
    const float itemHeight = UI::Space::buttonHeight;
    const float itemWidth = 200;
    const Rectangle menuArea = {0, 0, 250, 50};

    const float startX = menuArea.x + (menuArea.width - itemWidth) / 2;
    const float startY = menuArea.y + (menuArea.height - (activeItems * itemHeight + (activeItems - 1) * verticalSpacing)) / 2;

    itemViews.reserve(activeItems);
    for (int i = 0; i < activeItems; ++i) {
        Vector2 position = {startX, startY + i * (itemHeight + verticalSpacing)};
        Vector2 size = {itemWidth, itemHeight};
        auto itemView = std::make_shared<MenuItemView>(position, size);
        itemViews.push_back(itemView);
    }
    return itemViews;
}

void SettingsState::setMusic(const std::string& music) {
    selectedMusic = music;
    
    // Playback itself is global (AudioManager, called by MusicSelectCommand); this keeps the scene-local copy.
    std::cout << "Music changed to: " << music << std::endl;
    ++menuVersion;
}
