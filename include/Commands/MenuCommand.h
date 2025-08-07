#pragma once
#include "ICommand.h"
#include "gameState.h"

class startGameCommand : public ICommand {
private:
  GameStateModel* _gameState;
public:
  startGameCommand(GameStateModel* gameState);

  void execute() override;
  virtual bool canUndo() const override { return false; }
  virtual bool canRedo() const override { return false; }
  void undo() override {}
  void redo() override {}
  std::string getName() const override { return "Start Game Command"; }
  std::unique_ptr<ICommand> clone() const override; 
};