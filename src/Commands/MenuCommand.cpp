#include "MenuCommand.h"
#include "SceneManager.h"
#include "gameState.h"
#include "Scene/ConcreteScene/VersusScene.h"
#include "Scene/ConcreteScene/MainMenuScene.h"
#include "GameStates/ConcreteGameStates/VersusState.h"
#include "GameStates/ConcreteGameStates/MainMenuState.h"
// startGameCommand::startGameCommand(GameStateModel* gameState)
//     : _gameState(gameState) {}

// void startGameCommand::execute() {
//     if (_gameState) {
//         _gameState->setState(GameStateModel::State::IN_GAME);
//         std::cout << "Game started." << std::endl;
//     }
// }

// std::unique_ptr<ICommand> startGameCommand::clone() const {
//     // create a new instance of startGameCommand with the same game state
//     return std::make_unique<startGameCommand>(_gameState);
// }

void VersusCommand::execute() {
    if (_gameStateModel) {
        std::unique_ptr<GameState> newState = std::make_unique<VersusState>();
        // Notify SceneManager to change scene
        if (_sceneManager) {
            std::unique_ptr<Scene> scene = newState->createScene();
            if (scene) {
                _sceneManager->changeScene(std::move(scene));
            }
        }

        _gameStateModel->setState(std::move(newState));
        std::cout << "Versus mode activated." << std::endl;
    } else {
        std::cerr << "Error: GameStateModel is not initialized." << std::endl;
    }
}

std::unique_ptr<ICommand> VersusCommand::clone() const {
    // create a new instance of VersusCommand with the same game state and scene manager
    return std::make_unique<VersusCommand>(_gameStateModel, _sceneManager);
}

CommandType VersusCommand::getType() const {
    return CommandType::STATE_CHANGING; // This command changes the game state
}

void VersusBackCommand::execute() {
    if (_gameStateModel) {
        // Set the game state back to Main Menu
        std::unique_ptr<GameState> newState = std::make_unique<MainMenuState>();
        
        // Notify SceneManager to change scene
        if (_sceneManager) {
            std::unique_ptr<Scene> scene = newState->createScene();
            if (scene) {
                _sceneManager->changeScene(std::move(scene));
            }
        }

        _gameStateModel->setState(std::move(newState));
        std::cout << "Returning to Main Menu." << std::endl;
    } else {
        std::cerr << "Error: GameStateModel is not initialized." << std::endl;
    }
}

std::unique_ptr<ICommand> VersusBackCommand::clone() const {
    // create a new instance of VersusBackCommand with the same game state
    return std::make_unique<VersusBackCommand>(_gameStateModel, _sceneManager);
}

CommandType VersusBackCommand::getType() const {
    return CommandType::STATE_CHANGING; // This command changes the game state
}