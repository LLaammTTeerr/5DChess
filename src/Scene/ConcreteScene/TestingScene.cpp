#include "TestingScene.h"
#include "ResourceManager.h"
#include <iostream>
#include <raylib.h> // Assuming raylib is used for rendering
#include "chess.h"
#include "Render/View.h"

TestingScene::TestingScene(const std::string& gameMode)
    : _gameModeSelected(gameMode) {
  init();
}

void TestingScene::init(void) {
  #define TRY_LOAD_MODE(X) if (_gameModeSelected == Chess::NameOfGame<X>::value) { _game = Chess::createGame<X>(); }
  TRY_LOAD_MODE(Chess::StandardGame);
  TRY_LOAD_MODE(Chess::CustomGameEmitBishop);
  TRY_LOAD_MODE(Chess::CustomGameEmitKnight);
  TRY_LOAD_MODE(Chess::CustomGameEmitQueen);
  TRY_LOAD_MODE(Chess::CustomGameEmitRook);
  TRY_LOAD_MODE(Chess::CustomGameKVB);
  TRY_LOAD_MODE(Chess::MiscGameTimeLineInvasion);
  TRY_LOAD_MODE(Chess::MiscGameTimeLineBattle);
  TRY_LOAD_MODE(Chess::MiscGameTimeLineFragment);
  #undef TRY_LOAD_MODE

  _chessModel = std::make_shared<ChessModel>(_game);
  _chessView = std::make_shared<ChessView>(Vector3{5000, 5000, 1});
  _chessController = std::make_shared<ChessController>(*_chessModel, *_chessView);

  // auto boardViews = _chessController->computeBoardView2DsFromModel();
  // for (auto& boardView : boardViews) {
  //   std::cout << "Adding BoardView with ID: " << boardView->getBoard()->getTimeLine()->ID() << std::endl;
  // }
  // _chessController->handleInput();
  // for (auto timeLine : _chessModel->getTimeLines()) {
  //   auto board = timeLine->back();
  //   if (board) {
  //     std::cout << "TimeLine ID: " << timeLine->ID() << ", Board Full Turn: " << board->fullTurnNumber() << ", Half Turn: " << board->halfTurnNumber() << std::endl;
  //   }
  // }

}

void TestingScene::update(float deltaTime) {
  // std::cout << "Updating TestingScene..." << std::endl;
}

void TestingScene::handleInput() {
  _chessController->handleInput();
  // std::cout << "Handling input in TestingScene..." << std::endl;
}

void TestingScene::render() {
  ClearBackground(Color{164, 204, 217, 255}); // rgb(164, 204, 217)
  _chessController->render();
}

void TestingScene::cleanup(void) {}

bool TestingScene::isActive(void) const { return _isActive; }

std::string TestingScene::getName(void) const { return "TestingScene"; }

std::string TestingScene::getGameStateName(void) const {
  return "TESTING";
}


void TestingScene::onEnter() { _isActive = true; }

void TestingScene::onExit() { _isActive = false; }

bool TestingScene::shouldTransition() const { return false; }

