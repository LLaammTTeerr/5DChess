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

    const float verticalSpacing = 20.0f;
    const float itemHeight = 40.0f;
    const float itemWidth = 200;
    const Rectangle menuArea = {0, 0, 250, 50};

    const float startX = menuArea.x + (menuArea.width - itemWidth) / 2;
    const float startY = menuArea.y + (menuArea.height - (activeItems * itemHeight + (activeItems - 1) * verticalSpacing)) / 2;

    itemViews.reserve(activeItems);
    for (int i = 0; i < activeItems; ++i) {
        Vector2 position = {startX, startY + i * (itemHeight + verticalSpacing)};
        Vector2 size = {itemWidth, itemHeight};
        auto itemView = std::make_shared<MenuItemView>(position, size);
        itemView->setFont(ResourceManager::getInstance().getFont("public_sans_bold"));
        itemViews.push_back(itemView);
    }
    return itemViews;
}

void SettingsState::setTheme(const std::string& theme) {
    selectedTheme = theme;
    
    // Apply the theme change
    if (theme == "Classic") {
        ThemeManager::getInstance().setTheme(std::make_unique<ClassicTheme>());
    } else if (theme == "Modern") {
        ThemeManager::getInstance().setTheme(std::make_unique<ModernTheme>());
    } else if (theme == "Modern2") {
        ThemeManager::getInstance().setTheme(std::make_unique<Modern2Theme>());
    }
    
    std::cout << "Theme changed to: " << theme << std::endl;
    ++menuVersion;
}

void SettingsState::setMusic(const std::string& music) {
    selectedMusic = music;
    
    // TODO: Implement music change logic here
    // For now, just log the change
    std::cout << "Music changed to: " << music << std::endl;
    ++menuVersion;
}
