#include "MenuView.h"
#include "MenuComponent.h"
#include "gameState.h"
#include "MenuItemView.h"
#include "ResourceManager.h"


void ButtonMenuView::createNavigationItemViews(std::shared_ptr<MenuComponent> menuModel, GameState* gameState) {
    _itemViews.clear(); // Clear existing item views

    if (gameState == nullptr) {
        for (const auto& child : menuModel->getChildren()) {
            if (child) {
                Vector2 position = { 100, static_cast<float>(_itemViews.size() * 50 + 100) }; // Example positioning
                Vector2 size = { 200, 40 }; // Example size
                auto itemView = std::make_shared<MenuItemView>(position, size);
                itemView->setFont(ResourceManager::getInstance().getFont("public_sans_bold"));
                _itemViews.push_back(itemView);
            }
        }
    }
    else {
        _itemViews.clear(); // Clear existing item views
        // Create item views based on the game state
        auto itemViews = gameState->createNavigationMenuButtonItemViews(menuModel);
        for (const auto& itemView : itemViews) {
            if (itemView) {
                _itemViews.push_back(itemView);
            }
        }
    }
}

void ButtonMenuView::draw(std::shared_ptr<MenuComponent> menuModel) const {
    const auto& menuItems = menuModel->getChildren();
    // Draw each item view
    for (size_t i = 0; i < _itemViews.size() && i < menuItems.size(); ++i) {
        if (_itemViews[i] && menuItems[i] -> isEnabled()) {
            _itemViews[i] -> draw(menuItems[i]);
        }
    }
}

void ButtonMenuView::createInGameItemsViews(int numberOfItems) {
    _itemViews.clear(); // Clear existing item views

    const float horizontalSpacing = 20.0f; // spacing between items
    const float itemHeight = 30.0f;
    const float itemWidth = 150.0f;
    const Rectangle menuArea = {0, 0, (float)GetScreenWidth(), 100.0f}; // Example menu area

    const float startX = menuArea.x + (menuArea.width - numberOfItems * itemWidth - (numberOfItems - 1) * horizontalSpacing) / 2;
    const float startY = menuArea.y + (menuArea.height - itemHeight) / 2;

    _itemViews.reserve(numberOfItems); // Reserve space for the specified number of items
    for (int i = 0; i < numberOfItems; ++i) {
        Vector2 position = {startX + i * (itemWidth + horizontalSpacing), startY};
        Vector2 size = {itemWidth, itemHeight};
        auto itemView = std::make_shared<MenuItemView>(position, size);
        itemView->setFont(ResourceManager::getInstance().getFont("public_sans_bold"));
        _itemViews.push_back(itemView);
    }
}