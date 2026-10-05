#include "Render/Controller.h"
#include "ResourceManager.h"
#include "MenuController.h"
#include "MenuComponent.h"
#include "MenuCommand.h"
#include "MenuView.h"
#include "MenuItemView.h"
#include "Render/UITheme.h"




ChessController::ChessController(ChessModel& m, ChessView& v) : model(m), view(v) {
    setupViewCallbacks();
    initInGameMenu();
}

void ChessController::updateCurrentBoardFromModel() {
  _currentBoard = computeCurrentBoardFromModel();
}


void ChessController::updateBoardViewFromCurrentBoards() {
  _currentBoardViews = computeBoardViewFromCurrentBoards(_currentBoardType);
}

void ChessController::updateNewBoardViewsToView() {
  view.clearBoardViews();
  for (const auto& boardView : _currentBoardViews) if (boardView) {
    view.addBoardView(boardView);
  } 
}

void ChessController::update(float deltaTime) {
  updateCurrentBoardFromModel();
  updateBoardViewFromCurrentBoards();
  // after updating the board and board views, we need bridge the board to board view
  _boardToBoardViewMap.clear();
  _boardViewToBoardMap.clear();
  for (int i = 0; i < _currentBoard.size() && i < _currentBoardViews.size(); ++i) {
    _boardToBoardViewMap[_currentBoard[i]] = _currentBoardViews[i];
    _boardViewToBoardMap[_currentBoardViews[i]] = _currentBoard[i];
    
    // Set board reference in board view for timeline arrows
    _currentBoardViews[i]->setBoard(_currentBoard[i]);
  }
  updateNewBoardViewsToView();
  
  // Compute and update timeline arrows through proper MVC pattern
  auto timelineArrowData = computeTimelineArrows();
  view.updateTimelineArrows(timelineArrowData);
  
  // Compute and update present line through proper MVC pattern
  auto presentLineData = computePresentLine();
  view.updatePresentLine(presentLineData);
  
  // Update menu button states based on current game state
  updateMenuButtonStates();

  // Boards the current player may still move from get an accent border
  for (const auto& board : _currentBoard) {
    auto it = _boardToBoardViewMap.find(board);
    if (it == _boardToBoardViewMap.end() || !it->second) continue;
    bool moveable = !model._game->gameEnd() &&
        std::find(_moveableBoards.begin(), _moveableBoards.end(), board) != _moveableBoards.end();
    it->second->setMoveable(moveable);
  }
  view.updateHud(computeHud());

  if (model._game->gameEnd()) {
    _isGameEnd = true;
    return;
  }
}

void ChessController::handleInput() {
    // The in-game menu gets first dibs on the mouse: no board selection under a menu button
    bool mouseOverMenu = _inGameMenuController && _inGameMenuController->isMouseOverMenu();
    view.handleInput(mouseOverMenu);
    if (_inGameMenuController) {
        _inGameMenuController->handleInput();
    }
}

void ChessController::setupViewCallbacks() {
  view.setSelectedPositionCallback([this](std::pair<std::shared_ptr<BoardView>, Chess::Position2D> selectedPositionPair) {
    // Handle the selected position from the view
    Chess::SelectedPosition selectedPosition = {
        _boardViewToBoardMap[selectedPositionPair.first], // Get the board from the BoardView
        selectedPositionPair.second // Get the position from the pair
    };
    handleSelectedPosition(selectedPosition);
  });

  view.setMouseOverPositionCallback([this](std::pair<std::shared_ptr<BoardView>, Chess::Position2D> selectedPositionPair) {
    // Handle the selected position from the view
    Chess::SelectedPosition selectedPosition = {
        _boardViewToBoardMap[selectedPositionPair.first], // Get the board from the BoardView
        selectedPositionPair.second // Get the position from the pair
    };
    handleMouseOverPosition(selectedPosition);

  });
}

void ChessController::handleMouseOverPosition(Chess::SelectedPosition selectedPosition) {
  // The hover square is tracked and drawn by ChessView (accent @ 25%); nothing to do in the model.
  (void)selectedPosition;
}

