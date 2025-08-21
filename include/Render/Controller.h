#pragma once
#include <memory>
#include <functional>
#include <map>
#include "Render/RenderUtilis.h"
#include "chess.h"
#include "Render/BoardView.h"
#include "View.h"
#include "RenModel.h"

class InGameMenuController;
class MenuComponent;
class ChessController {
private:
  ChessModel& model;
  ChessView& view;

  std::shared_ptr<InGameMenuController> _inGameMenuController;
  std::shared_ptr<MenuComponent> _inGameMenuSystem;
private:
  void initInGameMenu();
  void renderInGameMenu() const;
  void updateMenuButtonStates(); // Update menu button enabled/disabled states based on game state

/// @brief private attribute and methods related to model
private:
  /// @brief current Boards
  std::vector<std::shared_ptr<Chess::Board>> _currentBoard;
  std::vector<std::shared_ptr<Chess::Board>> computeCurrentBoardFromModel() const;
  void updateCurrentBoardFromModel();
  
  /// @brief highlight Boards of current Boards
  std::vector<std::shared_ptr<Chess::Board>> _highlightedBoard;
  void resetHighlightedBoard() { _highlightedBoard.clear(); }
  void addHighlightedBoard(std::shared_ptr<Chess::Board> board) { _highlightedBoard.push_back(board); }

  ///@brief highlighted positions
  std::vector<Chess::SelectedPosition> _highlightedPositions;
  void resetHighlightedPositions() { _highlightedPositions.clear(); }
  void addHighlightedPosition(Chess::SelectedPosition position) { _highlightedPositions.push_back(position); }

/// @brief attribute and methods related to view
private:
  std::string _currentBoardType = "2D";
  /// @brief render attribute for current boards
  std::vector<std::shared_ptr<BoardView>> _currentBoardViews; // current board views in the view
  std::vector<std::shared_ptr<BoardView>> computeBoardViewFromCurrentBoards(std::string boardType) const;
  void updateBoardViewFromCurrentBoards();
  void updateNewBoardViewsToView();
  /// @brief helper of computeBoardViewFromModel()
  std::vector<std::shared_ptr<BoardView>> computeBoardView2DsFromCurrentBoards() const; // compute and assign to _currentBoardViews
  std::vector<std::shared_ptr<BoardView>> computeBoardView3DsFromCurrentBoards() const; // compute and assign to _currentBoardViews


/// @brief render attribute and methods for highlighted boards
private:   
  std::vector<std::shared_ptr<BoardView>> computeHighlightedBoardViews() const;

// bridge between model and view
private:
  std::map<std::shared_ptr<Chess::Board>, std::shared_ptr<BoardView>> _boardToBoardViewMap; // Map to store board views by board
  std::map<std::shared_ptr<BoardView>, std::shared_ptr<Chess::Board>> _boardViewToBoardMap; // Map to store boards by board view

public:
  ChessController(ChessModel& m, ChessView& v);
  void update(float deltaTime);
  void handleInput();
  void render();

private:
  void setupViewCallbacks();

  // Methods to update model state based on user input
  void handleSelectedPosition(Chess::SelectedPosition selectedPosition);
  void handleUndoMove();
  void handleSubmitMove();
  void handleDeselectPosition();
  // RenderMoveState convertModelToRenderState(const MoveState& moveState);
};