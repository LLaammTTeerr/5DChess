#include "MenuItemView.h"
#include "MenuComponent.h"
#include <raylib.h>


bool MenuItemView::isPointInside(Vector2 point) const {
    return point.x >= position.x && point.x <= position.x + size.x &&
           point.y >= position.y && point.y <= position.y + size.y;
}

void MenuItemView::draw(std::shared_ptr<MenuComponent> menuComponent) const {
    Color bgColor, textColor;
    
    if (!menuComponent->isEnabled()) {
        bgColor = disabledBackgroundColor;
        textColor = disabledTextColor;
    } else if (isSelected) {
        bgColor = selectedBackgroundColor;
        textColor = selectedTextColor;
    } else if (isHovered) {
        bgColor = hoverBackgroundColor;
        textColor = hoverTextColor;
    } else {
        bgColor = normalBackgroundColor;
        textColor = normalTextColor;
    }

    DrawRectangleV(position, size, bgColor);
    DrawRectangleLinesEx({position.x, position.y, size.x, size.y}, 2, BLACK);

    Vector2 textSize = MeasureTextEx(font, menuComponent->getTitle().c_str(), fontSize, fontSpacing);
    Vector2 textPos = {
        position.x + (size.x - textSize.x) / 2,
        position.y + (size.y - textSize.y) / 2
    };
    DrawTextEx(font, menuComponent->getTitle().c_str(), textPos, fontSize, fontSpacing, textColor);
}
