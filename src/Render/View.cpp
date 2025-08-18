#include "View.h"
#include "chess.h"
#include "raymath.h"
#include "PieceTheme.h"
#include <iostream>
#include "BoardView.h"

const float BOARD_WORLD_SIZE = 250.0f; // Assuming a standard chess board size
const float HORIZONTAL_SPACING = 60.0f; // Size of each square on the board
const float VERTICAL_SPACING = 60.0f; // Size of each square on the board



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
    std::shared_ptr<BoardView> previousSelectedBoardView = _selectedBoardView;
    
    for (auto& boardView : _boardViews) if (boardView) {
        if (boardView -> isMouseClickedOnBoard()) {
            _selectedBoardView = boardView;  // Store original reference, not clone
            if (boardView -> getMouseClickedPosition() != Chess::Position2D{-1, -1}) {
                _selectedPosition = boardView -> getMouseClickedPosition();
            }
        }
    }

    if (_selectedBoardView) {
        if (!previousSelectedBoardView) {
            std::cout << "Selected NEW board\n";
            /// @brief handle new board selection
            // if (_onMouseBoardClickCallback) {
                // _onMouseBoardClickCallback(_selectedBoardView);
            // }
        }
        else if (previousSelectedBoardView == _selectedBoardView) {
            if (_selectedPosition.x() != -1 && _selectedPosition.y() != -1) {
                std::cout << "Selected position: " << _selectedPosition.x() << ", " << _selectedPosition.y() << std::endl;
                /// @brief handle position selection on SAME board
                // if (_onPositionClickCallback) {
                    // _onPositionClickCallback(_selectedPosition);
                // }
            }
        }
        else {
            std::cout << "Selected DIFFERENT board\n";
            /// @brief handle different board selection
            // if (_onMouseBoardClickCallback) {
                // _onMouseBoardClickCallback(_selectedBoardView);
            // }
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

    float wheel = GetMouseWheelMove();
    if (wheel != 0) {
        std::cout << "Mouse wheel moved: " << wheel << std::endl;
        setZoom(_use3DRendering ? _camera3D.fovy + wheel * 0.1f : _camera2D.zoom + wheel * 0.1f);
    }

}

void ChessView::update(float deltaTime) {
    updateCamera(deltaTime);
    // updateSelectedBoardView();
    // updateSelectedPosition();
}


void ChessView::render() const {
    if (_use3DRendering) {
        BeginMode3D(_camera3D);
        for (const auto& boardView : _boardViews) {
            if (boardView && boardView->is3D()) {
                boardView->render();
            }
        }
        EndMode3D();
        // Render 2D views on top if mixed
        BeginMode2D(_camera2D);
        for (const auto& boardView : _boardViews) {
            if (boardView && !boardView->is3D()) {
                boardView->render();
            }
            // std::cout << "Rendering BoardView2D" << std::endl;
        }
        EndMode2D();
    } else {
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
            if (boardView == _selectedBoardView) {
                _selectedBoardView = nullptr;
            }
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

void ChessView::queueUpdateInvalidBoardSelection() {
    updateQueue.push_back([this](){
        if (_selectedBoardView) {
            _isSelectedBoardViewInvalid = true;
        }
        else {
            _isSelectedBoardViewInvalid = false;
        }
    });  
}

void ChessView::queueUpdateMoveState(const RenderMoveState& rmoveState) {
    updateQueue.push_back([this, rmoveState](){
        // if 
    });
}

void ChessView::clearBoardViews() {
    _boardViews.clear();
    _selectedBoardView = nullptr;
    _selectedPosition = Chess::Position2D(-1, -1);
    _isSelectedBoardViewInvalid = false;
}