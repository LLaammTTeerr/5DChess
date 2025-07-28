#pragma once

class Controller;
class GameModel;
class Graphic;


class GameModel {
private:
  Controller* _controller;
public:
  void makeMove();
  void addTimeline();
  void getValidMoves();
};

class Graphic {
private:
  Controller* _controller;
public:
  void renderBoard();
  void showTimeline();
  void highlightMoves(); 

  void handleInput();
  void notifyInput();
  void displayMenu();
  
};

class Controller {
private:
  GameModel* _game;
  Graphic* _graphic;

  std::vector<std::vector<pair<int, int>>> calculateBoardPositions();
public:
  Controller(GameModel* game, Graphic* graphic);


  void getInput() {
    // Handle user input
    _graphic->handleInput();
    _graphic->notifyInput();
  }

  void handleInputData() {
    // Process user input data
  }
}


std::vector<std::vector<pair<int, int>>> Controller::calculateBoardPositions() {  
  /// 
}