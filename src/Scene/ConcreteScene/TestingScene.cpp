#include "TestingScene.h"
#include "ResourceManager.h"
#include <iostream>
#include <raylib.h> // Assuming raylib is used for rendering
#include "chess.h"
#include "Render/View.h"

TestingScene::TestingScene() {
  init();
}

void TestingScene::init(void) {
  _game = Chess::createGame<Chess::StandardGame>();
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
  // // Font& font = ResourceManager::getInstance().getFont("public_sans_bold");
  // Texture2D& chessBoardTexture = ResourceManager::getInstance().getTexture2D("mainChessBoard");
  // // // Create chess game model
  // std::shared_ptr<Chess::TimeLine> timeLine = std::make_shared<Chess::TimeLine>(8, 1);
  // std::shared_ptr<Chess::Board> board1 = Chess::BoardBuilder::buildStandardBoard();
  // std::shared_ptr<Chess::Board> board2 = Chess::BoardBuilder::buildStandardBoard();

  // // Create multiple BoardView2D instances to test with ChessView
  // std::shared_ptr<BoardView> boardView1 = std::make_shared<BoardView2D>(board1, &chessBoardTexture);
  // std::shared_ptr<BoardView> boardView2 = std::make_shared<BoardView2D>(board2, &chessBoardTexture);

  // // Set up render areas for the boards (side by side)
  // auto boardView1_2D = std::static_pointer_cast<BoardView2D>(boardView1);
  // auto boardView2_2D = std::static_pointer_cast<BoardView2D>(boardView2);

  // boardView1_2D->setRenderArea(Rectangle{50, 50, 300, 300});
  // boardView2_2D->setRenderArea(Rectangle{400, 50, 300, 300});

  // // boardView1 -> render();
  // // boardView2 -> render();
  // // // Activate the boards
  // boardView1->setActive(true);
  // boardView2->setActive(true);

  // // Create ChessView and add board views
  // ChessView gameWorld(Vector3{2000, 2000, 1});
  // gameWorld.addBoardView(boardView1);
  // gameWorld.addBoardView(boardView2);


  // // // Set up callbacks for testing
  // gameWorld.setOnMousePositionCallback([](Chess::Position2D pos) {
  //     std::cout << "Mouse over position: (" << pos.x() << ", " << pos.y() << ")" << std::endl;
  // });

  // gameWorld.setOnMouseClickCallback([](Vector2 pos) {
  //     std::cout << "Mouse clicked at: (" << pos.x << ", " << pos.y << ")" << std::endl;
  // });

  // // // Test highlighting some positions
  // std::vector<Chess::Position2D> highlightPositions = {
  //     Chess::Position2D(2, 2),
  //     Chess::Position2D(4, 4),
  //     Chess::Position2D(6, 6)
  // };
  // boardView1->updateHighlightedPositions(highlightPositions);
  // gameWorld.update(GetFrameTime());
  // gameWorld.handleInput();
  // gameWorld.render();
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

