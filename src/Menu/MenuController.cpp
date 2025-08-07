#include "MenuController.h"
#include "MenuView.h"
#include "MenuFactory.h"
#include "MenuCommand.h"
#include "MenuComponent.h"

MenuController::MenuController(GameStateModel* gameState, std::shared_ptr<MenuComponent> menuSystem)
    : _gameState(gameState), 
      _lastState(GameStateModel::State::MAIN_MENU),
      _menuSystem(menuSystem),  
      _menuFactory(gameState)
{
    
    _currentMenuModel = _menuSystem; // Use the shared menu system directly

    _menuView = std::make_unique<ButtonMenuView>(); // Default view strategy
}


void MenuController::setViewStrategy(std::unique_ptr<IMenuView> view) {
    _menuView = std::move(view);
    if (_menuView) {
        _menuView->createItemViews(*_currentMenuModel);
    }
}




void MenuController::updateMenuForCurrentState() {
   GameStateModel::State currentState = _gameState->getState();
   std::cout << "Current State: " << _gameState->getCurrentStateName() << std::endl;
   if (currentState != _lastState) {
        std::cout << "State changed from " << (_lastState == GameStateModel::State::MAIN_MENU ? "MAIN_MENU" : "OTHER") << " to " << _gameState->getCurrentStateName() << std::endl;
        std::shared_ptr<MenuComponent> newMenu = _menuFactory.createMenuForState(currentState);
        if (newMenu) {

            _currentMenuModel = newMenu; // Update current menu model (both are shared_ptr now)
            _menuView->createItemViews(*_currentMenuModel); // Update item views for
            _menuFactory.activateMenu(_currentMenuModel, _gameState->getCurrentStateName());
            std::cout << "Menu updated for state: " << _gameState->getCurrentStateName() << std::endl;
            // display menu items
            for (const auto& item : _currentMenuModel->getChildren()) {
                std::cout << "Menu Item: " << item->getTitle() << ", Enabled: " << (item->isEnabled() ? "Yes" : "No") << std::endl;
            }
        }
        _lastState = currentState;
    }

    // if (currentState == GameStateModel::State::SETTINGS_MENU) {
    //     updateSettingsDisplay();
    // }
}


void MenuController::handleInput() {
    std::cout << "Handling input for current menu..." << std::endl;
    Vector2 mousePosition = GetMousePosition();
    bool mouseClicked = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);

    if (!_menuView) {
        std::cout << "Menu view is not set." << std::endl;
        return; // No view strategy set
    }
    // get input from the current menu view
    const std::vector<std::shared_ptr<MenuItemView>>& itemViews = _menuView->getItemViews();
    const auto& menuItems = _currentMenuModel->getChildren();

    std::cout << "hello" << std::endl;

    for (size_t i = 0; i < itemViews.size() && i < menuItems.size(); ++i) {
        bool isHovered = itemViews[i]->isPointInside(mousePosition) && menuItems[i]->isEnabled();
        itemViews[i]->setHovered(isHovered);

        if (isHovered && mouseClicked) {
            std::cout << "Clicked on:" << menuItems[i]->getTitle() << std::endl;
            auto command = menuItems[i]->cloneCommand();
            if (command) {
                command->execute(); // Execute the command associated with the menu item
                break;
            }
        }
    }
}


void MenuController::update() {
    updateMenuForCurrentState();
    handleInput();
}

void MenuController::draw() const {
    if (_menuView) {
        _menuView -> draw(*_currentMenuModel); 
    }
}

IMenuView* MenuController::getMenuView() const {
    return _menuView.get();
}