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

/// @brief Data structure for present line information passed from Controller
struct PresentLineData {
    float halfTurnPosition; // The half turn position where the present line should be drawn
    Color color;           // Color of the present line
    float thickness;       // Thickness of the line
    bool isVisible;        // Whether the line should be visible
    
    PresentLineData() 
        : halfTurnPosition(0.0f), color(YELLOW), thickness(20.0f), isVisible(true) {}
        
    PresentLineData(float position, Color col = YELLOW, float thick = 20.0f, bool visible = true)
        : halfTurnPosition(position), color(col), thickness(thick), isVisible(visible) {}
};