bool ChessController::handleSelectedFromBoard(Chess::SelectedPosition selectedPosition) {
    // Select the board from which to move
    /// @brief Step 1: Check if the selected board is valid
    if (!selectedPosition.board || !model._game->canMakeMoveFromBoard(selectedPosition.board)) {
      std::cout << "Invalid selection: cannot make move from the selected board." << std::endl;
      return false; // Invalid selection
    }


    /// @brief Step 2: Update the model with the selected board
    model.selectFromBoard(selectedPosition.board);


    /// @brief Step 3: Update the view with the highlighted board
    addHighlightedBoard(selectedPosition.board);
    view.update_highlightedBoard(computeHighlightedBoardViews());
    view.update_FromPosition(
        std::make_pair(nullptr, Chess::Position2D(-1, -1))
    );

    /// @brief Step 4: Apply adaptive zoom if board is small (zoom < 0.8)
    auto selectedBoardView = _boardToBoardViewMap[selectedPosition.board];
    if (selectedBoardView) {
      view.focusOnBoardWithAdaptiveZoom(selectedBoardView);
    }
    return true;
}

void ChessController::handleSelectedFromPosition(Chess::SelectedPosition selectedPosition) {
   // Select the position on the selected board
    /// Step 1: Check if the selected position is valid
    if (selectedPosition.board ->getPiece(selectedPosition.position) == nullptr) return;
    // highlight Piece's position
    std::shared_ptr<BoardView> selectedBoardView = _boardToBoardViewMap[selectedPosition.board];
    view.update_FromPosition(
        std::make_pair(selectedBoardView, selectedPosition.position)
    );
    if (selectedPosition.board->getPiece(selectedPosition.position)->color() != model._game->getCurrentTurnColor()) {
      std::cout << "Invalid selection: no piece at the selected position." << std::endl;
      return; // Invalid selection
    }
    std::cout << selectedPosition.board->getPiece(selectedPosition.position)->name() << " selected." << std::endl;
    
    /// Step 2: Update the model with the selected position
    model.selectFromPosition(selectedPosition.position); 


    
    /// Step 3: Update the view with the highlighted positions
    
    std::vector<Chess::SelectedPosition> getMoveablePositions = model._game->getMoveablePositions(selectedPosition);
    resetHighlightedPositions();
    // addHighlightedPosition(selectedPosition);
    for (const auto& pos : getMoveablePositions) {
      addHighlightedPosition(pos);
    }
    std::vector<std::pair<std::shared_ptr<BoardView>, Chess::Position2D>> Converted_highlightedPositions;
    for (const auto& pos : _highlightedPositions) {
        Converted_highlightedPositions.emplace_back(_boardToBoardViewMap[pos.board], pos.position);
    }
    view.update_highlightedPositions(Converted_highlightedPositions);

    if (selectedBoardView) {
      view.focusOnBoardWithAdaptiveZoom(selectedBoardView);
    }
}

bool ChessController::handleSelectedToBoard(Chess::SelectedPosition selectedPosition) {
    // Select the target board to which to move
   /// @brief Step 1: Check if the selected board is valid
    if (selectedPosition.board == nullptr) {
      std::cout << "Invalid selection: no target board selected." << std::endl;
      return false; // Invalid selection
    }
    std::vector<Chess::SelectedPosition> getMoveablePositions = 
      model._game->getMoveablePositions(Chess::SelectedPosition(
        model._currentMoveState.selectedBoard, 
        model._currentMoveState.selectedPosition
      ));
    bool validTargetBoard = false;
    for (const auto& pos : getMoveablePositions) {
      if (pos.board == selectedPosition.board) {
        validTargetBoard = true;
        break;
      }
    }
    if (!validTargetBoard) {
      std::cout << "Invalid selection: cannot move to the selected target board." << std::endl;
      return false; // Invalid selection
    }

    /// @brief Step 2: Update the model with the target board
    model.selectToBoard(selectedPosition.board);

    /// @brief Step 3: Update the view with the highlighted board
    addHighlightedBoard(selectedPosition.board);
    view.update_highlightedBoard(computeHighlightedBoardViews());
    return true;
}

