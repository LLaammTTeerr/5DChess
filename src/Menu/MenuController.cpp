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
#include "VersusScene.h" // Include VersusScene for VersusMenuController

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
   bool shouldUpdate = false;

   // Check if state name changed
   if (_gameStateModel->getCurrentStateName() != _lastState->getName()) {
       shouldUpdate = true;
   }
   // Special handling for VersusState - check if menu structure changed
   else if (auto* versusState = dynamic_cast<VersusState*>(_gameStateModel->getCurrentState())) {
       if (auto* lastVersusState = dynamic_cast<VersusState*>(_lastState.get())) {
           // Check if menu version changed (indicates menu structure change)
           if (versusState->getMenuVersion() != lastVersusState->getMenuVersion()) {
               shouldUpdate = true;
               std::cout << "NavigationMenuController: VersusState menu version changed, updating menu" << std::endl;
           }
       } else {
           shouldUpdate = true; // Last state wasn't VersusState
       }
   }

   if (shouldUpdate) {
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

// VersusMenuController Implementation
VersusMenuController::VersusMenuController(VersusScene* versusScene)
    : _versusScene(versusScene) {
    // Default view strategy will be set later
    _menuView = nullptr;
}

void VersusMenuController::setViewStrategy(std::unique_ptr<IMenuView> view) {
    _menuView = std::move(view);
    if (_menuView && _menuSystem) {
        _menuView->createInGameItemsViews(_menuSystem->getChildren().size());
    }
}

void VersusMenuController::handleInput() {
    if (!_menuView || !_menuSystem) return;

    Vector2 mousePos = GetMousePosition();
    bool mouseClicked = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);

    // Handle scroll input for ListMenuView
    if (auto* listView = dynamic_cast<ListMenuView*>(_menuView.get())) {
        listView->handleScrollInput();
    }

    // Handle hover states and clicks
    const auto& itemViews = _menuView->getItemViews();
    const auto& menuItems = _menuSystem->getChildren();

    for (size_t i = 0; i < itemViews.size() && i < menuItems.size(); ++i) {
        if (itemViews[i] && menuItems[i]->isEnabled()) {
            Vector2 itemPos, itemSize;
            Rectangle itemRect;
            
            // Get correct position accounting for scroll offset in ListMenuView
            if (auto* listView = dynamic_cast<ListMenuView*>(_menuView.get())) {
                itemPos = listView->getScrolledItemPosition(i);
                itemSize = itemViews[i]->getSize();
                
                // Check if item is visible in the list area
                Rectangle listArea = listView->getListArea();
                if (itemPos.y + itemSize.y < listArea.y || itemPos.y > listArea.y + listArea.height) {
                    // Item is not visible, skip interaction
                    itemViews[i]->setHovered(false);
                    itemViews[i]->setSelected(static_cast<int>(i) == _selectedGameModeIndex);
                    continue;
                }
                
                // Ensure click area doesn't extend beyond the list area (excluding scrollbar)
                Rectangle clickableArea = {
                    fmaxf(itemPos.x, listArea.x),
                    fmaxf(itemPos.y, listArea.y),
                    fminf(itemSize.x, listArea.width - 15.0f), // Account for scrollbar width
                    fminf(itemSize.y, listArea.y + listArea.height - itemPos.y)
                };
                itemRect = clickableArea;
            } else {
                itemPos = itemViews[i]->getPosition();
                itemSize = itemViews[i]->getSize();
                itemRect = {itemPos.x, itemPos.y, itemSize.x, itemSize.y};
            }
            
            bool isHovered = CheckCollisionPointRec(mousePos, itemRect);

            itemViews[i]->setHovered(isHovered);

            // Set selected state based on the currently selected game mode index
            itemViews[i]->setSelected(static_cast<int>(i) == _selectedGameModeIndex);

            if (isHovered && mouseClicked) {
                // Update selected game mode index
                _selectedGameModeIndex = static_cast<int>(i);

                // Execute the command for this menu item
                auto command = menuItems[i]->cloneCommand();
                if (command) {
                    command->execute();
                }
            }
        }
    }
}

void VersusMenuController::update() {
    // Update scrollbar if using ListMenuView
    if (_menuView) {
        if (auto* listView = dynamic_cast<ListMenuView*>(_menuView.get())) {
            listView->updateScrollbar();
        }

        // Update selected state for all menu items
        const auto& itemViews = _menuView->getItemViews();
        for (size_t i = 0; i < itemViews.size(); ++i) {
            if (itemViews[i]) {
                itemViews[i]->setSelected(static_cast<int>(i) == _selectedGameModeIndex);
            }
        }
    }
}

void VersusMenuController::draw() const {
    if (_menuView && _menuSystem) {
        _menuView->draw(_menuSystem);
    }
}

std::vector<std::string> fetchGameMode(void) {
  std::vector<std::string> gameModes;
  #define REGISTER_MODE(X) gameModes.push_back(Chess::NameOfGame<X>::value)
  REGISTER_MODE(Chess::StandardGame);
  REGISTER_MODE(Chess::CustomGameEmitBishop);
  REGISTER_MODE(Chess::CustomGameEmitKnight);
  REGISTER_MODE(Chess::CustomGameEmitQueen);
  #undef REGISTER_MODE

  gameModes.push_back("Knight vs. Bishop");
  gameModes.push_back("Simple Set");
  gameModes.push_back("Small");
  gameModes.push_back("Small - Flipped");
  gameModes.push_back("Small - Centerd");
  gameModes.push_back("Small - Open");
  return gameModes;
}

void VersusMenuController::createGameModeMenu() {
    // Create the main menu container
    _menuSystem = std::make_shared<Menu>("Game Mode Selection", true);

    // Create game mode menu items
    static std::vector<std::string> gameModes;
    if (gameModes.empty())
      gameModes = fetchGameMode();


    for (const auto& mode : gameModes) {
        auto menuItem = std::make_shared<MenuItem>(mode, true);

        // Create command for this game mode
        auto command = std::make_unique<GameModeSelectCommand>(mode, _versusScene);
        menuItem->setCommand(std::move(command));

        _menuSystem->addItem(menuItem);
    }

    // Update menu view if it exists
    if (_menuView) {
        _menuView->createInGameItemsViews(_menuSystem->getChildren().size());
    }
}

void VersusMenuController::selectGameMode(const std::string& mode) {
    _selectedGameMode = mode;
    std::cout << "Game mode selected via controller: " << mode << std::endl;

    // Find and update the selected game mode index
    if (_menuSystem) {
        const auto& menuItems = _menuSystem->getChildren();
        for (size_t i = 0; i < menuItems.size(); ++i) {
            if (menuItems[i]->getTitle() == mode) {
                _selectedGameModeIndex = static_cast<int>(i);
                break;
            }
        }
    }
}
