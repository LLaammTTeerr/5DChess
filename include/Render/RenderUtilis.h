#pragma once
#include <raylib.h>
#include <string>
#include <vector>
#include "BoardView.h"


enum class MovePhase {
  SELECT_FROM_BOARD,
  SELECT_FROM_POSITION,
  SELECT_TO_BOARD,
  SELECT_TO_POSITION,
  END_MOVE
};


struct RenderMoveState {
  std::shared_ptr<BoardView> selectedBoardView; // Board where the move is being made
  Chess::Position2D selectedPosition; // Position on the selected board
  std::shared_ptr<BoardView> targetBoardView; // Board where the move is being targeted
  Chess::Position2D targetPosition; // Position on the target board
  MovePhase currentPhase; // Current phase of the move (selecting from board, position

  RenderMoveState()
      : selectedBoardView(nullptr), targetBoardView(nullptr), currentPhase(MovePhase::SELECT_FROM_BOARD),
        selectedPosition{-1, -1}, targetPosition{-1, -1} {}

  ~RenderMoveState() = default;
};