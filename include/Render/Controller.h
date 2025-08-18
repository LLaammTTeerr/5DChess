#pragma once
#include <memory>
#include <functional>
#include "Render/RenderUtilis.h"
#include "chess.h"
#include "Render/BoardView.h"



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

  void handleSelectedBoard(std::shared_ptr<BoardView> board);
  void handleSelectedPosition(Chess::Position2D pos);

  std::shared_ptr<BoardView> getBoardViewFromModel(std::shared_ptr<Chess::Board> board);
  RenderMoveState convertModelToRenderState(const MoveState& moveState);
};