void ChessController::handleSelectedToPosition(Chess::SelectedPosition selectedPosition) {
 // Select the target position on the target board
  if (selectedPosition.board == nullptr) {
      std::cout << "Invalid selection: no target board selected." << std::endl;
      return; // Invalid selection
    }
    if (selectedPosition.position == Chess::Position2D(-1, -1)) {
      std::cout << "Invalid selection: no target position selected." << std::endl;
      return; // Invalid selection
    }
    /// @brief Step 1: Check if the selected position is valid
    std::vector<Chess::SelectedPosition> getMoveablePositions = 
      model._game->getMoveablePositions(Chess::SelectedPosition(
        model._currentMoveState.selectedBoard, 
        model._currentMoveState.selectedPosition
      ));
    bool validTargetPosition = false;
    for (const auto& pos : getMoveablePositions) {
      if (pos.board == selectedPosition.board && pos.position == selectedPosition.position) {
        validTargetPosition = true;
        break;
      }
    }
    if (!validTargetPosition) {
      std::cout << "Invalid selection: cannot move to the selected target position." << std::endl;
      return; // Invalid selection
    }

    /// @brief Step 2: Update the model with the validated target board and position
    /// (the board may differ from an earlier SELECT_TO_BOARD click, so take it from this click)
    model.selectToBoard(selectedPosition.board);
    model.selectToPosition(selectedPosition.position);
    
    /// @brief Step 3. Update the view with transition and make the move
    view.update_FromPosition({nullptr, Chess::Position2D(-1, -1)});
    /// @note the following code will be put in the onComplete callback of the transition
    /// @note for testing, now we just make the move directly
    model.makeMove(Chess::Move(
        {model._currentMoveState.selectedBoard, model._currentMoveState.selectedPosition},
        {model._currentMoveState.targetBoard, model._currentMoveState.targetPosition}
    ));
    model._currentMoveState.reset(); // Reset the move state after the move is made
    resetHighlightedBoard();
    resetHighlightedPositions();
    view.update_highlightedBoard(computeHighlightedBoardViews());
    view.update_highlightedPositions({}); // Clear highlighted positions after the move
    
    // Focus camera on the newest board with appropriate zoom
    std::shared_ptr<Chess::Board> newestBoard = model._game->getNewBoard();
    // calculate the position of the newest board view, the boardview of the newest board is not set in this frame
    // so just calculate the position based on the board's half turn number and time line ID
    std::shared_ptr<BoardView> newestBoardView = std::make_shared<BoardView2D>();
    newestBoardView->setBoardTexture(&ResourceManager::getInstance().getTexture2D("mainChessBoard"));
    newestBoardView->setRenderArea({
        static_cast<float>(newestBoard->halfTurnNumber()) * (BOARD_WORLD_SIZE + HORIZONTAL_SPACING),
        static_cast<float>(newestBoard->timeLineId()) * (BOARD_WORLD_SIZE + VERTICAL_SPACING),
        BOARD_WORLD_SIZE,
        BOARD_WORLD_SIZE
    });
    view.focusOnNewestBoard(newestBoardView);
}

void ChessController::handleSelectedPosition(Chess::SelectedPosition selectedPosition) {
  if (_isGameEnd) {
    return; // Ignore input if the game has ended
  }
  if (!selectedPosition.board) {
    return; // Clicked board view has no matching model board
  }
  /// @brief chose the board to move from
  if (model._currentMoveState.currentPhase == MovePhase::SELECT_FROM_BOARD) {
    if (handleSelectedFromBoard(selectedPosition)) {
      handleSelectedFromPosition(selectedPosition);
    }
  } 
  /// @brief chose the position to move from
  else if (model._currentMoveState.currentPhase == MovePhase::SELECT_FROM_POSITION) {
    if (selectedPosition.board == model._currentMoveState.selectedBoard) {
      handleSelectedFromPosition(selectedPosition);
    }
    else if (model._game->canMakeMoveFromBoard(selectedPosition.board)) {
      // Click on another board: start a fresh selection there
      clearSelection();
      if (handleSelectedFromBoard(selectedPosition)) {
        handleSelectedFromPosition(selectedPosition);
      }
    }
  } 
  else if (model._currentMoveState.currentPhase == MovePhase::SELECT_TO_BOARD) {
    if (handleSelectedToBoard(selectedPosition)) {
      handleSelectedToPosition(selectedPosition);
    }
  } 
  else if (model._currentMoveState.currentPhase == MovePhase::SELECT_TO_POSITION) {
    handleSelectedToPosition(selectedPosition);
  }
}

