#include "MenuView.h"
#include "MenuComponent.h"
#include "gameState.h"
#include "MenuItemView.h"



void ButtonMenuView::createItemViews(std::shared_ptr<MenuComponent> menuModel, GameState* gameState) {
    _itemViews.clear(); // Clear existing item views

    if (gameState == nullptr) {
        for (const auto& child : menuModel->getChildren()) {
            if (child) {
                Vector2 position = { 100, static_cast<float>(_itemViews.size() * 50 + 100) }; // Example positioning
                Vector2 size = { 200, 40 }; // Example size
                auto itemView = std::make_shared<MenuItemView>(position, size);
                _itemViews.push_back(itemView);
            }
        }
    }
    else {
        _itemViews.clear(); // Clear existing item views
        // Create item views based on the game state
        auto itemViews = gameState->createMenuButtonItemViews(menuModel);
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