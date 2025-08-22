#pragma once
#include "ICommand.h"
#include "gameState.h"
#include <functional>

// Forward declarations
class SceneManager;
class VersusScene;
class Scene;

// Base class for commands that change game state
class ChangeStateCommand : public ICommand {
protected:
  GameStateModel* _gameStateModel;
  SceneManager* _sceneManager;
  std::function<std::unique_ptr<GameState>()> _stateFactory;
  std::string _commandName;

public:
  ChangeStateCommand(GameStateModel* gameStateModel, SceneManager* sceneManager, 
                     std::function<std::unique_ptr<GameState>()> stateFactory,
                     const std::string& commandName)
      : _gameStateModel(gameStateModel), _sceneManager(sceneManager), 
        _stateFactory(stateFactory), _commandName(commandName) {}

  virtual ~ChangeStateCommand() = default;

  // Common implementation for state-changing commands
  virtual bool canUndo() const override { return false; }
  virtual bool canRedo() const override { return false; }
  virtual void undo() override {}
  virtual void redo() override {}
  virtual CommandType getType() const override { return CommandType::STATE_CHANGING; }
  virtual std::string getName() const override { return _commandName; }

  // Execute implementation is in .cpp file to avoid forward declaration issues
  virtual void execute() override;

  virtual std::unique_ptr<ICommand> clone() const override {
    return std::make_unique<ChangeStateCommand>(_gameStateModel, _sceneManager, _stateFactory, _commandName);
  }
};

// class startGameCommand : public ICommand {
// private:
//   GameStateModel* _gameState;
// public:
//   startGameCommand(GameStateModel* gameState);

//   void execute() override;
//   virtual bool canUndo() const override { return false; }
//   virtual bool canRedo() const override { return false; }
//   void undo() override {}
//   void redo() override {}
//   std::string getName() const override { return "Start Game Command"; }
//   std::unique_ptr<ICommand> clone() const override; 
// };



// Factory function declarations
std::unique_ptr<ChangeStateCommand> createVersusCommand(
    GameStateModel* gameStateModel, SceneManager* sceneManager);

std::unique_ptr<ChangeStateCommand> createVersusBackCommand(
    GameStateModel* gameStateModel, SceneManager* sceneManager);

std::unique_ptr<ChangeStateCommand> createVersusPlayCommand(
    GameStateModel* gameStateModel, SceneManager* sceneManager, 
    const std::string& selectedGameMode);

std::unique_ptr<ChangeStateCommand> createBackToVersusCommand(
    GameStateModel* gameStateModel, SceneManager* sceneManager);

class ChessController; // Forward declaration
class SubmitMoveCommand  : public ICommand {
public:
  SubmitMoveCommand();
  void execute() override;
  virtual bool canUndo() const override { return false; }
  virtual bool canRedo() const override { return false; }
  void undo() override {}
  void redo() override {}
  std::string getName() const override { return "Submit Move Command"; }
  std::unique_ptr<ICommand> clone() const override;
  CommandType getType() const override { return CommandType::IMMEDIATE; }
};

class UndoMoveCommand : public ICommand {
public: 
  UndoMoveCommand() {}
  void execute() override;
  virtual bool canUndo() const override { return true; }
  virtual bool canRedo() const override { return false; }
  void undo() override {}
  void redo() override {}
  std::string getName() const override { return "Undo Move Command"; }
  std::unique_ptr<ICommand> clone() const override;
  CommandType getType() const override { return CommandType::NON_STATE; }
};


class GameModeSelectCommand : public ICommand {
private:
  std::string _gameMode;
  VersusScene* _versusScene;
  
public:
  GameModeSelectCommand(const std::string& gameMode, VersusScene* versusScene)
      : _gameMode(gameMode), _versusScene(versusScene) {}
      
  void execute() override;
  virtual bool canUndo() const override { return false; }
  virtual bool canRedo() const override { return false; }
  void undo() override {}
  void redo() override {}
  std::string getName() const override { return "Game Mode Select Command"; }
  std::unique_ptr<ICommand> clone() const override;
  CommandType getType() const override { return CommandType::IMMEDIATE; }
};

class DeselectMoveCommand : public ICommand {
public:
  DeselectMoveCommand() {}
  void execute() override { executeCallback(); } // Execute the callback if set
  virtual bool canUndo() const override { return false; }
  virtual bool canRedo() const override { return false; }
  void undo() override {}
  void redo() override {}
  std::string getName() const override { return "Deselect Move Command"; }
  std::unique_ptr<ICommand> clone() const override;
  CommandType getType() const override { return CommandType::IMMEDIATE; }
};