#pragma once
#include <memory>
#include <functional>
#include "Render/utilis.h"
#include "chess.h"
#include "Render/BoardView.h"

struct RenderMoveState {
  std::shared_ptr<BoardView> selectedBoardView; // Board where the move is being made
  Chess::Position2D selectedPosition; // Position on the selected board
  std::shared_ptr<BoardView> targetBoardView; // Board where the move is being targeted
  Chess::Position2D targetPosition; // Position on the target board
  MovePhase currentPhase; // Current phase of the move (selecting from board, position
};

class ChessModel;
class ChessView;
class ChessController {
private:
  ChessModel& model;
  ChessView& view;

public:
  ChessController(ChessModel& m, ChessView& v);
  void handleInput() {};

private:
  void setupViewCallbacks();
  void setupModelCallbacks();

  void handleSelectedBoard(std::shared_ptr<Chess::Board> board);
  void handleSelectedPosition(Chess::Position2D pos);
  // void handleMoveSelection(const Chess::Board& board, const Chess::Position2D& position);
  // void handleTargetSelection(const Chess::Board& board, const Chess::Position2D& position);
  // void handleKeyPress(int key);
  // void handleMoveSubmission();

  std::shared_ptr<BoardView> getBoardViewFromModel(std::shared_ptr<Chess::Board> board);
  RenderMoveState convertModelToRenderState(const MoveState& moveState);
};