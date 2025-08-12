// #include "MenuFactory.h"
// #include "MenuComponent.h"
// #include "MenuCommand.h"
// #include "gameState.h"
// #include "SceneManager.h"

// CompositeMenuFactory::CompositeMenuFactory(GameStateModel* gameStateModel, SceneManager* sceneManager)
//     : _gameStateModel(gameStateModel), _sceneManager(sceneManager) {}

// std::shared_ptr<MenuComponent> CompositeMenuFactory::createMenuSystem() {
    // Create the root container
    // auto root = createMainMenu();

    // // Create main menu and add it to the root
    // auto mainMenu = createMainMenu();
    // root->addItem(std::move(mainMenu));

    // // Additional menus can be created and added here

    // return root;
    // return nullptr; // Placeholder return, actual implementation needed
// }

// std::shared_ptr<MenuComponent> CompositeMenuFactory::createMenuForState(GameStateModel::State state) {
//     switch (state) {
//         case GameStateModel::State::MAIN_MENU:
//             return createMainMenu();
//         case GameStateModel::State::SETTINGS_MENU:
//             return createSettingsMenu();
//         case GameStateModel::State::IN_GAME:
//             return createInGameMenu(); // Implement this method
//         // Add cases for other states as needed
//         default:
//             return nullptr; // Unknown state
//     }
// }





// std::shared_ptr<MenuComponent> CompositeMenuFactory::createMainMenu() {
//     auto mainMenu = std::make_shared<Menu>("Main Menu", true);
//     std::shared_ptr<MenuComponent> Versus = std::make_shared<MenuItem>("Versus", true);
//     Versus->setCommand(std::make_unique<VersusCommand>(gameState, _sceneManager)); // Set command for Versus

//     std::shared_ptr<MenuComponent> Puzzles = std::make_shared<MenuItem>("Puzzles", true);
//     std::shared_ptr<MenuComponent> Guide = std::make_shared<MenuItem>("Guide", true);
//     std::shared_ptr<MenuComponent> Settings = std::make_shared<MenuItem>("Settings", true);
//     std::shared_ptr<MenuComponent> Exit = std::make_shared<MenuItem>("Exit", true); 

    
    
//     mainMenu->addItem(Versus);
//     mainMenu->addItem(Puzzles);
//     mainMenu->addItem(Guide);
//     mainMenu->addItem(Settings);
//     mainMenu->addItem(Exit);

//     // mainMenu->setCommand() // Example command for the main menu

//     return mainMenu;
// }

// std::shared_ptr<MenuComponent> CompositeMenuFactory::createSettingsMenu() {
//     auto settingsMenu = std::make_shared<Menu>("Settings", true);
    
//     // Add items to the settings menu
//     std::shared_ptr<MenuComponent> audioSettings = std::make_shared<MenuItem>("Audio Settings", true);
//     // audioSettings->setCommand() // Example command for audio settings
//     std::shared_ptr<MenuComponent> videoSettings = std::make_shared<MenuItem>("Video Settings", true);
//     // videoSettings->setCommand() // Example command for video settings
//     std::shared_ptr<MenuComponent> controlsSettings = std::make_shared<MenuItem>("Controls Settings", true);
//     // controlsSettings->setCommand() // Example command for controls settings

//     settingsMenu->addItem(audioSettings);
//     settingsMenu->addItem(videoSettings);
//     settingsMenu->addItem(controlsSettings);

//     // settingsMenu->setCommand() // Example command for the settings menu

//     return settingsMenu;
// }


// std::shared_ptr<MenuComponent> CompositeMenuFactory::createInGameMenu() {
//     auto inGameMenu = std::make_shared<Menu>("In-Game Menu", true);

//     // Add items to the in-game menu
//     std::shared_ptr<MenuComponent> resumeGame = std::make_shared<MenuItem>("Resume Game", true);
//     // resumeGame->setCommand() // Example command for resuming the game
//     std::shared_ptr<MenuComponent> saveGame = std::make_shared<MenuItem>("Save Game", true);
//     // saveGame->setCommand() // Example command for saving the game
//     std::shared_ptr<MenuComponent> exitToMainMenu = std::make_shared<MenuItem>("Exit to Main Menu", true);
//     // exitToMainMenu->setCommand() // Example command for exiting to main menu

//     inGameMenu->addItem(resumeGame);
//     inGameMenu->addItem(saveGame);
//     inGameMenu->addItem(exitToMainMenu);

//     // inGameMenu->setCommand() // Example command for the in-game menu

//     return inGameMenu;
// }
