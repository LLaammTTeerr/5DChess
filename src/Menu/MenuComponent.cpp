#include "MenuComponent.h"
#include "ICommand.h"
#include "MenuCommand.h"

// std::shared_ptr<MenuComponent> MenuItem::clone() const {
//     auto cloned = std::make_shared<MenuComponent>(*this);
    
//     // Clone the command if it exists
//     if (_command) {
//         cloned->setCommand(_command->clone());
//     }
    
//     return cloned;
// }

// std::shared_ptr<MenuComponent> Menu::clone() const {
//     auto cloned = std::make_shared<Menu>(*this);
    
//     // Clone the command if it exists
//     if (_command) {
//         cloned->setCommand(_command->clone());
//     }
    
//     // Clone children
//     for (const auto& child : _children) {
//         cloned->addItem(child->clone());
//     }
    
//     return cloned;
// }

MenuItem::MenuItem(const std::string& title, bool enabled)
    : _title(title), _enabled(enabled), _command(nullptr) {}

void MenuItem::addItem(std::shared_ptr<MenuComponent> component) {}
void MenuItem::removeItem(std::shared_ptr<MenuComponent> component) {}

MenuComponent* MenuItem::findItem(const std::string& title) {
    return nullptr; // MenuItem does not have children, so return nullptr
}   

void MenuItem::setTitle(const std::string& title) {
    _title = title;
}
void MenuItem::setText(const std::string& text) {
    _text = text;
}
std::string MenuItem::getTitle() const {
    return _title;
}
std::string MenuItem::getText() const {
    return _text;
}

void MenuItem::setEnabled(bool enabled) {
    _enabled = enabled;
}
bool MenuItem::isEnabled() const {
    return _enabled;
}
void MenuItem::setCommand(std::unique_ptr<ICommand> command) {
    _command = std::move(command);
}
std::unique_ptr<ICommand> MenuItem::cloneCommand() const {
    if (_command) {
        return _command->clone(); // Assuming ICommand has a clone method
    }
    return nullptr; // No command to clone
}

const std::vector<std::shared_ptr<MenuComponent>>& MenuItem::getChildren() const {
    static std::vector<std::shared_ptr<MenuComponent>> emptyChildren;
    return emptyChildren; // MenuItem does not have children
}

Menu::Menu(const std::string& title, bool enabled)
    : _title(title), _enabled(enabled), _command(nullptr) {}
void Menu::addItem(std::shared_ptr<MenuComponent> component) {
    _children.push_back(component);
}

std::unique_ptr<ICommand> Menu::cloneCommand() const {
    if (_command) {
        return _command->clone(); // Assuming ICommand has a clone method
    }
    return nullptr; // No command to clone
}

void Menu::removeItem(std::shared_ptr<MenuComponent> component) {
    _children.erase(std::remove(_children.begin(), _children.end(), component), _children.end());
}

MenuComponent* Menu::findItem(const std::string& title) {
    if (_title == title) {
        return this; // Return this menu if the title matches
    }

    for (const auto& component : _children) {
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

void Menu::setTitle(const std::string& title) {
    _title = title;
}
void Menu::setText(const std::string& text) {
    _text = text;
}
std::string Menu::getTitle() const {
    return _title;
}
std::string Menu::getText() const {
    return _text;
}
void Menu::setEnabled(bool enabled) {
    _enabled = enabled;
}
bool Menu::isEnabled() const {
    return _enabled;
}
void Menu::setCommand(std::unique_ptr<ICommand> command) {
    _command = std::move(command);
}
const std::vector<std::shared_ptr<MenuComponent>>& Menu::getChildren() const {
    return _children; // Return the children of this menu
}
