#include "MenuView.h"
#include "MenuComponent.h"


bool MenuItemView::isPointInside(Vector2 point) const {
    return point.x >= position.x && point.x <= position.x + size.x &&
           point.y >= position.y && point.y <= position.y + size.y;
}

void MenuItemView::draw(const MenuComponent& menuComponent) const {
    Color bgColor = menuComponent.isEnabled() ? (isHovered ? hoverBackgroundColor : normalBackgroundColor) : disabledBackgroundColor;
    Color textColor = menuComponent.isEnabled() ? (isHovered ? hoverTextColor : normalTextColor) : disabledTextColor;

    DrawRectangleV(position, size, bgColor);
    DrawRectangleLinesEx({position.x, position.y, size.x, size.y}, 2, BLACK);

    Vector2 textSize = MeasureTextEx(font, menuComponent.getText().c_str(), fontSize, fontSpacing);
    Vector2 textPos = {
        position.x + (size.x - textSize.x) / 2,
        position.y + (size.y - textSize.y) / 2
    };
    DrawTextEx(font, menuComponent.getTitle().c_str(), textPos, fontSize, fontSpacing, textColor);
}


// void ButtonMenuView::drawTitle(const std::string& title) const {
//     DrawTextEx(titleFont, title.c_str(), { 10, 10 }, titleFontSize, 1.0f, titleColor);
// }

void ButtonMenuView::createItemViews(const MenuComponent& menuModel) {
    _itemViews.clear(); // Clear existing item views

    for (const auto& child : menuModel.getChildren()) {
        if (child) {
            Vector2 position = { 100, static_cast<float>(_itemViews.size() * 50 + 100) }; // Example positioning
            Vector2 size = { 200, 40 }; // Example size
            auto itemView = std::make_shared<MenuItemView>(position, size);
            _itemViews.push_back(itemView);
        }
    }
}

void ButtonMenuView::draw(const MenuComponent& menuModel) const {
    drawBackground();
    std::cout << "draw " << menuModel.getTitle() << std::endl; // Draw the menu title
    const auto& menuItems = menuModel.getChildren();
    // Draw each item view
    for (size_t i = 0; i < _itemViews.size() && i < menuItems.size(); ++i) {
        if (_itemViews[i] && menuItems[i] -> isEnabled()) {
            _itemViews[i] -> draw(*menuItems[i]);
        }
    }
}