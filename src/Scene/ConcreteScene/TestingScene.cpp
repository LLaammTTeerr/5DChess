#include "TestingScene.h"
#include "Render/UITheme.h"
#include <iostream>
#include <raylib.h> // Assuming raylib is used for rendering
#include "chess.h"
#include "Render/View.h"

TestingScene::TestingScene(const std::string& gameMode)
    : _gameModeSelected(gameMode) {}

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
  // Build the board views once so the first frame's input already has them to map clicks onto
  _chessController->update(0.0f);
}

void TestingScene::update(float deltaTime) {
  _chessController->update(deltaTime);
}

void TestingScene::onPointerBlocked() {
  _chessController->clearHover();
}

void TestingScene::handleInput() {
  _chessController->handleInput();
  // std::cout << "Handling input in TestingScene..." << std::endl;
}

void TestingScene::render() {
  ClearBackground(UI::Color::bg);
  UI::Cursor::beginFrame();
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

