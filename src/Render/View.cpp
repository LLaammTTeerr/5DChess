#include "View.h"
#include "chess.h"
#include "raymath.h"
#include "PieceTheme.h"
#include <iostream>
#include "BoardView.h"
#include <algorithm>
#include <cmath>
#include <cfloat>

#ifndef PI
#define PI 3.14159265358979323846f
#endif

// CameraController implementation
CameraController::CameraController(Vector3 worldSize) : _worldSize(worldSize) {
    // Initialize 2D camera
    _camera2D.target = { worldSize.x / 2, worldSize.y / 2 };
    _camera2D.offset = { GetScreenWidth() / 2.0f, GetScreenHeight() / 2.0f };
    _camera2D.rotation = 0.0f;
    _camera2D.zoom = 1.0f;

    // Initialize 3D camera
    _camera3D.position = { worldSize.x / 2, 10.0f, worldSize.z / 2 };
    _camera3D.target = { worldSize.x / 2, 0.0f, worldSize.z / 2 };
    _camera3D.up = { 0.0f, 1.0f, 0.0f };
    _camera3D.fovy = 45.0f;
    _camera3D.projection = CAMERA_PERSPECTIVE;
    
    // Initialize camera state tracking
    _targetCameraPosition = { worldSize.x / 2.0f, worldSize.y / 2.0f };
    _autoCenterPosition = { worldSize.x / 2.0f, worldSize.y / 2.0f };
}

void CameraController::handleUserInput() {
    // Handle keyboard input for camera controls
    if (IsKeyPressed(KEY_Z)) {
        toggleAutoZoom();
        std::cout << "Auto-zoom toggled: " << (_autoZoomEnabled ? "ON" : "OFF") << std::endl;
    }
    
    // Add manual auto-zoom trigger for testing
    if (IsKeyPressed(KEY_X)) {
        _cameraState = CameraState::AUTO_ZOOMING;
        _timeSinceUserInput = 0.0f;
        std::cout << "Manual auto-zoom triggered!" << std::endl;
    }
    
    // Handle mouse dragging for camera panning
    if (IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
        Vector2 mouseDelta = GetMouseDelta();
        if (Vector2Length(mouseDelta) > 0.1f) {
            moveCamera(Vector2Scale(mouseDelta, -1.0f / _camera2D.zoom));
            // User input detected, switch to user control
            _cameraState = CameraState::USER_CONTROLLED;
            _timeSinceUserInput = 0.0f;
        }
    }
    
    // Handle mouse wheel zoom (this is also called from the update method)
    float wheel = GetMouseWheelMove();
    if (wheel != 0) {
        std::cout << "Mouse wheel moved: " << wheel << ", current zoom: " << _camera2D.zoom << std::endl;
        setZoom(_use3DRendering ? _camera3D.fovy + wheel * 5.0f : _camera2D.zoom + wheel * 0.1f);
        
        // User input detected, switch to user control temporarily
        _cameraState = CameraState::USER_CONTROLLED;
        _timeSinceUserInput = 0.0f;
    }
}

void CameraController::setZoom(float zoom) {
    if (_use3DRendering) {
        _camera3D.fovy = std::max(10.0f, std::min(zoom, 120.0f));
    } else {
        _camera2D.zoom = std::max(0.1f, std::min(zoom, 5.0f));
    }
    
    // Check if zoom is too extreme and trigger auto-zoom if enabled
    if (_autoZoomEnabled) {
        float currentZoom = _use3DRendering ? _camera3D.fovy : _camera2D.zoom;
        std::cout << "Current zoom: " << currentZoom << ", auto-zoom range: [" << _minAutoZoom << ", " << _maxAutoZoom << "]" << std::endl;
        
        // More aggressive auto-zoom triggering conditions
        if (currentZoom < _minAutoZoom * 1.5f || currentZoom > _maxAutoZoom * 0.8f) {
            _cameraState = CameraState::AUTO_ZOOMING;
            _timeSinceUserInput = 0.0f;
            std::cout << "Auto-zoom triggered! Current zoom: " << currentZoom << std::endl;
        }
    }
}

