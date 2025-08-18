#include "RenModel.h"
#include "Render/View.h"
#include "Render/Controller.h"

ChessController::ChessController(ChessModel& m, ChessView& v) : model(m), view(v) {
    setupViewCallbacks();
    setupModelCallbacks();
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