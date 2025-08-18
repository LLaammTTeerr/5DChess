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

}

void ChessController::handleInput() {
    view.handleInput();
}

void ChessController::setupViewCallbacks() {
  view.setMouseBoardClickCallback([this](std::shared_ptr<BoardView> boardView) {
      handleSelectedBoard(boardView);
  });

  view.setPositionClickCallback([this](Chess::Position2D pos) {
      handleSelectedPosition(pos);
  });
}


void ChessController::setupModelCallbacks() {
  model.setInvalidBoardSelectionCallback([this]() {
    view.queueUpdateInvalidBoardSelection();
  });



  model.setMoveStateChangedCallback([this]() {
    // Notify the view about the move state change
    auto currentMoveState = model.getCurrentMoveState();
    view.queueUpdateMoveState(convertModelToRenderState(currentMoveState));
  });
} 

void ChessController::handleSelectedBoard(std::shared_ptr<BoardView> boardView) {
    if (!boardView) {
        std::cerr << "Null BoardView encountered!" << std::endl;
        return;
    }

    // Notify the model about the selected board
    auto board = boardView->getBoard();

    if (!board) {
        std::cerr << "Selected BoardView has no board!" << std::endl;
        return;
    }


    model.selectBoard(board);
}

void ChessController::handleSelectedPosition(Chess::Position2D pos) {
  model.selectPosition(pos);
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
    for (int halfTurn = 0; halfTurn < timeLine->halfTurnNumber(); ++halfTurn) {
      auto board = timeLine->getBoardByHalfTurn(halfTurn);
      if (board) {
        auto boardView = std::make_shared<BoardView2D>(board);
        boardView->setBoardTexture(&ResourceManager::getInstance().getTexture2D("mainChessBoard"));
        boardView->setRenderArea({
            static_cast<float>(halfTurn) * (BOARD_WORLD_SIZE + HORIZONTAL_SPACING),
            static_cast<float>(timeLine->ID()) * (BOARD_WORLD_SIZE + VERTICAL_SPACING),
            BOARD_WORLD_SIZE,
            BOARD_WORLD_SIZE
        });
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