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