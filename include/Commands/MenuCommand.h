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