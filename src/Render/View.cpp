#include "View.h"
#include "chess.h"
#include "raymath.h"
#include "PieceTheme.h"
#include <iostream>

const int BOARD_SIZE = 200; // Assuming a standard chess board size
const int HORIZONTAL_SPACING = 30; // Size of each square on the board
const int VERTICAL_SPACING = 30; // Size of each square on the board

BoardView2D::BoardView2D(std::shared_ptr<Chess::Board> board, Texture2D* texture)
    : _board(board), _boardTexture(texture), _camera(nullptr) {
    if (!_board) {
        std::cerr << "Board pointer is null!" << std::endl;
    }
}


void BoardView2D::render() const {
    if (!_board || !_boardTexture) {
        std::cerr << "Board or texture not set!" << std::endl;
        return;
    }
    if (_area.width <= 0 || _area.height <= 0) {
        std::cerr << "Invalid render area dimensions!" << std::endl;
        return;
    }

    // Draw the board
    if (_boardTexture == nullptr) {
        std::cerr << "Board texture is null!" << std::endl;
        return;
    }

    DrawTexturePro(
        *_boardTexture,
        Rectangle{0, 0, static_cast<float>(_boardTexture->width), static_cast<float>(_boardTexture->height)},
        _area,
        Vector2{0, 0},
        0.0f,
        WHITE
    );

    // Draw pieces
    for (int i = 0; i < 8; ++i) {
        for (int j = 0; j < 8; ++j) {
            Chess::Position2D pos(i, j);
            auto piece = _board->getPiece(pos);
            if (piece) {
              const std::string& name = piece->name();
              const std::string& color = piece->color() == Chess::PieceColor::PIECEWHITE ? "white" : "black";
              const std::string& pieceName = color + "_" + name;
              Texture2D& texture = ThemeManager::getInstance().getPieceTexture(pieceName);
              Vector2 piecePosition = {
                  _area.x + i * _area.width / 8,
                  _area.y + j * _area.height / 8
              };
              DrawTexturePro(
                  texture,
                  Rectangle{0, 0, static_cast<float>(texture.width), static_cast<float>(texture.height)},
                  Rectangle{piecePosition.x, piecePosition.y, _area.width / 8, _area.height / 8},
                  Vector2{0, 0},
                  0.0f,
                  WHITE
              );
            }
        }
    }

    // Draw highlighted positions
    for (const auto& pos : _highlightedPositions) {
        DrawRectangle(
            _area.x + pos.x()  * _area.width / 8,
            _area.y + pos.y() * _area.height / 8,
            _area.width / 8,
            _area.height / 8,
            (Color){0, 255, 0, 100} // Semi-transparent green
        );
    }

    if (_isActive) {
        drawSelectedBoundaries();
    } else if (_isMouseOver) {
        drawMouseOverBoundaries();
    }
}


void BoardView2D::update(float deltaTime) {
    updateMouseOverBoard();
}

void BoardView2D::updateMouseOverBoard() {
    Vector2 screenMousePos = GetMousePosition();
    if (_camera) {
        Vector2 worldMousePos = GetScreenToWorld2D(screenMousePos, *_camera);
        _isMouseOver = CheckCollisionPointRec(worldMousePos, _area);
    } else {
        _isMouseOver = CheckCollisionPointRec(screenMousePos, _area);
    }
}

bool BoardView2D::isMouseClickedOnBoard() const {
    if (_isMouseOver) {
        Vector2 mousePos = GetMousePosition();
        if (_camera) {
            Vector2 worldMousePos = GetScreenToWorld2D(mousePos, *_camera);
            return IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(worldMousePos, _area);
        } else {
            return IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(mousePos, _area);
        }
    }
    return false;
}

void BoardView2D::handleInput() {
    if (_isMouseOver && isMouseClickedOnBoard()) {
        setActive(true);
    }
}

void BoardView2D::drawSelectedBoundaries() const {
    DrawRectangleLinesEx(_area, 2, RED);
}
void BoardView2D::drawMouseOverBoundaries() const {
    DrawRectangleLinesEx(_area, 2, BLUE);
}

GameWorld::GameWorld(Vector3 worldSize)
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


