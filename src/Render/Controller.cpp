#include "Render/Controller.h"
#include "ResourceManager.h"

ChessController::ChessController(ChessModel& m, ChessView& v) : model(m), view(v) {
    setupViewCallbacks();
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
  }
  updateNewBoardViewsToView();
}

void ChessController::handleInput() {
    update(GetFrameTime());
    view.handleInput();
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
}


void ChessController::handleSelectedPosition(Chess::SelectedPosition selectedPosition) {
  MoveState currentMoveState = model._currentMoveState;
  if (currentMoveState.currentPhase == MovePhase::SELECT_FROM_BOARD) {
    // Select the board from which to move
    // Check if the selected position is valid: can move from the selected position
    if (!model._game->canMakeMoveFromBoard(selectedPosition.board)) {
      std::cout << "Invalid selection: cannot make move from the selected board." << std::endl;
      return; // Invalid selection
    }

    model.selectFromBoard(selectedPosition.board);
    addHighlightedBoard(selectedPosition.board);
    view.update_highlightedBoard(computeHighlightedBoardViews());

  } else if (currentMoveState.currentPhase == MovePhase::SELECT_FROM_POSITION) {
    // Select the position on the selected board
    std::cout << "Selected position: " << selectedPosition.position.x() << ", " << selectedPosition.position.y() << std::endl;
    if (selectedPosition.board ->getPiece(selectedPosition.position) == nullptr) {
      std::cout << "Invalid selection: no piece at the selected position." << std::endl;
      return; // Invalid selection
    }
    std::cout << selectedPosition.board->getPiece(selectedPosition.position)->name() << " selected." << std::endl;
    model.selectFromPosition(selectedPosition.position); // update model

    std::vector<Chess::SelectedPosition> getMoveablePositions = model._game->getMoveablePositions(selectedPosition);
    resetHighlightedPositions();
    addHighlightedPosition(selectedPosition);
    for (const auto& pos : getMoveablePositions) {
      addHighlightedPosition(pos);
    }
    std::vector<std::pair<std::shared_ptr<BoardView>, Chess::Position2D>> Converted_highlightedPositions;
    for (const auto& pos : _highlightedPositions) {
        Converted_highlightedPositions.emplace_back(_boardToBoardViewMap[pos.board], pos.position);
    }
    view.update_highlightedPositions(Converted_highlightedPositions);

  } else if (currentMoveState.currentPhase == MovePhase::SELECT_TO_BOARD) {
    // Select the target board for the move
    model.selectToBoard(selectedPosition.board);
  } else if (currentMoveState.currentPhase == MovePhase::SELECT_TO_POSITION) {
    // Select the target position on the target board
    model.selectToPosition(selectedPosition.position);
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
  view.render(_currentBoard);
}