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

private:
  std::vector<std::shared_ptr<BoardView>> _currentBoardViews2D;
  std::vector<std::shared_ptr<BoardView>> computeBoardView2DsFromModel() const; // compute and assign to _currentBoardViews
  void updateBoardView2DsFromModel();

  std::vector<std::shared_ptr<BoardView>> _currentBoardViews3D;
  std::vector<std::shared_ptr<BoardView>> computeBoardView3DsFromModel() const; // compute and assign to _currentBoardViews
  void updateBoardView3DsFromModel();


  std::vector<std::shared_ptr<Chess::Board>> _highlightedBoard;
  void resetHighlightedBoard() { _highlightedBoard.clear(); }
  void addHighlightedBoard(std::shared_ptr<Chess::Board> board) { _highlightedBoard.push_back(board); }
  std::vector<std::shared_ptr<BoardView>> computeHighlightedBoardView2Ds() const;

public:
  ChessController(ChessModel& m, ChessView& v);
  void update(float deltaTime);
  void handleInput();

private:
  void setupViewCallbacks();

  // Methods to update model state based on user input
  void handleSelectedPosition(Chess::SelectedPosition selectedPosition);

  std::shared_ptr<BoardView> getBoardViewFromModel(std::shared_ptr<Chess::Board> board);
  RenderMoveState convertModelToRenderState(const MoveState& moveState);


};