void CameraController::setCameraTarget(Vector2 target) {
    _camera2D.target = target;
    if (_use3DRendering) {
        _camera3D.target = { target.x, 0.0f, target.y };
    }
}

void CameraController::moveCamera(Vector2 delta) {
    _camera2D.target = Vector2Add(_camera2D.target, delta);
    if (_use3DRendering) {
        _camera3D.target.x += delta.x;
        _camera3D.target.z += delta.y;
        _camera3D.position.x += delta.x;
        _camera3D.position.z += delta.y;
    }
    clampToBounds();
}

void CameraController::update(float deltaTime, const std::vector<std::shared_ptr<BoardView>>& boardViews) {
    // Update camera state and handle transitions
    updateCameraState(deltaTime);
    
    // Calculate the center position of all board views
    calculateAutoCenterPosition(boardViews);
    
    // Calculate optimal zoom level if auto-zoom is enabled
    if (_autoZoomEnabled) {
        calculateOptimalZoom(boardViews);
    }
    
    switch (_cameraState) {
        case CameraState::AUTO_CENTERING:
            // Smoothly move camera to center of all boards
            _targetCameraPosition = _autoCenterPosition;
            smoothTransitionToTarget(deltaTime);
            
            // Apply auto-zoom when in auto-centering mode
            if (_autoZoomEnabled) {
                smoothZoomToTarget(deltaTime);
            }
            break;
            
        case CameraState::USER_CONTROLLED:
            // Camera is under user control, just update timeout
            _timeSinceUserInput += deltaTime;
            
            // Check if we should return to auto-centering after timeout
            if (_timeSinceUserInput >= _userControlTimeout) {
                Vector2 currentPos = _camera2D.target;
                float distanceFromCenter = Vector2Distance(currentPos, _autoCenterPosition);
                
                if (distanceFromCenter > _maxDistanceFromCenter) {
                    _cameraState = CameraState::TRANSITIONING;
                    _targetCameraPosition = _autoCenterPosition;
                }
            }
            break;
            
        case CameraState::AUTO_ZOOMING: {
            // Only perform zoom adjustment without moving camera position
            smoothZoomToTarget(deltaTime);
            
            // Check if zoom has reached target or is within acceptable range
            float currentZoom = _use3DRendering ? _camera3D.fovy : _camera2D.zoom;
            if (std::abs(currentZoom - _targetZoom) < 0.05f || 
                (currentZoom >= _minAutoZoom && currentZoom <= _maxAutoZoom)) {
                _cameraState = CameraState::AUTO_CENTERING;
            }
            break;
        }
            
        case CameraState::TRANSITIONING:
            // Smoothly transition to target position
            smoothTransitionToTarget(deltaTime);
            
            // Apply auto-zoom during transition
            if (_autoZoomEnabled) {
                smoothZoomToTarget(deltaTime);
            }
            
            // Check if we've reached the target
            Vector2 currentPos = _camera2D.target;
            float distanceToTarget = Vector2Distance(currentPos, _targetCameraPosition);
            if (distanceToTarget < 10.0f) { // Close enough threshold
                _cameraState = CameraState::AUTO_CENTERING;
            }
            break;
    }
}

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

void CameraController::clampToBounds() {
    if (_use3DRendering) {
        _camera3D.position.x = std::max(0.0f, std::min(_camera3D.position.x, _worldSize.x));
        _camera3D.position.y = std::max(1.0f, std::min(_camera3D.position.y, _worldSize.y));
        _camera3D.position.z = std::max(0.0f, std::min(_camera3D.position.z, _worldSize.z));
        _camera3D.target.x = std::max(0.0f, std::min(_camera3D.target.x, _worldSize.x));
        _camera3D.target.z = std::max(0.0f, std::min(_camera3D.target.z, _worldSize.z));
    } else {
        _camera2D.target.x = std::max(0.0f, std::min(_camera2D.target.x, _worldSize.x));
        _camera2D.target.y = std::max(0.0f, std::min(_camera2D.target.y, _worldSize.y));
    }
}