std::vector<std::shared_ptr<BoardView>> ChessController::computeBoardViewFromCurrentBoards(std::string boardType) const {
    if (boardType == "2D") {
        return computeBoardView2DsFromCurrentBoards();
    } else if (boardType == "3D") {
        return computeBoardView3DsFromCurrentBoards();
    }
    return std::vector<std::shared_ptr<BoardView>>(); // Empty vector for unsupported types
}

std::vector<std::shared_ptr<Chess::Board>> ChessController::computeCurrentBoardFromModel() const {
  std::vector<std::shared_ptr<Chess::Board>> Boards;

  auto timeLines = model.getTimeLines();
  for (const auto& timeLine : timeLines) {
    std::vector<std::shared_ptr<Chess::Board>> boards = timeLine->getBoards();
    for (const auto& board : boards) {
      if (board) {
        Boards.push_back(board);
      }
    }
  }
  return Boards;
}

std::vector<std::shared_ptr<BoardView>> ChessController::computeBoardView2DsFromCurrentBoards() const {
  std::vector<std::shared_ptr<BoardView>> boardViews;

  for (const auto& board : _currentBoard) {
    auto boardView = std::make_shared<BoardView2D>();
    boardView->setBoardTexture(&ResourceManager::getInstance().getTexture2D("mainChessBoard"));
    boardView->setRenderArea({
        static_cast<float>(board->halfTurnNumber()) * (BOARD_WORLD_SIZE + HORIZONTAL_SPACING),
        static_cast<float>(board->timeLineId()) * (BOARD_WORLD_SIZE + VERTICAL_SPACING),
        BOARD_WORLD_SIZE,
        BOARD_WORLD_SIZE
    });

    std::vector<std::pair<Chess::Position2D, std::string>> piecePositions;
    for (int x = 0; x < board->dim(); ++x) {
      for (int y = 0; y < board->dim(); ++y) {
        auto piece = board->getPiece(Chess::Position2D(x, y));
        if (piece) {
          const std::string& pieceColor = (piece->color() == Chess::PieceColor::PIECEWHITE) ? "white" : "black";
          const std::string& pieceName = pieceColor + "_" + piece->name();
          piecePositions.emplace_back(Chess::Position2D(x, y), pieceName);
        }
      }
    }
    boardView->setPiecePositions(piecePositions);
    boardView->setBoardDim(board->dim());
    boardViews.push_back(boardView);
  }
  
  return boardViews;
}

std::vector<std::shared_ptr<BoardView>> ChessController::computeBoardView3DsFromCurrentBoards() const {
  return std::vector<std::shared_ptr<BoardView>>(); // Placeholder for 3D board views
}


std::vector<std::shared_ptr<BoardView>> ChessController::computeHighlightedBoardViews() const {
    std::vector<std::shared_ptr<BoardView>> highlightedViews;
    for (auto& board : _highlightedBoard) {
        auto it = _boardToBoardViewMap.find(board);
        if (it != _boardToBoardViewMap.end()) {
            highlightedViews.push_back(it->second);
        } else {
            std::cerr << "BoardView not found for highlighted board!" << std::endl;
        }
    }
    return highlightedViews;
  }

void ChessController::render() {
  view.render();
  view.renderHud();
  renderInGameMenu();

  if (model._game->gameEnd()) {
    auto winner = model._game->getWinner();
    std::string winnerText = (winner == Chess::PieceColor::PIECEWHITE) ? "White wins!" : "Black wins!";
    view.renderEndGameScreen(winnerText);
  }
}



