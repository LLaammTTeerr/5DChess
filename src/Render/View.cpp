#include "View.h"
#include "chess.h"
#include "raymath.h"
#include "PieceTheme.h"
#include "PresentLineRenderer.h"
#include <iostream>
#include "BoardView.h"
#include <algorithm>
#include <cmath>
#include <cfloat>
#include "ResourceManager.h"

ChessView::ChessView(Vector3 worldSize)
    : _worldSize(worldSize) {
    // Initialize camera controller
    _cameraController = std::make_unique<CameraController>(worldSize);
    // Initialize arrow renderer
    _arrowRenderer = std::make_unique<TimelineArrowRenderer>();
    // Initialize present line renderer
    _presentLineRenderer = std::make_unique<PresentLineRenderer>();
}



void ChessView::handleMouseOver() {
    std::shared_ptr<BoardView> hoveredBoardView = nullptr;
    Chess::Position2D hoveredPosition(-1, -1);

    for (auto& boardView : _boardViews) {
        if (boardView && boardView->isMouseOverBoard()) {
            hoveredBoardView = boardView;
            hoveredPosition = boardView->getMouseOverPosition();
            break;  // Stop at the first hovered board
        }
    }

    if (hoveredBoardView && hoveredPosition.x() != -1 && hoveredPosition.y() != -1) {
        if (_onMouseOverPositionCallback) {
            _onMouseOverPositionCallback({hoveredBoardView, hoveredPosition});
        }
    }
}

void ChessView::handleMouseSelection() {
    std::shared_ptr<BoardView> selectedBoardView = nullptr;
    Chess::Position2D selectedPosition(-1, -1);

    for (auto& boardView : _boardViews) if (boardView) {
        if (boardView -> isMouseClickedOnBoard()) {
            selectedBoardView = boardView;  // Store original reference, not clone
            if (boardView -> getMouseClickedPosition() != Chess::Position2D{-1, -1}) {
                selectedPosition = boardView -> getMouseClickedPosition();
            }
        }
    }

    if (selectedBoardView && selectedPosition.x() != -1 && selectedPosition.y() != -1) {
        if (_onSelectedPositionCallback) {
            _onSelectedPositionCallback({selectedBoardView, selectedPosition});
        }
    }
}

void ChessView::handleInput() {
    /// @brief Handle input for camera movement and zoom
    update(GetFrameTime());

    /// @brief Handle mouse clicks: selected board and selected position
    handleMouseSelection();
    handleMouseOver();
    /// @brief Handle user camera input (delegate to CameraController)
    _cameraController->handleUserInput();
}

void ChessView::update(float deltaTime) {
    _cameraController->update(deltaTime, _boardViews);
    
    // Update arrow animations using the renderer
    _arrowRenderer->update(deltaTime);
    
    // Update present line animations
    _presentLineRenderer->update(deltaTime);
}


void ChessView::render_boardViews() const {
    BeginMode2D(*_cameraController->getCamera2D());

    for (const auto& boardView : _boardViews) {
        if (boardView) {
            boardView->render();
        } else {
            std::cerr << "Null BoardView encountered!" << std::endl;
        }
    }

    EndMode2D();
}

void ChessView::render() const {
    // Render present line first (behind everything else)
    renderPresentLine();
    
    // Render timeline arrows second (behind boards)
    renderTimelineArrows();
    
    render_boardViews();
    render_highlightBoard();
    render_highlightedPositions();
    render_highlightPiece(_fromPosition);

    // Draw UI
    Vector2 mousePos = GetMousePosition();
    Vector2 worldPos = _cameraController->isUsing3DRendering() ? Vector2{0, 0} : GetScreenToWorld2D(mousePos, *_cameraController->getCamera2D());
    
    // Delegate debug info rendering to CameraController
    // _cameraController->renderDebugInfo();
}


void ChessView::addBoardView(std::shared_ptr<BoardView> boardView) {
    if (boardView) {
        boardView -> setSupervisor(this);
        _boardViews.push_back(boardView);
        // Set the appropriate camera based on board view type
        if (boardView->is3D()) {
            _cameraController->setUsing3DRendering(true);
            boardView -> setCamera3D(_cameraController->getCamera3D());
        } else {
            boardView -> setCamera2D(_cameraController->getCamera2D());
        }
    } else {
        std::cerr << "Attempted to add a null BoardView!" << std::endl;
    }
}


void ChessView::removeBoardView(std::shared_ptr<BoardView> boardView) {
    if (boardView) {
        auto it = std::remove(_boardViews.begin(), _boardViews.end(), boardView);
        if (it != _boardViews.end()) {
            _boardViews.erase(it, _boardViews.end());
            // Re-evaluate rendering mode
            bool use3D = false;
            for (const auto& view : _boardViews) {
                if (view->is3D()) {
                    use3D = true;
                    break;
                }
            }
            _cameraController->setUsing3DRendering(use3D);
        } else {
            std::cerr << "Attempted to remove a non-existent BoardView!" << std::endl;
        }
    } else {
        std::cerr << "Attempted to remove a null BoardView!" << std::endl;
    }
}


