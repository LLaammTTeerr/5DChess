#include "TestingScene.h"
#include "ResourceManager.h"
#include <iostream>
#include <raylib.h> // Assuming raylib is used for rendering
#include "chess.h"
#include "Render/View.h"

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

}

void TestingScene::render() {
  ClearBackground(Color{164, 204, 217, 255}); // rgb(164, 204, 217)

  // Font& font = ResourceManager::getInstance().getFont("public_sans_bold");
  Texture2D& chessBoardTexture = ResourceManager::getInstance().getTexture2D("mainChessBoard");
  // // Create chess game model
  std::shared_ptr<Chess::TimeLine> timeLine = std::make_shared<Chess::TimeLine>(8, 1);
  std::shared_ptr<Chess::Board> board1 = Chess::BoardBuilder::buildStandardBoard();
  std::shared_ptr<Chess::Board> board2 = Chess::BoardBuilder::buildStandardBoard();

  // Create multiple BoardView2D instances to test with GameWorld
  std::shared_ptr<BoardView> boardView1 = std::make_shared<BoardView2D>(board1, &chessBoardTexture);
  std::shared_ptr<BoardView> boardView2 = std::make_shared<BoardView2D>(board2, &chessBoardTexture);
    
  // Set up render areas for the boards (side by side)
  auto boardView1_2D = std::static_pointer_cast<BoardView2D>(boardView1);
  auto boardView2_2D = std::static_pointer_cast<BoardView2D>(boardView2);
    
  boardView1_2D->setRenderArea(Rectangle{50, 50, 300, 300});
  boardView2_2D->setRenderArea(Rectangle{400, 50, 300, 300});
    
  // boardView1 -> render();
  // boardView2 -> render();
  // // Activate the boards
  boardView1->setActive(true);
  boardView2->setActive(true);

  // Create GameWorld and add board views
  GameWorld gameWorld(Vector3{2000, 2000, 1});
  gameWorld.addBoardView(boardView1);
  gameWorld.addBoardView(boardView2);

  
  // // Set up callbacks for testing
  gameWorld.setOnMousePositionCallback([](Chess::Position2D pos) {
      std::cout << "Mouse over position: (" << pos.x() << ", " << pos.y() << ")" << std::endl;
  });
    
  gameWorld.setOnMouseClickCallback([](Vector2 pos) {
      std::cout << "Mouse clicked at: (" << pos.x << ", " << pos.y << ")" << std::endl;
  });

  // // Test highlighting some positions
  std::vector<Chess::Position2D> highlightPositions = {
      Chess::Position2D(2, 2),
      Chess::Position2D(4, 4),
      Chess::Position2D(6, 6)
  };
  boardView1->updateHighlightedPositions(highlightPositions);
  gameWorld.update(GetFrameTime());
  gameWorld.handleInput();
  gameWorld.render();
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
