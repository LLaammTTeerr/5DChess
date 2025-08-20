#pragma once
#include "ICommand.h"
#include "gameState.h"


class SceneManager; // Forward declaration

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

class VersusCommand : public ICommand {
private:
  GameStateModel* _gameStateModel;
  SceneManager* _sceneManager; // Optional, if you want to notify SceneManager
public:
  VersusCommand(GameStateModel* gameStateModel, SceneManager* sceneManager)
      : _gameStateModel(gameStateModel), _sceneManager(sceneManager) {}
  void execute() override;
  virtual bool canUndo() const override { return false; }
  virtual bool canRedo() const override { return false; }
  void undo() override {}
  void redo() override {}
  std::string getName() const override { return "Versus Command"; }
  std::unique_ptr<ICommand> clone() const override;
  CommandType getType() const override;
};

class VersusBackCommand : public ICommand {
private:
  GameStateModel* _gameStateModel;
  SceneManager* _sceneManager; // Optional, if you want to notify SceneManager
public:
  VersusBackCommand(GameStateModel* gameStateModel, SceneManager* sceneManager)
      : _gameStateModel(gameStateModel), _sceneManager(sceneManager) {}
  void execute() override;
  virtual bool canUndo() const override { return false; }
  virtual bool canRedo() const override { return false; }
  void undo() override {}
  void redo() override {}
  std::string getName() const override { return "Back Command"; }
  std::unique_ptr<ICommand> clone() const override;
  CommandType getType() const override;
};

class VersusPlayCommand : public ICommand {
private:
  GameStateModel* _gameStateModel;
  SceneManager* _sceneManager; // Optional, if you want to notify SceneManager
public:
  VersusPlayCommand(GameStateModel* gameStateModel, SceneManager* sceneManager)
      : _gameStateModel(gameStateModel), _sceneManager(sceneManager) {} 
  void execute() override;
  virtual bool canUndo() const override { return false; }
  virtual bool canRedo() const override { return false; }
  void undo() override {}
  void redo() override {} 
  std::string getName() const override { return "Versus Play Command"; }
  std::unique_ptr<ICommand> clone() const override;
  CommandType getType() const override;
  
};

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