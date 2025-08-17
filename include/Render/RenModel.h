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
      : selectedBoard(nullptr), targetBoard(nullptr), currentPhase(MovePhase::SELECT_FROM_BOARD) {}
};

class ChessModel {
private:
  std::shared_ptr<Chess::Game> _game;
  MoveState _currentMoveState;

  std::function<void()> _onMoveStateChanged; // Callback for move phase changes
  std::function<void()> _onInvalidBoardSelection; // Callback for invalid board selection
  // std::function<void()> _onGameUpdated; // Callback for game updates

public:
  ChessModel(std::shared_ptr<Chess::Game> game);

  // Initialize game state (e.g., set up initial board)
  void initialize();

  // methods to apply turn and make moves
  // These methods will be called by the controller to update the game state
  void makeMove(const Chess::Move& move); 
  void applyTurn();

  void setMoveStateChangedCallback(std::function<void()> callback) {_onMoveStateChanged = callback; }
  void setInvalidBoardSelectionCallback(std::function<void()> callback) {_onInvalidBoardSelection = callback; }
  // void setGameUpdatedCallback(std::function<void()> callback) {_onGameUpdated = callback; }

  void notifyInvalidBoardSelection();
  void notifyMoveStateChanged();
  // void notifyGameUpdated();

  void selectBoard(std::shared_ptr<Chess::Board> board);
  void selectPosition(Chess::Position2D pos);

  MoveState getCurrentMoveState() const { return _currentMoveState; }
};