void CameraController::calculateAutoCenterPosition(const std::vector<std::shared_ptr<BoardView>>& boardViews) {
    if (boardViews.empty()) {
        _autoCenterPosition = { _worldSize.x / 2.0f, _worldSize.y / 2.0f };
        return;
    }
    
    Vector2 totalCenter = { 0.0f, 0.0f };
    int validBoardCount = 0;
    
    for (const auto& boardView : boardViews) {
        if (boardView && !boardView->is3D()) {
            Rectangle area = boardView->getArea();
            Vector2 boardCenter = { 
                area.x + area.width / 2.0f, 
                area.y + area.height / 2.0f 
            };
            totalCenter.x += boardCenter.x;
            totalCenter.y += boardCenter.y;
            validBoardCount++;
        }
    }
    
    if (validBoardCount > 0) {
        _autoCenterPosition.x = totalCenter.x / validBoardCount;
        _autoCenterPosition.y = totalCenter.y / validBoardCount;
    } else {
        _autoCenterPosition = { _worldSize.x / 2.0f, _worldSize.y / 2.0f };
    }
}

void CameraController::calculateOptimalZoom(const std::vector<std::shared_ptr<BoardView>>& boardViews) {
    if (!_autoZoomEnabled || boardViews.empty()) {
        return;
    }
    
    // Calculate the bounding box of all board views
    float minX = FLT_MAX, minY = FLT_MAX;
    float maxX = -FLT_MAX, maxY = -FLT_MAX;
    int validBoardCount = 0;
    
    for (const auto& boardView : boardViews) {
        if (boardView && !boardView->is3D()) {
            Rectangle area = boardView->getArea();
            minX = std::min(minX, area.x);
            minY = std::min(minY, area.y);
            maxX = std::max(maxX, area.x + area.width);
            maxY = std::max(maxY, area.y + area.height);
            validBoardCount++;
        }
    }
    
    if (validBoardCount == 0) {
        _targetZoom = _use3DRendering ? 45.0f : 1.0f; // Default values
        return;
    }
    
    // Calculate the total area covered by boards
    float totalWidth = maxX - minX;
    float totalHeight = maxY - minY;
    
    // Get screen dimensions
    float screenWidth = static_cast<float>(GetScreenWidth());
    float screenHeight = static_cast<float>(GetScreenHeight());
    
    if (_use3DRendering) {
        // For 3D, calculate FOV based on content distance
        // This is a simplified calculation - you might want to adjust based on camera distance
        float maxDimension = std::max(totalWidth, totalHeight);
        float cameraDistance = _camera3D.position.y; // Assuming Y is height
        
        // Calculate FOV needed to see the content
        float targetFOV = 2.0f * atanf(maxDimension / (2.0f * cameraDistance)) * (180.0f / PI);
        
        // Add some padding and clamp to reasonable range
        _targetZoom = std::max(20.0f, std::min(targetFOV * 1.2f, 60.0f));
    } else {
        // For 2D, calculate zoom levels needed to fit content with some padding
        float padding = 50.0f; // Reduced padding for more aggressive zoom
        float zoomX = (screenWidth - padding * 2) / totalWidth;
        float zoomY = (screenHeight - padding * 2) / totalHeight;
        
        // Use the smaller zoom to ensure everything fits
        float optimalZoom = std::min(zoomX, zoomY);
        
        // Ensure we have a reasonable zoom that's different from current
        optimalZoom = std::max(0.3f, std::min(optimalZoom, 2.5f));
        
        // Set target zoom (not clamped to auto-zoom range yet)
        _targetZoom = optimalZoom;
        
        std::cout << "Calculated optimal zoom: " << optimalZoom << " (total area: " << totalWidth << "x" << totalHeight << ")" << std::endl;
    }
}

