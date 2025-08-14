#include "TestingScene.h"
#include "ResourceManager.h"
#include <iostream>
#include <raylib.h> // Assuming raylib is used for rendering
// #include "chess.h"

void TestingScene::init(void) {

}

void TestingScene::update(float deltaTime) {
  std::cout << "Updating TestingScene..." << std::endl;
}

void TestingScene::handleInput() {
  // Handle input for testing scene
  if (IsKeyPressed(KEY_T)) {
    std::cout << "T key pressed in TestingScene" << std::endl;
  }
  
  if (IsKeyPressed(KEY_ENTER)) {
    std::cout << "Enter key pressed in TestingScene" << std::endl;
  }
  
  // Handle mouse input
  if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
    Vector2 mousePos = GetMousePosition();
    std::cout << "Mouse clicked at: (" << mousePos.x << ", " << mousePos.y << ")" << std::endl;
  }


  Texture2D& chessBoard = ResourceManager::getInstance().getTexture2D("mainChessBoard");
  DrawTexturePro(chessBoard, 
                {0, 0, (float)chessBoard.width, (float)chessBoard.height},
                {300, 0, 100.0f, 100.0f},
                {0, 0}, 0.0f, WHITE); // Draw the chess board texture covering the whole screen
}

void TestingScene::render() {
  ClearBackground(Color{164, 204, 217, 255}); // rgb(164, 204, 217)
  // Texture2D& welcomeBackground = ResourceManager::getInstance().getTexture2D("welcomeImage");
  // DrawTexturePro(welcomeBackground, {0, 0, (float)welcomeBackground.width, (float)welcomeBackground.height},
  //                {300, 0, (float)GetScreenWidth() - 300, (float)GetScreenHeight()},
  //                {0, 0}, 0.0f, WHITE); // Draw the background image covering the whole screen

  // Font& font = ResourceManager::getInstance().getFont("public_sans_bold");
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