std::vector<std::shared_ptr<BoardView>> ChessView::getBoardViews() const {
    return _boardViews;
}

// void ChessView::queueUpdateInvalidBoardSelection() {
//     updateQueue.push_back([this](){
//         if (_selectedBoardView) {
//             _isSelectedBoardViewInvalid = true;
//         }
//         else {
//             _isSelectedBoardViewInvalid = false;
//         }
//     });
// }

// void ChessView::queueUpdateMoveState(const RenderMoveState& rmoveState) {
//     updateQueue.push_back([this, rmoveState](){
//         // if
//     });
// }

void ChessView::clearBoardViews() {
    _boardViews.clear();
}

void ChessView::update_FromPosition(std::pair<std::shared_ptr<BoardView>, Chess::Position2D> fromPosition) {
    _fromPosition = fromPosition;
}

void ChessView::update_highlightedBoard(const std::vector<std::shared_ptr<BoardView>>& boardViews) {
    _highlightedBoards = boardViews;
}

void ChessView::render_highlightBoard() const {
    if (_cameraController->isUsing3DRendering()) {
        // 3D rendering code
        return;
    }

    BeginMode2D(*_cameraController->getCamera2D());
    for (const auto& boardView : _highlightedBoards) {
        if (boardView) {
            boardView->render_highlightBoundaries();
        } else {
            std::cerr << "Null BoardView encountered in highlighted boards!" << std::endl;
        }
    }
    EndMode2D();
}

void ChessView::render_highlightPiece(std::pair<std::shared_ptr<BoardView>, Chess::Position2D> piecePosition) const {
    if (piecePosition.first == nullptr || piecePosition.second.x() < 0 || piecePosition.second.y() < 0) {
        return;
    }
    if (_cameraController->isUsing3DRendering()) {
        // 3D rendering code for highlighted piece
        return;
    }

    BeginMode2D(*_cameraController->getCamera2D());
    if (piecePosition.first) {
        piecePosition.first->render_highlightPiece(piecePosition.second);
    } else {
        std::cerr << "Null BoardView encountered in highlighted piece!" << std::endl;
    }
    EndMode2D();
}

void ChessView::update_highlightedPositions(const std::vector<std::pair<std::shared_ptr<BoardView>, Chess::Position2D>>& positions) {
    if (!positions.empty()) {
        std::cout << "Updating highlighted positions with " << positions.size() << " entries." << std::endl;
    }
    _highlightedPositions = positions;
}


void ChessView::render_highlightedPositions() const {
    if (_cameraController->isUsing3DRendering()) {
        // 3D rendering code for highlighted positions
        return;
    }
    BeginMode2D(*_cameraController->getCamera2D());
    for (const auto& position : _highlightedPositions) {
        if (position.first) {
            position.first->render_highlightedPositions({position.second});
        } else {
            std::cerr << "Null BoardView encountered in highlighted positions!" << std::endl;
        }
    }
    EndMode2D();
}

void ChessView::startMoveTransition(
    std::shared_ptr<BoardView> fromBoardView,
    Chess::Position2D fromPosition,
    std::shared_ptr<BoardView> toBoardView,
    Chess::Position2D toPosition,
    float duration,
    std::function<void()> onComplete
) {
    if (fromBoardView && toBoardView) {
        TransitionComponent transition;
        transition.fromBoardView = fromBoardView;
        transition.fromPosition = fromPosition;
        transition.toBoardView = toBoardView;
        transition.toPosition = toPosition;
        transition.duration = duration;
        transition.onComplete = onComplete;

        _transitions.push_back(transition);
    } else {
        std::cerr << "Invalid board views for move transition!" << std::endl;
    }
}

void ChessView::updateTimelineArrows(const std::vector<TimelineArrowData>& arrowData) {
    _arrowRenderer->updateArrows(arrowData);
}

void ChessView::renderTimelineArrows() const {
    _arrowRenderer->render(_cameraController->getCamera2D(), _cameraController->isUsing3DRendering());
}

void ChessView::updatePresentLine(const PresentLineData& lineData) {
    _presentLineRenderer->updatePresentLine(lineData);
}

void ChessView::renderPresentLine() const {
    _presentLineRenderer->render(_cameraController->getCamera2D(), _cameraController->isUsing3DRendering(), _boardViews);
}


void ChessView::renderEndGameScreen(std::string winnerText) const {
    DrawText(winnerText.c_str(), GetScreenWidth() / 2 - MeasureText(winnerText.c_str(), 20) / 2, GetScreenHeight() / 2 - 10, 20, WHITE);
}