void CameraController::focusOnNewestBoard(const std::vector<std::shared_ptr<BoardView>>& boardViews) {
    if (boardViews.empty()) {
        return;
    }
    
    // Find the newest board (assume it's the last one in the vector)
    std::shared_ptr<BoardView> newestBoard = nullptr;
    float maxX = -FLT_MAX; // Find rightmost board (newest by timeline position)
    
    for (const auto& boardView : boardViews) {
        if (boardView && !boardView->is3D()) {
            Rectangle area = boardView->getArea();
            if (area.x > maxX) {
                maxX = area.x;
                newestBoard = boardView;
            }
        }
    }
    
    if (!newestBoard) {
        return;
    }
    
    // Set camera target to center of newest board
    Rectangle newestArea = newestBoard->getArea();
    Vector2 newestBoardCenter = {
        newestArea.x + newestArea.width / 2.0f,
        newestArea.y + newestArea.height / 2.0f
    };
    
    _targetCameraPosition = newestBoardCenter;
    
    // Calculate optimal zoom to see nearby boards around the newest one
    calculateOptimalZoomForNewestBoard(boardViews, newestBoard);
    
    // Force immediate transition to focus on newest board
    _cameraState = CameraState::TRANSITIONING;
    _timeSinceUserInput = 0.0f;
    
    std::cout << "Focusing camera on newest board at (" << newestBoardCenter.x << ", " << newestBoardCenter.y << ")" << std::endl;
}

void CameraController::calculateOptimalZoomForNewestBoard(const std::vector<std::shared_ptr<BoardView>>& boardViews, std::shared_ptr<BoardView> newestBoard) {
    if (!_autoZoomEnabled || !newestBoard) {
        return;
    }
    
    Rectangle newestArea = newestBoard->getArea();
    Vector2 newestCenter = {
        newestArea.x + newestArea.width / 2.0f,
        newestArea.y + newestArea.height / 2.0f
    };
    
    // Define nearby range - boards within this distance will be considered
    float nearbyRange = 400.0f; // Adjust this value based on your board spacing
    
    // Find all boards within range of the newest board
    float minX = FLT_MAX, minY = FLT_MAX;
    float maxX = -FLT_MAX, maxY = -FLT_MAX;
    int nearbyBoardCount = 0;
    
    for (const auto& boardView : boardViews) {
        if (boardView && !boardView->is3D()) {
            Rectangle area = boardView->getArea();
            Vector2 boardCenter = {
                area.x + area.width / 2.0f,
                area.y + area.height / 2.0f
            };
            
            float distance = Vector2Distance(newestCenter, boardCenter);
            if (distance <= nearbyRange) {
                minX = std::min(minX, area.x);
                minY = std::min(minY, area.y);
                maxX = std::max(maxX, area.x + area.width);
                maxY = std::max(maxY, area.y + area.height);
                nearbyBoardCount++;
            }
        }
    }
    
    if (nearbyBoardCount == 0) {
        // Fallback to just the newest board
        minX = newestArea.x;
        minY = newestArea.y;
        maxX = newestArea.x + newestArea.width;
        maxY = newestArea.y + newestArea.height;
    }
    
    // Calculate zoom to fit nearby boards with padding
    float totalWidth = maxX - minX;
    float totalHeight = maxY - minY;
    
    float screenWidth = static_cast<float>(GetScreenWidth());
    float screenHeight = static_cast<float>(GetScreenHeight());
    
    if (_use3DRendering) {
        float maxDimension = std::max(totalWidth, totalHeight);
        float cameraDistance = _camera3D.position.y;
        float targetFOV = 2.0f * atanf(maxDimension / (2.0f * cameraDistance)) * (180.0f / PI);
        _targetZoom = std::max(20.0f, std::min(targetFOV * 1.3f, 60.0f));
    } else {
        float padding = 80.0f; // Extra padding to see context around newest board
        float zoomX = (screenWidth - padding * 2) / totalWidth;
        float zoomY = (screenHeight - padding * 2) / totalHeight;
        
        float optimalZoom = std::min(zoomX, zoomY);
        optimalZoom = std::max(0.4f, std::min(optimalZoom, 2.0f));
        
        _targetZoom = optimalZoom;
        
        std::cout << "Calculated optimal zoom for newest board: " << optimalZoom 
                  << " (nearby area: " << totalWidth << "x" << totalHeight 
                  << ", nearby boards: " << nearbyBoardCount << ")" << std::endl;
    }
}

