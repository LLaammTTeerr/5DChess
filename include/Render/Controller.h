#pragma once
#include <memory>
#include <functional>
#include <map>
#include <algorithm>
#include "Render/RenderUtilis.h"
#include "chess.h"
#include "Render/BoardView.h"
#include "View.h"
#include "RenModel.h"
#include "ui/Widgets.h"

// Forward declarations
struct TimelineArrowData; // Forward declaration for timeline arrows
class ChessController {
private:
  ChessModel& model;
  ChessView& view;

  // The Undo / Deselect / Submit row under the HUD pill
  ui::Button _undo{"Undo", {}}, _deselect{"Deselect", {}}, _submit{"Submit", {}, true};
private:
  bool _isGameEnd = false;
private:
  void layoutButtons();
  void updateButtonStates();
  std::vector<std::shared_ptr<Chess::Board>> _moveableBoards; // refreshed by updateButtonStates()
  /// canSubmit() and "no mandatory board left" are not free (a threat search over the multiverse), and the HUD and the
  /// menu ask every frame: cache them until the game's stateVersion() changes.
  struct TurnStatus { bool canSubmit = false; bool mandatoryEmpty = true; };
  const TurnStatus& turnStatus() const;
  mutable TurnStatus _turnStatus;
  mutable const Chess::IGame* _turnStatusGame = nullptr;
  mutable unsigned long long _turnStatusVersion = 0;
  HudData computeHud() const; // Update menu button enabled/disabled states based on game state

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
  /// Updates the action buttons (they take the pointer first), then the board input.
  void handleInput(float deltaTime);
  /// Developer tools / scripted demos: drive the same paths a click would
  void scriptedSelect(Chess::SelectedPosition p) { handleSelectedPosition(p); }
  void scriptedSubmit() { handleSubmitMove(); }
  void render() const;

private:
  void setupViewCallbacks();

  // Timeline arrow computation methods
  std::vector<TimelineArrowData> computeTimelineArrows() const;
  std::vector<TimelineArrowData> computeProgressionArrows() const;
  std::vector<TimelineArrowData> computeBranchingArrows() const;

  // Present line computation methods
  PresentLineData computePresentLine() const;

  // Methods to update model state based on user input
  void handleSelectedPosition(Chess::SelectedPosition selectedPosition);
  void handleMouseOverPosition(Chess::SelectedPosition selectedPosition);
  
  bool handleSelectedFromBoard(Chess::SelectedPosition selectedPosition);
  void handleSelectedFromPosition(Chess::SelectedPosition selectedPosition);
  bool handleSelectedToBoard(Chess::SelectedPosition selectedPosition);
  void handleSelectedToPosition(Chess::SelectedPosition selectedPosition);

  void handleUndoMove();
  void handleSubmitMove();
  void handleDeselectPosition();
  void clearSelection(); // reset in-progress move state and view highlights
};