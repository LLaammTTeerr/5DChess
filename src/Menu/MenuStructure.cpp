#include <raylib.h>
#include "MenuStructure.h"
#include <memory>
#include "ICommand.h"

std::string GameStateModel::getCurrentStateName() const {
    switch (_currentState) {
      case State::MAIN_MENU: return "Main Menu";
      case State::SETTINGS_MENU: return "Settings Menu";
      case State::IN_GAME: return "In Game";
      case State::PAUSED: return "Paused";
      default: return "Unknown State";
    }
}

void Menu::removeItem(std::shared_ptr<MenuComponent> component) {
    _components.erase(std::remove(_components.begin(), _components.end(), component), _components.end());
}



MenuComponent* Menu::findItem(const std::string& title) {
    if (_title == title) {
        return this; // Return this menu if the title matches
    }

    for (const auto& component : _components) {
        if (component->getTitle() == title) {
            return component.get();
        }
        MenuComponent* found = component->findItem(title);
        if (found) {            
            return found;
        }
    }

    return nullptr; // Not found
}

bool MenuItemView::isPointInside(Vector2 point) const {
    return point.x >= position.x && point.x <= position.x + size.x &&
           point.y >= position.y && point.y <= position.y + size.y;
}

void MenuItemView::draw(const MenuComponent& menuComponent) const {
    Color bgColor = menuComponent.isEnabled() ? (isHovered ? hoverBackgroundColor : normalBackgroundColor) : disabledBackgroundColor;
    Color textColor = menuComponent.isEnabled() ? (isHovered ? hoverTextColor : normalTextColor) : disabledTextColor;

    DrawRectangleV(position, size, bgColor);
    DrawRectangleLinesEx({position.x, position.y, size.x, size.y}, 2, BLACK);

    Vector2 textSize = MeasureTextEx(font, menuComponent.getText().c_str(), 20, 1);
    Vector2 textPos = {
        position.x + (size.x - textSize.x) / 2,
        position.y + (size.y - textSize.y) / 2
    };

    DrawText(menuComponent.getText().c_str(), (int)textPos.x, (int)textPos.y, 20, textColor);
}
