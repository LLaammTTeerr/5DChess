#include "MenuCommand.h"
#include "SceneManager.h"
#include "gameState.h"
#include "Scene/ConcreteScene/VersusScene.h"
#include "Scene/ConcreteScene/MainMenuScene.h"
#include "Scene/ConcreteScene/TestingScene.h"
#include "Scene/ConcreteScene/SettingsScene.h"

#include "GameStates/ConcreteGameStates/VersusState.h"
#include "GameStates/ConcreteGameStates/MainMenuState.h"
#include "GameStates/ConcreteGameStates/TestingState.h"
#include "GameStates/ConcreteGameStates/SettingsState.h"
#include "Render/Controller.h"

// ChangeStateCommand execute implementation
void ChangeStateCommand::execute() {
  if (_gameStateModel && _stateFactory) {
    std::unique_ptr<GameState> newState = _stateFactory();
    
    // Notify SceneManager to change scene
    if (_sceneManager && newState) {
      std::unique_ptr<Scene> scene = newState->createScene();
      if (scene) {
        _sceneManager->changeScene(std::move(scene));
      }
    }

    _gameStateModel->setState(std::move(newState));
  }
}

// Factory function implementations with proper state creation
std::unique_ptr<ChangeStateCommand> createVersusCommand(
    GameStateModel* gameStateModel, SceneManager* sceneManager) {
  return std::make_unique<ChangeStateCommand>(
      gameStateModel, sceneManager,
      std::function<std::unique_ptr<GameState>()>([]() { 
        return std::make_unique<VersusState>(); 
      }),
      "Versus Command"
  );
}

std::unique_ptr<ChangeStateCommand> createVersusBackCommand(
    GameStateModel* gameStateModel, SceneManager* sceneManager) {
  return std::make_unique<ChangeStateCommand>(
      gameStateModel, sceneManager,
      std::function<std::unique_ptr<GameState>()>([]() { 
        return std::make_unique<MainMenuState>(); 
      }),
      "Back Command"
  );
}

std::unique_ptr<ChangeStateCommand> createVersusPlayCommand(
    GameStateModel* gameStateModel, SceneManager* sceneManager, 
    const std::string& selectedGameMode) {
  return std::make_unique<ChangeStateCommand>(
      gameStateModel, sceneManager,
      std::function<std::unique_ptr<GameState>()>([selectedGameMode]() { 
        return std::make_unique<TestingState>(selectedGameMode); 
      }),
      "Versus Play Command"
  );
}

std::unique_ptr<ChangeStateCommand> createBackToVersusCommand(
    GameStateModel* gameStateModel, SceneManager* sceneManager) {
  return std::make_unique<ChangeStateCommand>(
      gameStateModel, sceneManager,
      std::function<std::unique_ptr<GameState>()>([]() { 
        return std::make_unique<VersusState>(); 
      }),
      "Back to Versus Command"
  );
}

std::unique_ptr<ChangeStateCommand> createSettingsCommand(
    GameStateModel* gameStateModel, SceneManager* sceneManager) {
  return std::make_unique<ChangeStateCommand>(
      gameStateModel, sceneManager,
      std::function<std::unique_ptr<GameState>()>([]() { 
        return std::make_unique<SettingsState>(); 
      }),
      "Settings Command"
  );
}

std::unique_ptr<ChangeStateCommand> createSettingsBackCommand(
    GameStateModel* gameStateModel, SceneManager* sceneManager) {
  return std::make_unique<ChangeStateCommand>(
      gameStateModel, sceneManager,
      std::function<std::unique_ptr<GameState>()>([]() { 
        return std::make_unique<MainMenuState>(); 
      }),
      "Settings Back Command"
  );
}

SubmitMoveCommand::SubmitMoveCommand() {}

void SubmitMoveCommand::execute() {
    executeCallback(); // Execute the callback if set
}


std::unique_ptr<ICommand> SubmitMoveCommand::clone() const {
    auto cloned = std::make_unique<SubmitMoveCommand>();
    cloned->_callback = _callback; // Copy the callback
    return cloned;
}

std::unique_ptr<ICommand> UndoMoveCommand::clone() const {
    auto cloned = std::make_unique<UndoMoveCommand>();
    cloned->_callback = _callback; // Copy the callback
    return cloned;
}

std::unique_ptr<ICommand> DeselectMoveCommand::clone() const {
    auto cloned = std::make_unique<DeselectMoveCommand>();
    cloned->_callback = _callback; // Copy the callback
    return cloned;
}

void UndoMoveCommand::execute() {
    std::cout << "Undoing last move..." << std::endl;
    // This command is a placeholder for undo functionality
    // In a real implementation, you would access the game state and undo the last move
    executeCallback(); // Execute the callback if set
}

void GameModeSelectCommand::execute() {
    if (_versusScene) {
        std::cout << "Selected game mode: " << _gameMode << std::endl;
        _versusScene->selectGameMode(_gameMode);
    }
    executeCallback(); // Execute the callback if set
}

std::unique_ptr<ICommand> GameModeSelectCommand::clone() const {
    auto cloned = std::make_unique<GameModeSelectCommand>(_gameMode, _versusScene);
    cloned->_callback = _callback; // Copy the callback
    return cloned;
}

void ThemeSelectCommand::execute() {
    std::cout << "Selected theme: " << _theme << std::endl;
    
    // Apply theme change through SettingsState if available
    if (_settingsState) {
        _settingsState->setTheme(_theme);
    }
    
    executeCallback(); // Execute the callback if set
}

std::unique_ptr<ICommand> ThemeSelectCommand::clone() const {
    auto cloned = std::make_unique<ThemeSelectCommand>(_theme, _settingsState);
    cloned->_callback = _callback; // Copy the callback
    return cloned;
}

void MusicSelectCommand::execute() {
    std::cout << "Selected music: " << _music << std::endl;
    
    // Apply music change through SettingsState if available
    if (_settingsState) {
        _settingsState->setMusic(_music);
    }
    
    executeCallback(); // Execute the callback if set
}

std::unique_ptr<ICommand> MusicSelectCommand::clone() const {
    auto cloned = std::make_unique<MusicSelectCommand>(_music, _settingsState);
    cloned->_callback = _callback; // Copy the callback
    return cloned;
}

std::unique_ptr<ThemeSelectCommand> createThemeSelectCommand(const std::string& theme, SettingsState* settingsState) {
    return std::make_unique<ThemeSelectCommand>(theme, settingsState);
}

std::unique_ptr<MusicSelectCommand> createMusicSelectCommand(const std::string& music, SettingsState* settingsState) {
    return std::make_unique<MusicSelectCommand>(music, settingsState);
}

