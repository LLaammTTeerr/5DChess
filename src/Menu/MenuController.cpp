#include "MenuController.h"
#include "MenuView.h"
#include "MenuFactory.h"
#include "MenuCommand.h"
#include "MenuComponent.h"
#include "SceneManager.h"
#include "gameState.h"
#include "GameStates/ConcreteGameStates/MainMenuState.h"
#include "GameStates/ConcreteGameStates/VersusState.h"

MenuController::MenuController(GameStateModel* gameStateModel, std::shared_ptr<MenuComponent> menuSystem, 
                               SceneManager* sceneManager)
    : _gameStateModel(gameStateModel), 
      _lastState(std::make_unique<MainMenuState>()), // Initialize with a default state
      _menuSystem(menuSystem),
      _sceneManager(sceneManager)
{
    _currentMenuModel = _menuSystem; // Use the shared menu system directly
    _menuView = std::make_unique<ButtonMenuView>(); // Default view strategy
}


void MenuController::setViewStrategy(std::unique_ptr<IMenuView> view) {
    _menuView = std::move(view);
    if (_menuView) {
        _menuView->createItemViews(_currentMenuModel, _gameStateModel->getCurrentState());
    }
}



void MenuController::updateMenuForCurrentState() {
   if (_gameStateModel->getCurrentStateName() != _lastState->getName()) {
        std::shared_ptr<MenuComponent> newMenu = _gameStateModel->getCurrentState()->createMenu(_gameStateModel, _sceneManager);
        if (newMenu) {
            _currentMenuModel = newMenu; // Update current menu model (both are shared_ptr now)
            _menuView->createItemViews(_currentMenuModel, _gameStateModel->getCurrentState()); // Update item views for the new menu
        }
        _lastState = std::move(_gameStateModel->getCurrentState()->clone()); // Store the last state
    }
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
                
                std::cout << "hihihihihihihihihihihi" << std::endl;
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
    std::cout << "Input handling complete." << std::endl;
}


void MenuController::update() {
    updateMenuForCurrentState();
    handleInput();
}

void MenuController::draw() const {
    if (_menuView) {
        _menuView -> draw(_currentMenuModel); 
    }
    std::cout << "Drawing menu for current state: " << _gameStateModel->getCurrentStateName() << std::endl;
}

IMenuView* MenuController::getMenuView() const {
    return _menuView.get();
}