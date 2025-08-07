#include "gameState.h"

std::string GameStateModel::getCurrentStateName() const {
    switch (_currentState) {
      case State::MAIN_MENU: return "Main Menu";
      case State::SETTINGS_MENU: return "Settings Menu";
      case State::IN_GAME: return "In Game";
      case State::PAUSED: return "Paused";
      default: return "Unknown State";
    }
}