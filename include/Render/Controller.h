#pragma once
#include <memory>
#include <functional>

#include "chess.h"

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
};