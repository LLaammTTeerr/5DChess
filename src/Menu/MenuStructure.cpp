#include "MenuStructure.h"
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

const std::vector<std::shared_ptr<MenuComponent>>& MenuItem::getChildren() const {
    static std::vector<std::shared_ptr<MenuComponent>> emptyChildren;
    return emptyChildren; // MenuItem does not have children
}

std::unique_ptr<ICommand> MenuItem::cloneCommand() const {
    if (_command) {
        return _command->clone(); // Assuming ICommand has a clone method
    }
    return nullptr; // No command to clone
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

std::unique_ptr<MenuComponent> CompositeMenuFactory::createMenuSystem() {
    // Create the root container
    // auto root = std::make_unique<Menu>(std::make_unique<ICommand>(), true);

    // // Create main menu and add it to the root
    // auto mainMenu = createMainMenu();
    // root->addItem(std::move(mainMenu));

    // // Additional menus can be created and added here

    // return root;
    return nullptr; // Placeholder return, actual implementation needed
}

std::unique_ptr<MenuComponent> CompositeMenuFactory::createMainMenu() {
    auto mainMenu = std::make_unique<Menu>("Main Menu", true);
    
    // Add items to the main menu
    std::shared_ptr<MenuComponent> startGame = std::make_shared<MenuItem>("Start Game", true);
    // startGame->setCommand() // Example command
    std::shared_ptr<MenuComponent> settings = std::make_shared<MenuItem>("Settings", true);
    // settings->setCommand(std::make_unique<ICommand>()); // Example command
    std::shared_ptr<MenuComponent> exitGame = std::make_shared<MenuItem>("Exit Game", true);
    // exitGame->setCommand(std::make_unique<ICommand>()); // Example command 

    mainMenu->addItem(startGame);
    mainMenu->addItem(settings);
    mainMenu->addItem(exitGame);

    // mainMenu->setCommand() // Example command for the main menu

    return mainMenu;
}

std::unique_ptr<MenuComponent> CompositeMenuFactory::createSettingsMenu() {
    auto settingsMenu = std::make_unique<Menu>("Settings", true);
    
    // Add items to the settings menu
    std::shared_ptr<MenuComponent> audioSettings = std::make_shared<MenuItem>("Audio Settings", true);
    // audioSettings->setCommand() // Example command for audio settings
    std::shared_ptr<MenuComponent> videoSettings = std::make_shared<MenuItem>("Video Settings", true);
    // videoSettings->setCommand() // Example command for video settings
    std::shared_ptr<MenuComponent> controlsSettings = std::make_shared<MenuItem>("Controls Settings", true);
    // controlsSettings->setCommand() // Example command for controls settings

    settingsMenu->addItem(audioSettings);
    settingsMenu->addItem(videoSettings);
    settingsMenu->addItem(controlsSettings);

    // settingsMenu->setCommand() // Example command for the settings menu

    return settingsMenu;
}

void CompositeMenuFactory::showMenu(MenuComponent* root, const std::string& title) {
    if (!root) {
        return; // No menu to show
    }

    for (auto child : root->getChildren()) {
        child -> setEnabled(false); // Disable all children initially
    }

    auto showedMenu = root->findItem(title);
    if (showedMenu) {
        showedMenu -> setEnabled(true);
    }
    else {
        std::cerr << "Menu with title '" << title << "' not found." << std::endl;
        return; // Menu not found
    }
}   


void MenuController::setViewStrategy(std::unique_ptr<IMenuView> view) {
    _menuView = std::move(view);
    if (_menuView) {
        _menuView->createItemViews(*_menuModel);
    }
}

std::unique_ptr<MenuComponent> CompositeMenuFactory::createMenuForState(GameStateModel::State state) {
    switch (state) {
        case GameStateModel::State::MAIN_MENU:
            return createMainMenu();
        case GameStateModel::State::SETTINGS_MENU:
            return createSettingsMenu();
        // Add cases for other states as needed
        default:
            return nullptr; // Unknown state
    }
}


void MenuController::updateMenuForCurrentState() {
   GameStateModel::State currentState = _gameState->getState();
   std::cout << "Current State: " << _gameState->getCurrentStateName() << std::endl;
   if (currentState != _lastState) {
        std::cout << "State changed from " << _gameState->getCurrentStateName() << " to " << _gameState->getCurrentStateName() << std::endl;
        std::unique_ptr<MenuComponent> newMenu = _menuFactory.createMenuForState(currentState);
        if (newMenu) {
            _menuModel = newMenu.release(); // Transfer ownership of the new menu
            _menuFactory.showMenu(_menuModel, _gameState->getCurrentStateName());
        }
        _lastState = currentState;
    }

    // if (currentState == GameStateModel::State::SETTINGS_MENU) {
    //     updateSettingsDisplay();
    // }
}


void MenuController::handleInput() {
    Vector2 mousePosition = GetMousePosition();
    bool mouseClicked = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);

    if (!_menuView) {
        std::cerr << "Menu view is not set." << std::endl;
        return; // No view strategy set
    }
    // get input from the current menu view
    const std::vector<std::shared_ptr<MenuItemView>>& itemViews = _menuView->getItemViews();
    const auto& menuItems = _menuModel->getChildren();


    for (size_t i = 0; i < itemViews.size() && i < menuItems.size(); ++i) {
        bool isHovered = itemViews[i]->isPointInside(mousePosition) && menuItems[i]->isEnabled();
        itemViews[i]->setHovered(isHovered);

        if (isHovered && mouseClicked) {
            std::cout << "Clicked on:" << menuItems[i]->getTitle() << std::endl;
        //     auto command = menuItems[i]->cloneCommand();
        //     if (command) {
        //         command->execute(); // Execute the command associated with the menu item
        //     }
        }
    }
}


void MenuController::update() {
    updateMenuForCurrentState();
    handleInput();
}

void MenuController::draw() const {
    if (_menuView) {
        _menuView -> draw(*_menuModel); 
    }
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

    const auto& menuItems = menuModel.getChildren();
    // Draw each item view
    for (size_t i = 0; i < _itemViews.size() && i < menuItems.size(); ++i) {
        if (_itemViews[i] && menuItems[i] -> isEnabled()) {
            _itemViews[i] -> draw(*menuItems[i]);
        }
    }
}