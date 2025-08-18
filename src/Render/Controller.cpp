#include "RenModel.h"
#include "Render/View.h"
#include "Render/Controller.h"
#include "ResourceManager.h"

ChessController::ChessController(ChessModel& m, ChessView& v) : model(m), view(v) {
    setupViewCallbacks();
    setupModelCallbacks();
}

void ChessController::update(float deltaTime) {
  // updateBoardViews base on current model state
  updateBoardView2DsFromModel();
  updateBoardView3DsFromModel();
}

void ChessController::handleInput() {
    update(GetFrameTime());
    view.handleInput();
}

void ChessController::setupViewCallbacks() {
  view.setSelectedPositionCallback([this](Chess::SelectedPosition selectedPosition) {
    // Handle the selected position from the view
    handleSelectedPosition(selectedPosition);
  });
}


void ChessController::setupModelCallbacks() {
  // model.setInvalidBoardSelectionCallback([this]() {
  //   view.queueUpdateInvalidBoardSelection();
  // });


  // model.setMoveStateChangedCallback([this]() {
  //   // Notify the view about the move state change
  //   auto currentMoveState = model.getCurrentMoveState();
  //   view.queueUpdateMoveState(convertModelToRenderState(currentMoveState));
  // });
} 

void ChessController::handleSelectedPosition(Chess::SelectedPosition selectedPosition) {
  model.selectPosition(selectedPosition);
}

std::shared_ptr<BoardView> ChessController::getBoardViewFromModel(std::shared_ptr<Chess::Board> board) {
    for (const auto& boardView : view.getBoardViews()) {
        if (boardView->getBoard() == board) {
            return boardView;
        }
    }
    return nullptr; // Not found
}

RenderMoveState ChessController::convertModelToRenderState(const MoveState& moveState) {
    RenderMoveState renderState;
    renderState.selectedBoardView = getBoardViewFromModel(moveState.selectedBoard);
    renderState.selectedPosition = moveState.selectedPosition;
    renderState.targetBoardView = getBoardViewFromModel(moveState.targetBoard);
    renderState.targetPosition = moveState.targetPosition;
    renderState.currentPhase = moveState.currentPhase;

    return renderState;
}

std::vector<std::shared_ptr<BoardView>> ChessController::computeBoardView2DsFromModel() const {
  std::vector<std::shared_ptr<BoardView>> boardViews;
  auto timeLines = model.getTimeLines();

  // create new board views based on the timelines
  for (const auto& timeLine : timeLines) {
    std::vector<std::shared_ptr<Chess::Board>> boards = timeLine->getBoards();
    for (auto& board : boards){
      if (board) {
        auto boardView = std::make_shared<BoardView2D>(board);
        boardView->setBoardTexture(&ResourceManager::getInstance().getTexture2D("mainChessBoard"));
        boardView->setRenderArea({
            static_cast<float>(board->halfTurnNumber()) * (BOARD_WORLD_SIZE + HORIZONTAL_SPACING),
            static_cast<float>(timeLine->ID()) * (BOARD_WORLD_SIZE + VERTICAL_SPACING),
            BOARD_WORLD_SIZE,
            BOARD_WORLD_SIZE
        });
        boardViews.push_back(boardView);
      }
    }
  }

  return boardViews;
}

std::vector<std::shared_ptr<BoardView>> ChessController::computeBoardView3DsFromModel() const {
  return std::vector<std::shared_ptr<BoardView>>(); // Placeholder for 3D board views
}

  // Map timelines to their indices for Y positioning
  // std::unordered_map<std::shared_ptr<Chess::TimeLine>, int> timelineToIndex;
  // for (size_t i = 0; i < timeLines.size(); ++i) {
  //   timelineToIndex[timeLines[i]] = static_cast<int>(i);
  // }
  // For each board view, compute its render area
  // for (const auto& boardView : view.getBoardViews()) {
  //   auto board = boardView->getBoard();
  //   if (!board) continue;

  //   auto timeLine = board->getTimeLine();
  //   auto it = timelineToIndex.find(timeLine);
  //   if (it == timelineToIndex.end()) continue;

  //   int tlIndex = it->second;
  //   int halfTurn = board->halfTurnNumber();

  //   float x = static_cast<float>(halfTurn) * (BOARD_WORLD_SIZE + HORIZONTAL_SPACING);
  //   float y = static_cast<float>(tlIndex) * (BOARD_WORLD_SIZE + VERTICAL_SPACING);

  //   Rectangle area = {x, y, BOARD_WORLD_SIZE, BOARD_WORLD_SIZE};
  //   positions.push_back({boardView, area});
  // }

// }


void ChessController::updateBoardView2DsFromModel() {
  view.clearBoardViews();
  
  auto boardViews = computeBoardView2DsFromModel();
  for (auto& boardView : boardViews) {
    view.addBoardView(boardView);
  }
}


void ChessController::updateBoardView3DsFromModel() {
  // Placeholder for updating 3D board views
  // Currently, this method does nothing as we are not implementing 3D views yet
}