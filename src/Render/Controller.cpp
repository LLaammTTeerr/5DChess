#include "Render/Controller.h"
#include "ResourceManager.h"
#include "MenuController.h"
#include "MenuComponent.h"
#include "MenuCommand.h"
#include "MenuView.h"
#include "MenuItemView.h"




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
  
  // Update menu button states based on current game state
  updateMenuButtonStates();
}

void ChessController::handleInput() {
    update(GetFrameTime());
    view.handleInput();
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
  if (model._currentMoveState.currentPhase == MovePhase::SELECT_FROM_BOARD || 
      model._currentMoveState.currentPhase == MovePhase::SELECT_FROM_POSITION) {
    // If we are in the phase of selecting a board or position, we can highlight the mouse over position
    resetHighlightedPositions();
    addHighlightedPosition(selectedPosition);
    std::vector<std::pair<std::shared_ptr<BoardView>, Chess::Position2D>> Converted_highlightedPositions;
    for (const auto& pos : _highlightedPositions) {
        Converted_highlightedPositions.emplace_back(_boardToBoardViewMap[pos.board], pos.position);
    }
    view.update_highlightedPositions(Converted_highlightedPositions);
  }
}

void ChessController::handleSelectedPosition(Chess::SelectedPosition selectedPosition) {
  /// @brief chose the board to move from
  if (model._currentMoveState.currentPhase == MovePhase::SELECT_FROM_BOARD) {
    // Select the board from which to move
    /// @brief Step 1: Check if the selected board is valid
    if (!model._game->canMakeMoveFromBoard(selectedPosition.board)) {
      std::cout << "Invalid selection: cannot make move from the selected board." << std::endl;
      return; // Invalid selection
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

  } 
  /// @brief chose the position to move from
  else if (model._currentMoveState.currentPhase == MovePhase::SELECT_FROM_POSITION) {
    // Select the position on the selected board
    /// Step 1: Check if the selected position is valid
    if (selectedPosition.board ->getPiece(selectedPosition.position) == nullptr ||
  selectedPosition.board->getPiece(selectedPosition.position)->color() != model._game->getCurrentTurnColor()) {
      std::cout << "Invalid selection: no piece at the selected position." << std::endl;
      return; // Invalid selection
    }
    std::cout << selectedPosition.board->getPiece(selectedPosition.position)->name() << " selected." << std::endl;
    
    /// Step 2: Update the model with the selected position
    model.selectFromPosition(selectedPosition.position); 


    
    /// Step 3: Update the view with the highlighted positions
    // highlight Piece's position
    std::shared_ptr<BoardView> selectedBoardView = _boardToBoardViewMap[selectedPosition.board];
    view.update_FromPosition(
        std::make_pair(selectedBoardView, selectedPosition.position)
    );
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

  } 
  else if (model._currentMoveState.currentPhase == MovePhase::SELECT_TO_BOARD) {
    /// @brief Step 1: Check if the selected board is valid
    if (selectedPosition.board == nullptr) {
      std::cout << "Invalid selection: no target board selected." << std::endl;
      return; // Invalid selection
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
      return; // Invalid selection
    }

    /// @brief Step 2: Update the model with the target board
    model.selectToBoard(selectedPosition.board);

    /// @brief Step 3: Update the view with the highlighted board
    addHighlightedBoard(selectedPosition.board);
    view.update_highlightedBoard(computeHighlightedBoardViews());


  } else if (model._currentMoveState.currentPhase == MovePhase::SELECT_TO_POSITION) {
    // Select the target position on the target board
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

    /// @brief Step 2: Update the model with the target position
    model.selectToPosition(selectedPosition.position);
    
    /// @brief Step 3. Update the view with transition and make the move
    view.update_FromPosition({nullptr, Chess::Position2D(-1, -1)});
    /// @brief update transition, we will complete it later
    /// Case: move to the same board
    // if (model._currentMoveState.selectedBoard == model._currentMoveState.targetBoard) {
    //   view.startAddBoardViewTransition(_boardToBoardViewMap[model._currentMoveState.selectedBoard]);
    // }
    // else {
    //   view.startMoveTransition(
    //       _boardToBoardViewMap[model._currentMoveState.selectedBoard],
    //       model._currentMoveState.selectedPosition,
    //       _boardToBoardViewMap[model._currentMoveState.targetBoard],
    //       model._currentMoveState.targetPosition,
    //       0.5f, // Duration of the transition
    //       [this]() {
    //         // Callback after the transition is complete
    //         model.makeMove(Chess::Move(
    //             model._currentMoveState.selectedBoard,
    //             model._currentMoveState.selectedPosition,
    //             model._currentMoveState.targetBoard,
    //             model._currentMoveState.targetPosition
    //         ));
    //         // model._currentMoveState.reset(); // Reset the move state after the move is made
    //       }
    //   );
    //   // after move the piece between boards, we create a new board
    //   // view.startAddBoardViewTransition
    // }

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
    view.focusOnNewestBoard();
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

// RenderMoveState ChessController::convertModelToRenderState(const MoveState& moveState) {
//     RenderMoveState renderState;
//     renderState.selectedBoardView = getBoardViewFromModel(moveState.selectedBoard);
//     renderState.selectedPosition = moveState.selectedPosition;
//     renderState.targetBoardView = getBoardViewFromModel(moveState.targetBoard);
//     renderState.targetPosition = moveState.targetPosition;
//     renderState.currentPhase = moveState.currentPhase;

//     return renderState;
// }

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
        static_cast<float>(board->getTimeLine()->ID()) * (BOARD_WORLD_SIZE + VERTICAL_SPACING),
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
  renderInGameMenu();
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

  _inGameMenuController = std::make_shared<InGameMenuController>(&model, &view, _inGameMenuSystem);
}


void ChessController::renderInGameMenu() const {
  if (_inGameMenuController) {
    _inGameMenuController->draw();
  } else {
    std::cerr << "InGameMenuController is not initialized!" << std::endl;
  }
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
    bool canUndo = model._game->undoable();
    undoItem->setEnabled(canUndo);
  }

  // Update Submit button: enabled if there are no moveable boards (turn can be submitted)
  if (submitItem) {
    std::vector<std::shared_ptr<Chess::Board>> moveableBoards = model._game->getMoveableBoards();
    bool canSubmit = moveableBoards.empty();
    submitItem->setEnabled(canSubmit);
  }

  // Update Deselect button: enabled if there's a current move state to deselect
  // (i.e., not in the initial SELECT_FROM_BOARD phase or has selections made)
  if (deselectItem) {
    bool canDeselect = model._currentMoveState.currentPhase != MovePhase::SELECT_FROM_BOARD ||
                       model._currentMoveState.selectedBoard != nullptr;
    deselectItem->setEnabled(canDeselect);
  }
}

void ChessController::handleUndoMove() {
  std::cout << "Undoing last move..." << std::endl;

  // No validity check needed - button is disabled when invalid
  model._game->undo();
  std::cout << "Last move undone successfully." << std::endl;
  resetHighlightedBoard();
  resetHighlightedPositions();
  
  // Focus camera on the newest board after undo
  view.focusOnNewestBoard();
  
  // Update menu button states after game state change
  updateMenuButtonStates();
}

void ChessController::handleSubmitMove() {
  std::cout << "Submitting move..." << std::endl;

  // No validity check needed - button is disabled when invalid
  model._game->submitTurn();
  std::cout << "Move submitted successfully." << std::endl;
  resetHighlightedBoard();
  resetHighlightedPositions();
  
  // Update menu button states after game state change
  updateMenuButtonStates();
}

void ChessController::handleDeselectPosition() {
  std::cout << "Deselecting position..." << std::endl;

  // No validity check needed - button is disabled when invalid
  model._currentMoveState.reset(); // Reset the move state
  resetHighlightedBoard();
  resetHighlightedPositions();
  view.update_highlightedBoard(computeHighlightedBoardViews());
  view.update_highlightedPositions({}); // Clear highlighted positions
  view.update_FromPosition({nullptr, Chess::Position2D(-1, -1)});
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
                    Color color = (timeline->ID() % 2 == 0) ? BLUE : GREEN;
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
        auto parentTimeline = timeline->parent();
        if (!parentTimeline) continue; // Skip main timeline
        
        // Find the fork point board in parent timeline
        int forkPoint = timeline->forkAt();
        std::shared_ptr<Chess::Board> parentBoard = nullptr;
        
        try {
            if (model.getGame()->boardExists(parentTimeline->ID(), forkPoint)) {
                parentBoard = model.getGame()->getBoard(parentTimeline->ID(), forkPoint);
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
                arrows.emplace_back(parentBoardView, childBoardView, "branch", RED);
            }
        }
    }
    
    return arrows;
}

