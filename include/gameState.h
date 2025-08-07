#pragma once
#include <string>
#include <memory>
#include <vector>
#include <iostream>


class GameStateModel {
public:
  enum class State {
    MAIN_MENU,
    SETTINGS_MENU,
    IN_GAME,
    PAUSED,
    NONE // Placeholder for no state
  };

private:
  State _currentState = State::MAIN_MENU;
  int _volume = 50; // Default volume level
  std::string _gameMode = "Normal"; // Default game mode

public:
  void setState(State state) { _currentState = state; };
  State getState() const { return _currentState; };
  void setVolume(int vol) { _volume = vol; };
  int getVolume() const { return _volume; };
  void setGameMode(const std::string& mode) { _gameMode = mode; };
  const std::string& getGameMode() const { return _gameMode; };
  std::string getCurrentStateName() const;
};