void GameWorld::ClampCameraToBounds() {
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


void GameWorld::setZoom(float zoom) {
    if (_use3DRendering) {
        _camera3D.fovy = std::max(10.0f, std::min(zoom, 90.0f)); // Clamp 3D FOV
    } else {
        _camera2D.zoom = std::max(0.1f, std::min(zoom, 3.0f)); // Clamp 2D zoom
    }
    ClampCameraToBounds();
}


void GameWorld::setCameraTarget(Vector2 target) {
    if (_use3DRendering) {
        _camera3D.target = { target.x, _camera3D.target.y, target.y };
    } else {
        _camera2D.target = target;
    }
    ClampCameraToBounds();
}


void GameWorld::moveCamera(Vector2 delta) {
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



void GameWorld::handleInput() {
    for (auto& boardView : _boardViews) {
        if (boardView) {
            boardView->handleInput();
            if (boardView->isActive() && _selectedBoardView != boardView) {
                if (_selectedBoardView) {
                    _selectedBoardView->setActive(false);
                }
                // _selectedBoardView = boardView;
                // if (_onBoardSelectCallback) {
                    // _onBoardSelectCallback(boardView->getBoard());
                // }
            }
        } else {
            std::cerr << "Null BoardView encountered!" << std::endl;
        }
    }

    // Handle camera movement
    if (_use3DRendering) {
        if (IsKeyDown(KEY_W)) moveCamera({0.0f, 0.0f});
        if (IsKeyDown(KEY_S)) moveCamera({0.0f, 0.0f});
        if (IsKeyDown(KEY_A)) moveCamera({-5.0f, 0.0f});
        if (IsKeyDown(KEY_D)) moveCamera({5.0f, 0.0f});
        if (IsKeyDown(KEY_Q)) _camera3D.position.y += 5.0f; // Move up
        if (IsKeyDown(KEY_E)) _camera3D.position.y -= 5.0f; // Move down
    } else {
        if (IsKeyDown(KEY_W)) moveCamera({0.0f, -5.0f});
        if (IsKeyDown(KEY_S)) moveCamera({0.0f, 5.0f});
        if (IsKeyDown(KEY_A)) moveCamera({-5.0f, 0.0f});
        if (IsKeyDown(KEY_D)) moveCamera({5.0f, 0.0f});
    }

    if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) {
        Vector2 mouseDelta = GetMouseDelta();
        moveCamera({-mouseDelta.x, -mouseDelta.y});
    }

    float wheel = GetMouseWheelMove();
    if (wheel != 0) {
        setZoom(_use3DRendering ? _camera3D.fovy : _camera2D.zoom + wheel * 0.1f);
    }

    if (_onMouseClickCallback && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        _onMouseClickCallback(GetMousePosition());
    }
}


void GameWorld::updateCamera(float deltaTime) {
    // Center camera on active board view, if any
    for (const auto& boardView : _boardViews) {
        if (boardView && boardView->isActive()) {
            if (boardView->is3D()) {
                auto boardView3D = std::dynamic_pointer_cast<BoardView3D>(boardView);
                // if (boardView3D) {
                //     Vector3 pos = boardView3D->getPosition();
                //     setCameraTarget({ pos.x, pos.z });
                // }
            } else {
                auto boardView2D = std::dynamic_pointer_cast<BoardView2D>(boardView);
                if (boardView2D) {
                    Rectangle area = boardView2D->getArea();
                    Vector2 boardCenter = { area.x + area.width / 2.0f, area.y + area.height / 2.0f };
                    setCameraTarget(boardCenter);
                }
            }
            break; // Only target the first active board view
        }
    }
}

void GameWorld::update(float deltaTime) {
    for (auto& boardView : _boardViews) {
        if (boardView) {
            boardView->update(deltaTime);
        } else {
            std::cerr << "Null BoardView encountered!" << std::endl;
        }
    }
    updateCamera(deltaTime);
}


void GameWorld::render() const {
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


void GameWorld::addBoardView(std::shared_ptr<BoardView> boardView) {
    if (boardView) {
        _boardViews.push_back(boardView);
        // Set the appropriate camera based on board view type
        if (boardView->is3D()) {
            _use3DRendering = true;
            // Cast to BoardView3D to set Camera3D (requires dynamic_cast if setCamera is not in BoardView)
            auto boardView3D = std::dynamic_pointer_cast<BoardView3D>(boardView);
            // if (boardView3D) {
                // boardView3D->setPosition({ boardView3D->getArea().x, 0.0f, boardView3D->getArea().y });
                // boardView3D->setCamera(getCamera3D());
            // }
        } else {
            auto boardView2D = std::dynamic_pointer_cast<BoardView2D>(boardView);
            if (boardView2D) {
                boardView2D->setCamera(getCamera2D());
            }
        }
    } else {
        std::cerr << "Attempted to add a null BoardView!" << std::endl;
    }
}


void GameWorld::removeBoardView(std::shared_ptr<BoardView> boardView) {
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


BoardView3D::BoardView3D(std::shared_ptr<Chess::Board> board, Texture2D* texture, Vector3 position, Camera3D* camera)
    : _board(board), _boardTexture(texture), _position(position), _camera(camera) {
    Mesh mesh = GenMeshPlane(_boardSize, _boardSize, 1, 1);
    _boardModel = LoadModelFromMesh(mesh);
    _boardModel.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = *_boardTexture;
}

BoardView3D::~BoardView3D() {
    UnloadModel(_boardModel);
}

// void BoardView3D::update(float deltaTime) {
//     updateMouseOverBoard();
// }

// void BoardView3D::handleInput() {
    // if (_isMouseOver && isMouseClicked()) {
        // setActive(true);
    // }
// }

// void BoardView3D::render() const {
    // BeginMode3D(*_camera);
    // DrawModel(_boardModel, _position, 1.0f, _isActive ? WHITE : (Color){255, 255, 255, 200});

    // float squareSize = _boardSize / 8.0f;
    // for (int row = 0; row < 8; row++) {
    //     for (int col = 0; col < 8; col++) {
    //         if (auto piece = _board->getPiece(row, col)) {
    //             Texture2D* pieceTexture = getPieceTexture(piece);
    //             Vector3 piecePos = {
    //                 _position.x - _boardSize/2 + (col + 0.5f) * squareSize,
    //                 _position.y + 0.01f, // Avoid z-fighting
    //                 _position.z - _boardSize/2 + (row + 0.5f) * squareSize
    //             };
    //             DrawBillboard(*_camera, *pieceTexture, piecePos, squareSize, WHITE);
    //         }
    //     }
    // }

    // for (const auto& pos : _highlightedPositions) {
    //     Vector3 highlightPos = {
    //         _position.x - _boardSize/2 + (pos.col + 0.5f) * squareSize,
    //         _position.y + 0.02f,
    //         _position.z - _boardSize/2 + (pos.row + 0.5f) * squareSize
    //     };
    //     DrawCube(highlightPos, squareSize, 0.01f, squareSize, (Color){0, 255, 0, 100});
    // }
    // EndMode3D();
// }

void BoardView3D::setBoard(std::shared_ptr<Chess::Board> board) {
    _board = board;
}

void BoardView3D::setBoardTexture(Texture2D* texture) {
    _boardTexture = texture;
    _boardModel.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = *texture;
}

void BoardView3D::setActive(bool active) {
    _isActive = active;
}

bool BoardView3D::isActive() const {
    return _isActive;
}

void BoardView3D::updateHighlightedPositions(const std::vector<Chess::Position2D>& positions) {
    _highlightedPositions = positions;
}

bool BoardView3D::is3D() const {
    return true;
}

void BoardView3D::setPosition(Vector3 position) {
    _position = position;
}

// void BoardView3D::updateMouseOverBoard() {
    // Ray ray = GetMouseRay(GetMousePosition(), *_camera);
    // BoundingBox box = {
    //     {_position.x - _boardSize/2, _position.y - 0.1f, _position.z - _boardSize/2},
    //     {_position.x + _boardSize/2, _position.y + 0.1f, _position.z + _boardSize/2}
    // };
    // _isMouseOver = CheckCollisionRayBox(ray, box);
// }

// bool BoardView3D::isMouseClicked() const {
    // return IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
// }

