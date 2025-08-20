#include "chess.h"
#include <vector>
#include <memory>
#include "Render/RenderUtilis.h"

#pragma once

// class ChessModel;
// class ChessView;
// class ChessController;


struct MoveState {
  std::shared_ptr<Chess::Board> selectedBoard; // Board where the move is being made
  Chess::Position2D selectedPosition; // Position on the selected board
  std::shared_ptr<Chess::Board> targetBoard; // Board where the move is being targeted
  Chess::Position2D targetPosition; // Position on the target board
  MovePhase currentPhase; // Current phase of the move (selecting from board, position

  MoveState()
      : selectedBoard(nullptr), targetBoard(nullptr), currentPhase(MovePhase::SELECT_FROM_BOARD),
        selectedPosition{-1, -1}, targetPosition{-1, -1} {}

  void reset() {
    selectedBoard = nullptr;
    targetBoard = nullptr;
    selectedPosition = {-1, -1};
    targetPosition = {-1, -1};
    currentPhase = MovePhase::SELECT_FROM_BOARD; // Reset to initial phase
  }
};

class ChessRenderController; // Forward declaration

/// @brief Adapter class to convert model data to view data
class ChessModel {
private:
  std::shared_ptr<Chess::IGame> _game;
  MoveState _currentMoveState;

public:
  friend class ChessController;
  ChessModel(std::shared_ptr<Chess::IGame> game) : _game(game) {
    // Initialize the turn state if needed
    initialize();
  }

  // Initialize game state (e.g., set up initial board)
  void initialize() {
    // This can be empty if the game is already initialized
    _currentMoveState.currentPhase = MovePhase::SELECT_FROM_BOARD; // Set initial move phase
  }

  // methods to apply turn and make moves
  // These methods will be called by the controller to update the game state
  void makeMove(const Chess::Move& move) { _game->makeMove(move); }
  void applyTurn() { _game->submitTurn(); }


  void selectFromBoard(std::shared_ptr<Chess::Board> board) {
    _currentMoveState.selectedBoard = board;
    _currentMoveState.currentPhase = MovePhase::SELECT_FROM_POSITION; // Move to next phase
  }

  void selectFromPosition(Chess::Position2D selectedPosition) {
    _currentMoveState.selectedPosition = selectedPosition;
    _currentMoveState.currentPhase = MovePhase::SELECT_TO_BOARD; // Move to next phase
  }

  void selectToBoard(std::shared_ptr<Chess::Board> board) {
    _currentMoveState.targetBoard = board;
    _currentMoveState.currentPhase = MovePhase::SELECT_TO_POSITION; // Move to next phase
  }

  void selectToPosition(Chess::Position2D selectedPosition) {
    _currentMoveState.targetPosition = selectedPosition;
    _currentMoveState.currentPhase = MovePhase::END_MOVE; // Move to end phase
  }


  MoveState getCurrentMoveState() const { return _currentMoveState; }

  std::vector<std::shared_ptr<Chess::TimeLine>> getTimeLines() const {return _game->getTimeLines(); }
  int getPresentHalfTurn() const { return _game->presentHalfTurn(); }
  int getPresentFullTurn() const { return _game->presentFullTurn(); }
};


