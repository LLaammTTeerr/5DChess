#include "gameState.h"
#include <iostream>
#include "MenuComponent.h"
#include "MenuCommand.h"
#include "Scene/ConcreteScene/MainMenuScene.h"
#include "Scene/ConcreteScene/VersusScene.h"
#include "Scene/SceneManager.h"
#include "GameStates/ConcreteGameStates/MainMenuState.h"
#include "GameStates/ConcreteGameStates/VersusState.h"

// GameStateModel implementation
GameStateModel::GameStateModel() {
    // Initialize with Main Menu state
    _currentState = std::make_unique<MainMenuState>();
    _currentState->onEnter(this);
}

void GameStateModel::setState(std::unique_ptr<GameState> newState) {
    if (_currentState) {
        _currentState->onExit(this);
    }
    _currentState = std::move(newState);
    if (_currentState) {
        _currentState->onEnter(this);
    }
}

void GameStateModel::setStateByName(const std::string& stateName) {
    auto newState = createState(stateName);
    if (newState) {
        setState(std::move(newState));
    }
}

std::string GameStateModel::getCurrentStateName() const {
    if (_currentState) {
        return _currentState->getName();
    }
    return "NONE";
}

void GameStateModel::update(float deltaTime) {
    if (_currentState) {
        _currentState->update(this, deltaTime);
    }
}

std::unique_ptr<GameState> GameStateModel::createState(const std::string& stateName) {
    if (stateName == "MAIN_MENU") {
        return std::make_unique<MainMenuState>();
    } 
    else if (stateName == "VERSUS") {
        return std::make_unique<VersusState>();
    } 
    // else if (stateName == "PUZZLES") {
    //     return std::make_unique<PuzzlesState>();
    // } else if (stateName == "GUIDE") {
    //     return std::make_unique<GuideState>();
    // } else if (stateName == "SETTINGS_MENU") {
    //     return std::make_unique<SettingsMenuState>();
    // } else if (stateName == "IN_GAME") {
    //     return std::make_unique<InGameState>();
    // } else if (stateName == "PAUSED") {
    //     return std::make_unique<PausedState>();
    // }
    return nullptr;
}



std::shared_ptr<MenuComponent> GameStateModel::createNavigationMenuForCurrentState(SceneManager* sceneManager) {
    if (_currentState) {
        return _currentState->createNavigationMenu(this, sceneManager);
    }
    return nullptr;
}