void ChessController::initInGameMenu() {
   _inGameMenuSystem = std::make_shared<Menu>("In-Game Menu", true);
  
  std::shared_ptr<MenuComponent> Undo = std::make_shared<MenuItem>("Undo", true);
  auto UndoCommand = std::make_unique<UndoMoveCommand>();
  UndoCommand->setCallback([this](){
    handleUndoMove(); 
  });
  Undo->setCommand(std::move(UndoCommand));

  std::shared_ptr<MenuComponent> Deselect = std::make_shared<MenuItem>("Deselect", true);
  auto DeselectCommand = std::make_unique<DeselectMoveCommand>();
  DeselectCommand->setCallback([this](){
    handleDeselectPosition();
  });
  Deselect->setCommand(std::move(DeselectCommand));

  std::shared_ptr<MenuComponent> Submit = std::make_shared<MenuItem>("Submit", true);
  auto SubmitCommand = std::make_unique<SubmitMoveCommand>();
  SubmitCommand->setCallback([this](){
    handleSubmitMove();
  });
  Submit->setCommand(std::move(SubmitCommand));

  _inGameMenuSystem->addItem(Undo);
  _inGameMenuSystem->addItem(Deselect);
  _inGameMenuSystem->addItem(Submit);

  _inGameMenuController = std::make_shared<InGameMenuController>(_inGameMenuSystem);
}


void ChessController::renderInGameMenu() const {
  if (_inGameMenuController) {
    _inGameMenuController->draw();
  } else {
    std::cerr << "InGameMenuController is not initialized!" << std::endl;
  }
}

HudData ChessController::computeHud() const {
  HudData hud;
  hud.whiteToMove = model._game->getCurrentTurnColor() == Chess::PieceColor::PIECEWHITE;
  hud.fullTurn = model._game->presentFullTurn() + 1;
  hud.timelineCount = model._game->timeLineCount();
  if (model._game->gameEnd()) {
    hud.hint = "";
  } else if (_moveableBoards.empty()) {
    hud.hint = "Submit your turn";
  } else {
    switch (model._currentMoveState.currentPhase) {
      case MovePhase::SELECT_FROM_BOARD: hud.hint = "Select a board"; break;
      case MovePhase::SELECT_FROM_POSITION: hud.hint = "Select a piece"; break;
      default: hud.hint = "Select a target"; break;
    }
  }
  return hud;
}

void ChessController::updateMenuButtonStates() {
  if (!_inGameMenuSystem) {
    return; // Menu not initialized yet
  }

  // Find menu items by title
  MenuComponent* undoItem = _inGameMenuSystem->findItem("Undo");
  MenuComponent* submitItem = _inGameMenuSystem->findItem("Submit");
  MenuComponent* deselectItem = _inGameMenuSystem->findItem("Deselect");

  // Update Undo button: enabled if there are moves to undo
  if (undoItem) {
    bool canUndo = model._game->undoable() && !model._game->gameEnd();
    undoItem->setEnabled(canUndo);
  }

  // Update Submit button: enabled if there are no moveable boards (turn can be submitted)
  if (submitItem) {
    _moveableBoards = model._game->getMoveableBoards();
    bool canSubmit = _moveableBoards.empty() && !model._game->gameEnd();
    submitItem->setEnabled(canSubmit);
  }

  // Update Deselect button: enabled if there's a current move state to deselect
  // (i.e., not in the initial SELECT_FROM_BOARD phase or has selections made)
  if (deselectItem) {
    bool canDeselect = (model._currentMoveState.currentPhase != MovePhase::SELECT_FROM_BOARD ||
                        model._currentMoveState.selectedBoard != nullptr) && !model._game->gameEnd();
    deselectItem->setEnabled(canDeselect);
  }
}

void ChessController::clearSelection() {
  model._currentMoveState.reset(); // Reset the in-progress move
  resetHighlightedBoard();
  resetHighlightedPositions();
  view.update_highlightedBoard(computeHighlightedBoardViews());
  view.update_highlightedPositions({}); // Clear highlighted positions
  view.update_FromPosition({nullptr, Chess::Position2D(-1, -1)});
}

void ChessController::handleUndoMove() {
  std::cout << "Undoing last move..." << std::endl;

  // No validity check needed - button is disabled when invalid
  model._game->undo();
  std::cout << "Last move undone successfully." << std::endl;
  clearSelection();
  
  // Update menu button states after game state change
  updateMenuButtonStates();
}

void ChessController::handleSubmitMove() {
  std::cout << "Submitting move..." << std::endl;

  // No validity check needed - button is disabled when invalid
  model._game->submitTurn();
  std::cout << "Move submitted successfully." << std::endl;
  clearSelection();
  
  // Update menu button states after game state change
  updateMenuButtonStates();
}

