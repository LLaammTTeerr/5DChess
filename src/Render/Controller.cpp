#include "RenModel.h"
#include "Render/View.h"
#include "Render/Controller.h"

ChessController::ChessController(ChessModel& m, ChessView& v) : model(m), view(v) {
    setupViewCallbacks();
    setupModelCallbacks();
}

void ChessController::setupViewCallbacks() {

}


void ChessController::setupModelCallbacks() {
  model.setInvalidBoardSelectionCallback([this]() {
    std::shared_ptr<BoardView> selectedBoardView = getBoardViewFromModel(model.getSelectedBoard());
    if (selectedBoardView) {
      view.
    }
  });

  

  model.setMoveStateChangedCallback([this]() {
    // Notify the view about the move state change
    auto currentMoveState = model.getCurrentMoveState();

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