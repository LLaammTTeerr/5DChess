#include "RenModel.h"
#include "Render/View.h"
#include "Render/Controller.h"
#include "ResourceManager.h"

ChessController::ChessController(ChessModel& m, ChessView& v) : model(m), view(v) {
    setupViewCallbacks();
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


void ChessController::handleSelectedPosition(Chess::SelectedPosition selectedPosition) {
  MoveState currentMoveState = model._currentMoveState;
  if (currentMoveState.currentPhase == MovePhase::SELECT_FROM_BOARD) {
    // Select the board from which to move
    // Check if the selected position is valid: can move from the selected position
    if (!model._game->canMakeMoveFromBoard(selectedPosition.board)) {
      std::cout << "Invalid selection: cannot make move from the selected board." << std::endl;
      return; // Invalid selection
    }

    model.selectFromBoard(selectedPosition.board);
    addHighlightedBoard(selectedPosition.board);
    view.update_highlightedBoard(computeHighlightedBoardView2Ds());

  } else if (currentMoveState.currentPhase == MovePhase::SELECT_FROM_POSITION) {
    // Select the position on the selected board
    model.selectFromPosition(selectedPosition.position);
    
  } else if (currentMoveState.currentPhase == MovePhase::SELECT_TO_BOARD) {
    // Select the target board for the move
    model.selectToBoard(selectedPosition.board);
  } else if (currentMoveState.currentPhase == MovePhase::SELECT_TO_POSITION) {
    // Select the target position on the target board
    model.selectToPosition(selectedPosition.position);
  }

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


void ChessController::updateBoardView2DsFromModel() {
  _currentBoardViews2D = computeBoardView2DsFromModel();
  
  view.clearBoardViews();
  for (auto& boardView : _currentBoardViews2D) {
    view.addBoardView(boardView);
  }
}


void ChessController::updateBoardView3DsFromModel() {
  // Placeholder for updating 3D board views
  // Currently, this method does nothing as we are not implementing 3D views yet
}

std::vector<std::shared_ptr<BoardView>> ChessController::computeHighlightedBoardView2Ds() const {
    std::vector<std::shared_ptr<BoardView>> highlightedViews;
    for (const auto& boardView : _currentBoardViews2D) {
      if (boardView && boardView->getBoard() && 
          std::find(_highlightedBoard.begin(), _highlightedBoard.end(), boardView->getBoard()) != _highlightedBoard.end()) {
        highlightedViews.push_back(boardView);
      }
    }
    return highlightedViews;
  }