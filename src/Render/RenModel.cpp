#include "RenModel.h"
#include "chess.h"
#include <memory>
#include <vector>


ChessModel::ChessModel(std::shared_ptr<Chess::Game> game) : _game(game) {
  // Initialize the turn state if needed
  initialize();
}

void ChessModel::initialize() {
  // Initialize game state (e.g., set up initial board)
  // This can be empty if the game is already initialized
  _currentMoveState.currentPhase = MovePhase::SELECT_FROM_BOARD; // Set initial move phase
}

void ChessModel::applyTurn() {
  _game->submitTurn();
} 

void ChessModel::makeMove(const Chess::Move& move) {
  _game->makeMove(move);
  // notifyMoveStateChanged(); // Notify observers that a move has been made
}

void ChessModel::notifyInvalidBoardSelection() {
  if (_onInvalidBoardSelection) {
    _onInvalidBoardSelection();
  }
}

void ChessModel::notifyMoveStateChanged() {
  if (_onMoveStateChanged) {
    _onMoveStateChanged();
  }
}

// void ChessModel::notifyGameUpdated() {
//   if (_onGameUpdated) {
//     _onGameUpdated();
//   }
// }

void ChessModel::selectBoard(std::shared_ptr<Chess::Board> board) {
  if (_currentMoveState.currentPhase != MovePhase::SELECT_FROM_BOARD &&
      _currentMoveState.currentPhase != MovePhase::SELECT_TO_BOARD) {
    
    notifyInvalidBoardSelection(); // Notify observers about invalid selection
    return; // Only allow selection in the appropriate phases
  }

  if (_currentMoveState.currentPhase == MovePhase::SELECT_FROM_BOARD) {
    if (!_game->canMakeMoveFromBoard(board)) {
      notifyInvalidBoardSelection();
      return;
    }

    _currentMoveState.selectedBoard = board;
    _currentMoveState.currentPhase = MovePhase::SELECT_FROM_POSITION; // Move to next phase

  } else if (_currentMoveState.currentPhase == MovePhase::SELECT_TO_BOARD) {
    std::vector<Chess::SelectedPosition> moveablePositions = _game->getMoveablePositions(
      Chess::SelectedPosition(board, _currentMoveState.selectedPosition)
    );

    if (moveablePositions.empty()) {
      notifyInvalidBoardSelection();
      return; // No valid positions to move to
    }
    // find if the target board is in the moveable positions
    bool found = false;
    for (const auto& pos : moveablePositions) {
      if (pos.board == board) {
        found = true;
        break;
      }
    }
    if (!found) {
      notifyInvalidBoardSelection();
      return; // Target board is not valid for the current move
    }

    _currentMoveState.targetBoard = board;
    _currentMoveState.currentPhase = MovePhase::SELECT_TO_POSITION; // Move to next phase
  }

  notifyMoveStateChanged(); // Notify observers that the move state has changed
}

void ChessModel::selectPosition(Chess::Position2D position) {
  if (_currentMoveState.currentPhase != MovePhase::SELECT_FROM_POSITION &&
      _currentMoveState.currentPhase != MovePhase::SELECT_TO_POSITION) {
    return; // Only allow selection in the appropriate phases
  }

  if (_currentMoveState.currentPhase == MovePhase::SELECT_FROM_POSITION) {
    _currentMoveState.selectedPosition = position;
    _currentMoveState.currentPhase = MovePhase::SELECT_TO_BOARD; // Move to next phase
  } else if (_currentMoveState.currentPhase == MovePhase::SELECT_TO_POSITION) {
    _currentMoveState.targetPosition = position;
    _currentMoveState.currentPhase = MovePhase::END_MOVE;
  }

  notifyMoveStateChanged(); // Notify observers that the move state has changed
}



