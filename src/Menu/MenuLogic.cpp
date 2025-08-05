#include "MenuLogic.h"
#include "ICommand.h"
#include <raylib.h>

MenuItem::MenuItem(const std::string& name, std::unique_ptr<ICommand> cmd)
    : name(name), command(std::move(cmd)) {}

void MenuItem::add(std::shared_ptr<MenuComponent> component) {
    // MenuItem cannot have children, so we do nothing or throw an exception
    std::cerr << "Cannot add a component to a MenuItem." << std::endl;
}

void MenuItem::handleInput() {}

std::string MenuItem::getName() const {
    return name;
}

Menu::Menu(const std::string& name) : name(name) {}

void Menu::add(std::shared_ptr<MenuComponent> component) {
    components.push_back(component);
}

void Menu::handleInput() {}

std::string Menu::getName() const {
    return name;
}

void Menu::run() {}