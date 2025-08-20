#include "MenuController.h"
#include "MenuView.h"
#include "MenuCommand.h"
#include "MenuComponent.h"
#include "SceneManager.h"
#include "gameState.h"
#include "GameStates/ConcreteGameStates/MainMenuState.h"
#include "GameStates/ConcreteGameStates/VersusState.h"
#include "Render/View.h"
#include "Render/Controller.h"
#include "Render/RenModel.h"

NavigationMenuController::NavigationMenuController(GameStateModel* gameStateModel, std::shared_ptr<MenuComponent> menuSystem, 
                               SceneManager* sceneManager)
    : _gameStateModel(gameStateModel), 
      _lastState(std::make_unique<MainMenuState>()), // Initialize with a default state
      _sceneManager(sceneManager)
{
    _menuSystem = menuSystem; // Assign to the protected member from the base class
    _currentMenuModel = _menuSystem; // Use the shared menu system directly
    _menuView = std::make_unique<ButtonMenuView>(); // Default view strategy
}

void NavigationMenuController::setViewStrategy(std::unique_ptr<IMenuView> view) {
    _menuView = std::move(view);
    if (_menuView) {
        _menuView->createNavigationItemViews(_currentMenuModel, _gameStateModel->getCurrentState());
    }
}



void NavigationMenuController::updateNavigationMenuForCurrentState() {
   if (_gameStateModel->getCurrentStateName() != _lastState->getName()) {
        std::shared_ptr<MenuComponent> newMenu = _gameStateModel->getCurrentState()->createNavigationMenu(_gameStateModel, _sceneManager);
        if (newMenu) {
            _currentMenuModel = newMenu; // Update current menu model (both are shared_ptr now)
            _menuView->createNavigationItemViews(_currentMenuModel, _gameStateModel->getCurrentState()); // Update item views for the new menu
        }
        _lastState = std::move(_gameStateModel->getCurrentState()->clone()); // Store the last state
    }
}


void NavigationMenuController::handleInput() {
    Vector2 mousePosition = GetMousePosition();
    bool mouseClicked = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);

    if (!_menuView) {
        std::cout << "Menu view is not set." << std::endl;
        return; // No view strategy set
    }
    // get input from the current menu view
    const std::vector<std::shared_ptr<MenuItemView>>& itemViews = _menuView->getItemViews();
    const auto& menuItems = _currentMenuModel->getChildren();

    for (size_t i = 0; i < itemViews.size() && i < menuItems.size(); ++i) {
        bool isHovered = itemViews[i]->isPointInside(mousePosition) && menuItems[i]->isEnabled();
        itemViews[i]->setHovered(isHovered);

        if (isHovered && mouseClicked) {
            std::cout << "Clicked on:" << menuItems[i]->getTitle() << std::endl;
            auto command = menuItems[i]->cloneCommand();
            std::cout << "Command: " << (command ? command->getName() : "None") << std::endl;
            if (command) {
                CommandType cmdType = command->getType();
                command->execute(); // Execute the command
                
                // Handle different command types
                if (cmdType == CommandType::STATE_CHANGING || cmdType == CommandType::IMMEDIATE) {
                    std::cout << "State-changing command executed, stopping command processing" << std::endl;
                    break; // Stop processing more commands for state-changing commands
                }
                // For NON_STATE commands, continue to allow multiple commands in one frame
                std::cout << "Non-state command executed, continuing..." << std::endl;
            }
        }
    }
}


void NavigationMenuController::update() {
    updateNavigationMenuForCurrentState();
    handleInput();
}

void NavigationMenuController::draw() const {
    if (_menuView) {
        _menuView -> draw(_currentMenuModel); 
    }
}

IMenuView* NavigationMenuController::getMenuView() const {
    return _menuView.get();
}

InGameMenuController::InGameMenuController(ChessModel* chessModel, ChessView* chessView, std::shared_ptr<MenuComponent> menuSystem)
    : _chessModel(chessModel), _chessView(chessView) {
    _menuSystem = menuSystem; // Assign to the protected member from the base class
    setViewStrategy(std::make_unique<ButtonMenuView>()); // Default view strategy
}

void InGameMenuController::setViewStrategy(std::unique_ptr<IMenuView> view) {
    _menuView = std::move(view);
    if (_menuView) {
        int numberOfItems = 3; // Deslect, Undo, Submit
        _menuView->createInGameItemsViews(numberOfItems);
    }
}

void InGameMenuController::handleInput() {
    Vector2 mousePosition = GetMousePosition();
    bool mouseClicked = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);

    if (!_menuView) {
        std::cout << "InGameMenuView is not set." << std::endl;
        return; // No view strategy set
    }

    const std::vector<std::shared_ptr<MenuItemView>>& itemViews = _menuView->getItemViews();
    const auto& menuItems = _menuSystem->getChildren();

    for (size_t i = 0; i < itemViews.size() && i < menuItems.size(); ++i) {
        bool isHovered = itemViews[i]->isPointInside(mousePosition) && menuItems[i]->isEnabled();
        itemViews[i]->setHovered(isHovered);

        if (isHovered && mouseClicked) {
            std::cout << "Clicked on: " << menuItems[i]->getTitle() << std::endl;
            auto command = menuItems[i]->cloneCommand();
            if (command) {
                command->execute(); // Execute the command
                // command->executeCallback(); // Execute the callback if set
            }
            else {
                std::cout << "No command associated with this menu item." << std::endl;
            }
        }
    }
}
void InGameMenuController::update() {}
void InGameMenuController::draw() const {
    if (_menuView) {
        _menuView->draw(_menuSystem);
    } else {
        std::cerr << "InGameMenuView is not initialized!" << std::endl;
    }
}