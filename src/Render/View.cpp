#include "View.h"
#include "chess.h"
#include "raymath.h"
#include "PieceTheme.h"
#include <iostream>
#include "BoardView.h"
#include <algorithm>
#include <cmath>
#include <cfloat>

ChessView::ChessView(Vector3 worldSize)
    : _worldSize(worldSize) {
    // Initialize camera controller
    _cameraController = std::make_unique<CameraController>(worldSize);
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

    /// @brief Handle user camera input (delegate to CameraController)
    _cameraController->handleUserInput();
}

void ChessView::update(float deltaTime) {
    _cameraController->update(deltaTime, _boardViews);
    
    // Update arrow animations
    _arrowAnimationState.animationTime += deltaTime;
    _arrowAnimationState.dashOffset += deltaTime * 50.0f; // Adjust speed as needed
    _arrowAnimationState.pulsePhase += deltaTime * 2.0f;
    
    // Update timeline arrows if game reference is available
    if (_gameRef) {
        updateTimelineArrows();
    }
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
    // Render timeline arrows first (behind boards)
    renderTimelineArrows();
    
    render_boardViews();
    render_highlightBoard();
    render_highlightedPositions();

    // Draw UI
    Vector2 mousePos = GetMousePosition();
    Vector2 worldPos = _cameraController->isUsing3DRendering() ? Vector2{0, 0} : GetScreenToWorld2D(mousePos, *_cameraController->getCamera2D());
    
    DrawText("Game World", 10, 10, 20, BLACK);
    
    // Delegate debug info rendering to CameraController
    _cameraController->renderDebugInfo();
    
    DrawText(TextFormat("Mouse Screen Pos: (%.2f, %.2f)", mousePos.x, mousePos.y), 10, 110, 20, BLACK);
    DrawText(TextFormat("Mouse World Pos: (%.2f, %.2f)", worldPos.x, worldPos.y), 10, 135, 20, BLACK);
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

void ChessView::updateTimelineArrows() {
    if (!_gameRef) return;
    
    _timelineArrows.clear();
    
    // Generate progression arrows (within timelines)
    generateProgressionArrows();
    
    // Generate branching arrows (between timelines)
    generateBranchingArrows();
}

void ChessView::generateProgressionArrows() {
    if (!_gameRef) return;
    
    auto timelines = _gameRef->getTimeLines();
    
    for (const auto& timeline : timelines) {
        auto boards = timeline->getBoards();
        
        // Create arrows between consecutive boards in the timeline
        for (size_t i = 0; i < boards.size() - 1; ++i) {
            // Find corresponding board views
            std::shared_ptr<BoardView> fromBoardView = nullptr;
            std::shared_ptr<BoardView> toBoardView = nullptr;
            
            for (const auto& boardView : _boardViews) {
                if (boardView->getBoard() == boards[i]) {
                    fromBoardView = boardView;
                }
                if (boardView->getBoard() == boards[i + 1]) {
                    toBoardView = boardView;
                }
            }
            
            if (fromBoardView && toBoardView) {
                TimelineArrow arrow;
                arrow.startPos = calculateArrowPosition(fromBoardView, false); // end of from board
                arrow.endPos = calculateArrowPosition(toBoardView, true); // start of to board
                arrow.color = (timeline->ID() % 2 == 0) ? BLUE : GREEN; // Alternate colors
                arrow.thickness = 4.0f;
                arrow.type = "progression";
                arrow.isAnimated = true;
                
                _timelineArrows.push_back(arrow);
            }
        }
    }
}

void ChessView::generateBranchingArrows() {
    if (!_gameRef) return;
    
    auto timelines = _gameRef->getTimeLines();
    
    for (const auto& timeline : timelines) {
        auto parentTimeline = timeline->parent();
        if (!parentTimeline) continue; // Skip main timeline
        
        // Find the fork point board in parent timeline
        int forkPoint = timeline->forkAt();
        std::shared_ptr<Chess::Board> parentBoard = nullptr;
        
        try {
            if (_gameRef->boardExists(parentTimeline->ID(), forkPoint)) {
                parentBoard = _gameRef->getBoard(parentTimeline->ID(), forkPoint);
            }
        } catch (...) {
            continue; // Skip if board doesn't exist
        }
        
        if (!parentBoard || timeline->getBoards().empty()) continue;
        
        // Find corresponding board views
        std::shared_ptr<BoardView> parentBoardView = nullptr;
        std::shared_ptr<BoardView> childBoardView = nullptr;
        
        for (const auto& boardView : _boardViews) {
            if (boardView->getBoard() == parentBoard) {
                parentBoardView = boardView;
            }
            if (boardView->getBoard() == timeline->getBoards()[0]) {
                childBoardView = boardView;
            }
        }
        
        if (parentBoardView && childBoardView) {
            TimelineArrow arrow;
            arrow.startPos = calculateArrowPosition(parentBoardView, false);
            arrow.endPos = calculateArrowPosition(childBoardView, true);
            arrow.color = RED; // Use red for branching arrows
            arrow.thickness = 5.0f;
            arrow.type = "branch";
            arrow.isAnimated = true;
            
            _timelineArrows.push_back(arrow);
        }
    }
}

Vector2 ChessView::calculateArrowPosition(std::shared_ptr<BoardView> boardView, bool isStart) const {
    if (!boardView) return Vector2{0, 0};
    
    // Get board view center position
    Vector2 boardCenter = boardView->getBoardCenter();
    float boardSize = boardView->getBoardSize();
    
    if (isStart) {
        // Arrow starts from left side of board
        return Vector2{boardCenter.x - boardSize * 0.6f, boardCenter.y};
    } else {
        // Arrow ends at right side of board
        return Vector2{boardCenter.x + boardSize * 0.6f, boardCenter.y};
    }
}

void ChessView::renderTimelineArrows() const {
    if (_cameraController->isUsing3DRendering()) {
        // Skip arrow rendering in 3D mode for now
        return;
    }
    
    BeginMode2D(*_cameraController->getCamera2D());
    
    for (const auto& arrow : _timelineArrows) {
        if (arrow.type == "progression") {
            // Draw animated dashed line for progression
            drawAnimatedDashedLine(arrow.startPos, arrow.endPos, arrow.color, arrow.thickness, _arrowAnimationState.dashOffset);
        } else if (arrow.type == "branch") {
            // Draw curved arrow for branching
            drawCurvedArrow(arrow.startPos, arrow.endPos, arrow.color, arrow.thickness, _arrowAnimationState.animationTime);
        }
    }
    
    EndMode2D();
}

void ChessView::drawCurvedArrow(Vector2 start, Vector2 end, Color color, float thickness, float animationOffset) const {
    // Calculate control point for curved arrow
    Vector2 midPoint = {(start.x + end.x) * 0.5f, (start.y + end.y) * 0.5f};
    Vector2 direction = {end.x - start.x, end.y - start.y};
    Vector2 perpendicular = {-direction.y, direction.x};
    
    // Normalize perpendicular vector
    float length = sqrtf(perpendicular.x * perpendicular.x + perpendicular.y * perpendicular.y);
    if (length > 0) {
        perpendicular.x /= length;
        perpendicular.y /= length;
    }
    
    // Create curved control point
    float curvature = 50.0f; // Adjust curve intensity
    Vector2 controlPoint = {
        midPoint.x + perpendicular.x * curvature,
        midPoint.y + perpendicular.y * curvature
    };
    
    // Draw curved line using multiple segments
    int segments = 20;
    for (int i = 0; i < segments; ++i) {
        float t1 = (float)i / segments;
        float t2 = (float)(i + 1) / segments;
        
        // Quadratic Bezier curve calculation
        Vector2 p1 = {
            (1 - t1) * (1 - t1) * start.x + 2 * (1 - t1) * t1 * controlPoint.x + t1 * t1 * end.x,
            (1 - t1) * (1 - t1) * start.y + 2 * (1 - t1) * t1 * controlPoint.y + t1 * t1 * end.y
        };
        Vector2 p2 = {
            (1 - t2) * (1 - t2) * start.x + 2 * (1 - t2) * t2 * controlPoint.x + t2 * t2 * end.x,
            (1 - t2) * (1 - t2) * start.y + 2 * (1 - t2) * t2 * controlPoint.y + t2 * t2 * end.y
        };
        
        // Add pulsing effect
        float pulse = 1.0f + 0.3f * sinf(_arrowAnimationState.pulsePhase + t1 * 3.14159f);
        Color animatedColor = {
            (unsigned char)(color.r * pulse),
            (unsigned char)(color.g * pulse),
            (unsigned char)(color.b * pulse),
            color.a
        };
        
        DrawLineEx(p1, p2, thickness, animatedColor);
    }
    
    // Draw arrowhead at the end
    Vector2 arrowDir = {end.x - controlPoint.x, end.y - controlPoint.y};
    float arrowLength = sqrtf(arrowDir.x * arrowDir.x + arrowDir.y * arrowDir.y);
    if (arrowLength > 0) {
        arrowDir.x /= arrowLength;
        arrowDir.y /= arrowLength;
        
        Vector2 arrowSide = {-arrowDir.y, arrowDir.x};
        float arrowSize = thickness * 2;
        
        Vector2 arrowPoint1 = {
            end.x - arrowDir.x * arrowSize + arrowSide.x * arrowSize * 0.5f,
            end.y - arrowDir.y * arrowSize + arrowSide.y * arrowSize * 0.5f
        };
        Vector2 arrowPoint2 = {
            end.x - arrowDir.x * arrowSize - arrowSide.x * arrowSize * 0.5f,
            end.y - arrowDir.y * arrowSize - arrowSide.y * arrowSize * 0.5f
        };
        
        DrawTriangle(end, arrowPoint1, arrowPoint2, color);
    }
}

void ChessView::drawAnimatedDashedLine(Vector2 start, Vector2 end, Color color, float thickness, float dashOffset) const {
    Vector2 direction = {end.x - start.x, end.y - start.y};
    float totalLength = sqrtf(direction.x * direction.x + direction.y * direction.y);
    
    if (totalLength == 0) return;
    
    direction.x /= totalLength;
    direction.y /= totalLength;
    
    float dashLength = 10.0f;
    float gapLength = 5.0f;
    float segmentLength = dashLength + gapLength;
    
    // Apply animation offset
    float animatedOffset = fmodf(dashOffset, segmentLength);
    
    for (float distance = -animatedOffset; distance < totalLength; distance += segmentLength) {
        float segmentStart = fmaxf(0, distance);
        float segmentEnd = fminf(totalLength, distance + dashLength);
        
        if (segmentStart < segmentEnd) {
            Vector2 lineStart = {
                start.x + direction.x * segmentStart,
                start.y + direction.y * segmentStart
            };
            Vector2 lineEnd = {
                start.x + direction.x * segmentEnd,
                start.y + direction.y * segmentEnd
            };
            
            DrawLineEx(lineStart, lineEnd, thickness, color);
        }
    }
    
    // Draw arrowhead at the end
    Vector2 arrowPoint1 = {
        end.x - direction.x * thickness * 2 + direction.y * thickness,
        end.y - direction.y * thickness * 2 - direction.x * thickness
    };
    Vector2 arrowPoint2 = {
        end.x - direction.x * thickness * 2 - direction.y * thickness,
        end.y - direction.y * thickness * 2 + direction.x * thickness
    };
    
    DrawTriangle(end, arrowPoint1, arrowPoint2, color);
}