void ChessController::handleDeselectPosition() {
  std::cout << "Deselecting position..." << std::endl;

  // No validity check needed - button is disabled when invalid
  clearSelection();
  // Update menu button states after game state change
  updateMenuButtonStates();
}

std::vector<TimelineArrowData> ChessController::computeTimelineArrows() const {
    std::vector<TimelineArrowData> arrows;
    
    // Compute progression arrows (within timelines)
    auto progressionArrows = computeProgressionArrows();
    arrows.insert(arrows.end(), progressionArrows.begin(), progressionArrows.end());
    
    // Compute branching arrows (between timelines)
    auto branchingArrows = computeBranchingArrows();
    arrows.insert(arrows.end(), branchingArrows.begin(), branchingArrows.end());
    
    return arrows;
}

std::vector<TimelineArrowData> ChessController::computeProgressionArrows() const {
    std::vector<TimelineArrowData> arrows;
    
    auto timelines = model.getGame()->getTimeLines();
    
    for (const auto& timeline : timelines) {
        auto boards = timeline->getBoards();
        
        // Create arrows between consecutive boards in the timeline
        for (size_t i = 0; i < boards.size() - 1; ++i) {
            // Find corresponding board views
            std::shared_ptr<BoardView> fromBoardView = nullptr;
            std::shared_ptr<BoardView> toBoardView = nullptr;
            
            auto fromIt = _boardToBoardViewMap.find(boards[i]);
            auto toIt = _boardToBoardViewMap.find(boards[i + 1]);
            
            if (fromIt != _boardToBoardViewMap.end() && toIt != _boardToBoardViewMap.end()) {
                fromBoardView = fromIt->second;
                toBoardView = toIt->second;
                
                if (fromBoardView && toBoardView) {
                    // Alternate colors based on timeline ID
                    Color color = UI::Color::timelineArrow;
                    arrows.emplace_back(fromBoardView, toBoardView, "progression", color);
                }
            }
        }
    }
    
    return arrows;
}

std::vector<TimelineArrowData> ChessController::computeBranchingArrows() const {
    std::vector<TimelineArrowData> arrows;
    
    auto timelines = model.getGame()->getTimeLines();
    
    for (const auto& timeline : timelines) {
        if (!timeline->hasParent()) continue; // Skip original timelines
        
        // Find the fork point board in parent timeline
        int forkPoint = timeline->forkAt();
        std::shared_ptr<Chess::Board> parentBoard = nullptr;
        
        try {
            if (model.getGame()->boardExists(timeline->parentId(), forkPoint)) {
                parentBoard = model.getGame()->getBoard(timeline->parentId(), forkPoint);
            }
        } catch (...) {
            continue; // Skip if board doesn't exist
        }
        
        if (!parentBoard || timeline->getBoards().empty()) continue;
        
        // Find corresponding board views
        std::shared_ptr<BoardView> parentBoardView = nullptr;
        std::shared_ptr<BoardView> childBoardView = nullptr;
        
        auto parentIt = _boardToBoardViewMap.find(parentBoard);
        auto childIt = _boardToBoardViewMap.find(timeline->getBoards()[0]);
        
        if (parentIt != _boardToBoardViewMap.end() && childIt != _boardToBoardViewMap.end()) {
            parentBoardView = parentIt->second;
            childBoardView = childIt->second;
            
            if (parentBoardView && childBoardView) {
                arrows.emplace_back(parentBoardView, childBoardView, "branch", UI::Color::branchArrow);
            }
        }
    }
    
    return arrows;
}

PresentLineData ChessController::computePresentLine() const {
    // Get the buffer half turn from the game model
    int bufferHalfTurn = model.getGame()->bufferHalfTurn();
    
    // Create present line data with appropriate styling
    PresentLineData lineData;
    lineData.halfTurnPosition = static_cast<float>(bufferHalfTurn);
    lineData.color = UI::Color::presentLine;
    lineData.thickness = 3.0f;            // screen pixels (renderer divides by zoom)
    lineData.isVisible = true;            // Always visible
    
    return lineData;
}

