#include "View.h"
#include "chess.h"
#include "raymath.h"
#include "PieceTheme.h"
#include <iostream>
#include "BoardView.h"
#include <algorithm>




ChessView::ChessView(Vector3 worldSize)
    : _worldSize(worldSize) {
    // Initialize 2D camera
    _camera2D.target = { worldSize.x / 2, worldSize.y / 2 };
    _camera2D.offset = { GetScreenWidth() / 2.0f, GetScreenHeight() / 2.0f };
    _camera2D.rotation = 0.0f;
    _camera2D.zoom = 1.0f;

    // Initialize 3D camera
    _camera3D.position = { worldSize.x / 2, 10.0f, worldSize.z / 2 }; // Position above the world
    _camera3D.target = { worldSize.x / 2, 0.0f, worldSize.z / 2 }; // Look at world center
    _camera3D.up = { 0.0f, 1.0f, 0.0f }; // Up vector
    _camera3D.fovy = 45.0f; // Field of view
    _camera3D.projection = CAMERA_PERSPECTIVE; // Perspective projection
}


void ChessView::ClampCameraToBounds() {
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


void ChessView::setZoom(float zoom) {
    if (_use3DRendering) {
        _camera3D.fovy = std::max(10.0f, std::min(zoom, 90.0f)); // Clamp 3D FOV
    } else {
        _camera2D.zoom = std::max(0.1f, std::min(zoom, 3.0f)); // Clamp 2D zoom
    }
    ClampCameraToBounds();
}


void ChessView::setCameraTarget(Vector2 target) {
    if (_use3DRendering) {
        _camera3D.target = { target.x, _camera3D.target.y, target.y };
    } else {
        _camera2D.target = target;
    }
    ClampCameraToBounds();
}


void ChessView::moveCamera(Vector2 delta) {
    if (_use3DRendering) {
        _camera3D.position.x -= delta.x * 0.01f;
        _camera3D.position.z -= delta.y * 0.01f;
        _camera3D.target.x -= delta.x * 0.01f;
        _camera3D.target.z -= delta.y * 0.01f;
    } else {
        _camera2D.target.x -= delta.x * 0.01f;
        _camera2D.target.y -= delta.y * 0.01f;
    }
    ClampCameraToBounds();
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


    // std::cout << "Capture mouse wheel \n";
    // Handle camera2D zoom based on mouse wheel

    // // Handle camera movement
    // if (_use3DRendering) {
    //     if (IsKeyDown(KEY_W)) moveCamera({0.0f, 0.0f});
    //     if (IsKeyDown(KEY_S)) moveCamera({0.0f, 0.0f});
    //     if (IsKeyDown(KEY_A)) moveCamera({-5.0f, 0.0f});
    //     if (IsKeyDown(KEY_D)) moveCamera({5.0f, 0.0f});
    //     if (IsKeyDown(KEY_Q)) _camera3D.position.y += 5.0f; // Move up
    //     if (IsKeyDown(KEY_E)) _camera3D.position.y -= 5.0f; // Move down
    // } else {
    //     if (IsKeyDown(KEY_W)) moveCamera({0.0f, -5.0f});
    //     if (IsKeyDown(KEY_S)) moveCamera({0.0f, 5.0f});
    //     if (IsKeyDown(KEY_A)) moveCamera({-5.0f, 0.0f});
    //     if (IsKeyDown(KEY_D)) moveCamera({5.0f, 0.0f});
    // }

    // if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) {
    //     Vector2 mouseDelta = GetMouseDelta();
    //     moveCamera({-mouseDelta.x, -mouseDelta.y});
    // }

    // float wheel = GetMouseWheelMove();
    // if (wheel != 0) {
    //     setZoom(_use3DRendering ? _camera3D.fovy : _camera2D.zoom + wheel * 0.1f);
    // }

    // if (_onMouseClickCallback && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
    //     _onMouseClickCallback(GetMousePosition());
    // }
}


void ChessView::updateCamera(float deltaTime) {
    // Center camera on active board view, if any
    for (const auto& boardView : _boardViews) {
        if (boardView->is3D()) {
            // auto boardView3D = std::dynamic_pointer_cast<BoardView3D>(boardView);
            // if (boardView3D) {
            //     Vector3 pos = boardView3D->getPosition();
            //     setCameraTarget({ pos.x, pos.z });
            // }
        } else {
            Rectangle area = boardView->getArea();
            Vector2 boardCenter = { area.x + area.width / 2.0f, area.y + area.height / 2.0f };
            setCameraTarget(boardCenter);
        }
    }

    // Handle mouse wheel zoom
    float wheel = GetMouseWheelMove();
    if (wheel != 0) {
        std::cout << "Mouse wheel moved: " << wheel << std::endl;
        setZoom(_use3DRendering ? _camera3D.fovy + wheel * 0.1f : _camera2D.zoom + wheel * 0.1f);
    }

    // /// Handle mouse drag
    // if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) {
    //     Vector2 mouseDelta = GetMouseDelta();
    //     moveCamera({-mouseDelta.x, -mouseDelta.y});
    // }
    
}

void ChessView::update(float deltaTime) {
    updateCamera(deltaTime);
    // updateSelectedBoardView();
    // updateSelectedPosition();
}


void ChessView::render_boardViews() const {
    BeginMode2D(_camera2D);

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
    Vector2 worldPos = _use3DRendering ? Vector2{0, 0} : GetScreenToWorld2D(mousePos, _camera2D);
    DrawText("Game World", 10, 10, 20, BLACK);
    if (_use3DRendering) {
        DrawText(TextFormat("Camera3D Pos: (%.2f, %.2f, %.2f)", _camera3D.position.x, _camera3D.position.y, _camera3D.position.z), 10, 35, 20, BLACK);
        DrawText(TextFormat("Camera3D Target: (%.2f, %.2f, %.2f)", _camera3D.target.x, _camera3D.target.y, _camera3D.target.z), 10, 60, 20, BLACK);
        DrawText(TextFormat("FOV: %.2f", _camera3D.fovy), 10, 85, 20, BLACK);
    } else {
        DrawText(TextFormat("Camera2D Target: (%.2f, %.2f)", _camera2D.target.x, _camera2D.target.y), 10, 35, 20, BLACK);
        DrawText(TextFormat("Zoom: %.2f", _camera2D.zoom), 10, 60, 20, BLACK);
    }
    DrawText(TextFormat("Mouse Screen Pos: (%.2f, %.2f)", mousePos.x, mousePos.y), 10, 110, 20, BLACK);
    DrawText(TextFormat("Mouse World Pos: (%.2f, %.2f)", worldPos.x, worldPos.y), 10, 135, 20, BLACK);
}


void ChessView::addBoardView(std::shared_ptr<BoardView> boardView) {
    if (boardView) {
        boardView -> setSupervisor(this);
        _boardViews.push_back(boardView);
        // Set the appropriate camera based on board view type
        if (boardView->is3D()) {
            _use3DRendering = true;
            boardView -> setCamera3D(getCamera3D());
        } else {
            boardView -> setCamera2D(getCamera2D());
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
            _use3DRendering = false;
            for (const auto& view : _boardViews) {
                if (view->is3D()) {
                    _use3DRendering = true;
                    break;
                }
            }
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
    if (_use3DRendering) {
        // 3D rendering code
        return;
    }

    BeginMode2D(_camera2D);
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
    if (_use3DRendering) {
        // 3D rendering code for highlighted positions
        return;
    }
    BeginMode2D(_camera2D);
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