void CameraController::focusOnBoardWithAdaptiveZoom(const std::vector<std::shared_ptr<BoardView>>& boardViews, std::shared_ptr<BoardView> targetBoard) {
    if (!targetBoard || boardViews.empty()) {
        return;
    }
    
    // Constants for adaptive zoom
    const float ZOOM_CUTOFF = 0.8f;      // Below this zoom, trigger adaptive zoom
    const float COMFORTABLE_ZOOM = 1.8f; // Target zoom for comfortable viewing
    const float ANIMATION_DURATION = 0.3f; // Smooth transition duration
    
    // Get current zoom level
    float currentZoom = _use3DRendering ? (90.0f / _camera3D.fovy) : _camera2D.zoom; // Convert FOV to zoom-like scale for 3D
    
    // Only trigger adaptive zoom if current zoom is below cutoff
    if (currentZoom >= ZOOM_CUTOFF) {
        std::cout << "Current zoom (" << currentZoom << ") is above cutoff (" << ZOOM_CUTOFF << "), skipping adaptive zoom" << std::endl;
        return;
    }
    
    // Get target board center
    Rectangle targetArea = targetBoard->getArea();
    Vector2 boardCenter = {
        targetArea.x + targetArea.width / 2.0f,
        targetArea.y + targetArea.height / 2.0f
    };
    
    // Set camera target to center of selected board
    _targetCameraPosition = boardCenter;
    
    // Set zoom target for comfortable viewing
    if (_use3DRendering) {
        // For 3D: Convert comfortable zoom to FOV
        _targetZoom = 90.0f / COMFORTABLE_ZOOM; // Inverse relationship
        _targetZoom = std::max(20.0f, std::min(_targetZoom, 80.0f)); // Clamp FOV
    } else {
        // For 2D: Direct zoom value
        _targetZoom = COMFORTABLE_ZOOM;
        _targetZoom = std::max(0.5f, std::min(_targetZoom, 3.0f)); // Clamp zoom
    }
    
    // Adjust transition speeds for smooth 0.3s animation
    float originalTransitionSpeed = _cameraTransitionSpeed;
    float originalZoomSpeed = _zoomTransitionSpeed;
    
    // Calculate speeds needed to complete transition in ANIMATION_DURATION
    _cameraTransitionSpeed = 3.0f / ANIMATION_DURATION; // Adjust based on typical distance
    _zoomTransitionSpeed = 8.0f / ANIMATION_DURATION;   // Adjust based on typical zoom change
    
    // Force transition to adaptive zoom state
    _cameraState = CameraState::TRANSITIONING;
    _timeSinceUserInput = 0.0f;
    
    std::cout << "Adaptive zoom triggered! Focusing on board at (" << boardCenter.x << ", " << boardCenter.y 
              << ") with target zoom: " << _targetZoom << std::endl;
    
    // Note: Transition speeds will be restored when user takes control or auto-zoom takes over
}

void CameraController::updateCameraState(float deltaTime) {
    // This method can be used for any additional state-specific logic
    // Currently, most state management is handled in update()
}

void CameraController::smoothTransitionToTarget(float deltaTime) {
    if (_use3DRendering) {
        // For 3D camera, interpolate position and target
        Vector3 currentPos = _camera3D.position;
        Vector3 targetPos = { _targetCameraPosition.x, currentPos.y, _targetCameraPosition.y };
        
        _camera3D.position.x = Lerp(currentPos.x, targetPos.x, _cameraTransitionSpeed * deltaTime);
        _camera3D.position.z = Lerp(currentPos.z, targetPos.z, _cameraTransitionSpeed * deltaTime);
        _camera3D.target.x = Lerp(_camera3D.target.x, _targetCameraPosition.x, _cameraTransitionSpeed * deltaTime);
        _camera3D.target.z = Lerp(_camera3D.target.z, _targetCameraPosition.y, _cameraTransitionSpeed * deltaTime);
    } else {
        // For 2D camera, interpolate target
        _camera2D.target.x = Lerp(_camera2D.target.x, _targetCameraPosition.x, _cameraTransitionSpeed * deltaTime);
        _camera2D.target.y = Lerp(_camera2D.target.y, _targetCameraPosition.y, _cameraTransitionSpeed * deltaTime);
    }
    
    clampToBounds();
}

