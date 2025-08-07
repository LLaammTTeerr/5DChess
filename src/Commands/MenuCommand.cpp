#include "MenuCommand.h"

startGameCommand::startGameCommand(GameStateModel* gameState)
    : _gameState(gameState) {}

void startGameCommand::execute() {
    if (_gameState) {
        _gameState->setState(GameStateModel::State::IN_GAME);
        std::cout << "Game started." << std::endl;
    }
}

std::unique_ptr<ICommand> startGameCommand::clone() const {
    // create a new instance of startGameCommand with the same game state
    return std::make_unique<startGameCommand>(_gameState);
}