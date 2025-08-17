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
  model.setTryMoveCallback([this]() {
    
  });
} 

void ChessController::handleSelectedBoard(std::shared_ptr<Chess::Board> board) {
  model.selectBoard(board);
}

void ChessController::handleSelectedPosition(Chess::Position2D pos) {
  model.selectPosition(pos);
}