void CameraController::smoothZoomToTarget(float deltaTime) {
    if (!_autoZoomEnabled) {
        return;
    }
    
    if (_use3DRendering) {
        // For 3D camera, adjust FOV
        float currentFOV = _camera3D.fovy;
        float targetFOV = _targetZoom; // For 3D, target zoom is the FOV value
        
        if (std::abs(currentFOV - targetFOV) > 0.1f) {
            _camera3D.fovy = Lerp(currentFOV, targetFOV, _zoomTransitionSpeed * deltaTime);
            // Clamp to valid FOV range
            _camera3D.fovy = std::max(10.0f, std::min(_camera3D.fovy, 90.0f));
        }
    } else {
        // For 2D camera, adjust zoom
        float currentZoom = _camera2D.zoom;
        
        if (std::abs(currentZoom - _targetZoom) > 0.01f) {
            _camera2D.zoom = Lerp(currentZoom, _targetZoom, _zoomTransitionSpeed * deltaTime);
            // Clamp to valid zoom range
            _camera2D.zoom = std::max(0.1f, std::min(_camera2D.zoom, 3.0f));
        }
    }
    
    clampToBounds();
}

void CameraController::renderDebugInfo() const {
    // Background for debug info
    DrawRectangle(5, 30, 400, 200, Fade(BLACK, 0.7f));
    
    // Camera state display
    const char* stateText = "";
    Color stateColor = WHITE;
    switch (_cameraState) {
        case CameraState::AUTO_CENTERING: 
            stateText = "AUTO_CENTERING"; 
            stateColor = GREEN;
            break;
        case CameraState::AUTO_ZOOMING: 
            stateText = "AUTO_ZOOMING"; 
            stateColor = YELLOW;
            break;
        case CameraState::USER_CONTROLLED: 
            stateText = "USER_CONTROLLED"; 
            stateColor = RED;
            break;
        case CameraState::TRANSITIONING: 
            stateText = "TRANSITIONING"; 
            stateColor = BLUE;
            break;
    }
    
    DrawText(TextFormat("Camera State: %s", stateText), 10, 35, 16, stateColor);
    DrawText(TextFormat("Auto Center: (%.1f, %.1f)", _autoCenterPosition.x, _autoCenterPosition.y), 10, 55, 14, WHITE);
    DrawText(TextFormat("Auto Zoom: %s | Target: %.2f", _autoZoomEnabled ? "ON" : "OFF", _targetZoom), 10, 75, 14, _autoZoomEnabled ? GREEN : RED);
    DrawText(TextFormat("Auto Zoom Range: [%.2f, %.2f]", _minAutoZoom, _maxAutoZoom), 10, 95, 14, WHITE);
    DrawText(TextFormat("Time Since Input: %.1fs", _timeSinceUserInput), 10, 115, 14, WHITE);
    
    if (_use3DRendering) {
        DrawText(TextFormat("Camera3D Pos: (%.1f, %.1f, %.1f)", _camera3D.position.x, _camera3D.position.y, _camera3D.position.z), 10, 135, 14, WHITE);
        DrawText(TextFormat("Camera3D Target: (%.1f, %.1f, %.1f)", _camera3D.target.x, _camera3D.target.y, _camera3D.target.z), 10, 155, 14, WHITE);
        DrawText(TextFormat("Current FOV: %.2f", _camera3D.fovy), 10, 175, 14, WHITE);
    } else {
        DrawText(TextFormat("Camera2D Target: (%.1f, %.1f)", _camera2D.target.x, _camera2D.target.y), 10, 135, 14, WHITE);
        DrawText(TextFormat("Current Zoom: %.2f", _camera2D.zoom), 10, 155, 14, WHITE);
    }
    
    // Instructions
    DrawText("Controls: Z=Toggle Auto-Zoom, X=Force Auto-Zoom", 10, 195, 12, LIGHTGRAY);
    DrawText("Mouse: Drag=Pan, Wheel=Zoom", 10, 210, 12, LIGHTGRAY);
}


