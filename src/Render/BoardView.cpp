#include "BoardView.h"
#include "View.h"
#include "chess.h"
#include "raymath.h"
#include "PieceTheme.h"
#include <iostream>


const float BOARD_WORLD_SIZE = 250.0f; // Assuming a standard chess board size
const float HORIZONTAL_SPACING = 60.0f; // Size of each square on the board
const float VERTICAL_SPACING = 60.0f; // Size of each square on the board
const int STANDARD_BOARD_DIM = 8;


void BoardView2D::render() const {
    if (!_boardTexture) {
        std::cerr << "Board texture not set!" << std::endl;
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
        Rectangle{0, 0, static_cast<float>(_boardTexture->width * float(float(_boardDim) / float(STANDARD_BOARD_DIM))), static_cast<float>(_boardTexture->height * (float(_boardDim) / float(STANDARD_BOARD_DIM)))},
        _area,
        Vector2{0, 0},
        0.0f,
        WHITE
    );

    render_pieces();
}

bool BoardView2D::isMouseOverBoard() const {
    Vector2 screenMousePos = GetMousePosition();
    bool isMouseOver = false;
    if (_camera) {
        Vector2 worldMousePos = GetScreenToWorld2D(screenMousePos, *_camera);
        isMouseOver = CheckCollisionPointRec(worldMousePos, _area);
    } else {
        isMouseOver = CheckCollisionPointRec(screenMousePos, _area);
    }
    return isMouseOver;
}

bool BoardView2D::isMouseClickedOnBoard() const {
    return isMouseOverBoard() && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
}


void BoardView2D::setSupervisor(ChessView* supervisor) {
    _supervisor = supervisor;
}

Chess::Position2D BoardView2D::getMouseOverPosition() const {
    if (isMouseOverBoard()) {
        Vector2 mousePos = GetMousePosition();
        if (_camera) {
            Vector2 worldMousePos = GetScreenToWorld2D(mousePos, *_camera);
            return Chess::Position2D{
                static_cast<int>((worldMousePos.x - _area.x) / (_area.width / _boardDim)),
                static_cast<int>((worldMousePos.y - _area.y) / (_area.height / _boardDim))
            };
        } else {
            return Chess::Position2D{
                static_cast<int>((mousePos.x - _area.x) / (_area.width / _boardDim)),
                static_cast<int>((mousePos.y - _area.y) / (_area.height / _boardDim))
            };
        }
    }
    return Chess::Position2D{-1, -1}; // Invalid position
}

Chess::Position2D BoardView2D::getMouseClickedPosition() const {
    if (isMouseClickedOnBoard()) {
        Vector2 mousePos = GetMousePosition();
        if (_camera) {
            Vector2 worldMousePos = GetScreenToWorld2D(mousePos, *_camera);
            return Chess::Position2D{
                static_cast<int>((worldMousePos.x - _area.x) / (_area.width / _boardDim)),
                static_cast<int>((worldMousePos.y - _area.y) / (_area.height / _boardDim))
            };
        } else {
            return Chess::Position2D{
                static_cast<int>((mousePos.x - _area.x) / (_area.width / _boardDim)),
                static_cast<int>((mousePos.y - _area.y) / (_area.height / _boardDim))
            };
        }
    }
    return Chess::Position2D{-1, -1};
}

void BoardView2D::render_highlightBoundaries() const {
    DrawRectangleLinesEx(_area, 2, RED);
}

void BoardView2D::render_highlightedPositions(std::vector<Chess::Position2D> positions) const {
    for (const auto& pos : positions) {
        DrawRectangle(
            _area.x + pos.x() * _area.width / _boardDim,
            _area.y + pos.y() * _area.height / _boardDim,
            _area.width / _boardDim,
            _area.height / _boardDim,
            (Color){0, 255, 0, 100} // Semi-transparent green
        );
    }
}

void BoardView2D::render_pieces() const {
    for (const auto& [pos, pieceName] : _piecePositions) {
        ThemeManager::getInstance().setTheme(std::make_unique<ModernTheme>());
        Texture2D& texture = ThemeManager::getInstance().getPieceTexture(pieceName);
        Vector2 piecePosition = {
            _area.x + pos.x() * _area.width / _boardDim,
            _area.y + pos.y() * _area.height / _boardDim
        };
        DrawTexturePro(
                texture,
                Rectangle{0, 0, static_cast<float>(texture.width), static_cast<float>(texture.height)},
                Rectangle{piecePosition.x, piecePosition.y, _area.width / _boardDim, _area.height / _boardDim},
                Vector2{0, 0},
                0.0f,
                WHITE
        );
    }
}

// void BoardView3D::setSupervisor(ChessView* supervisor) {
//     _supervisor = supervisor;
// }



// BoardView3D::BoardView3D(std::shared_ptr<Chess::Board> board, Texture2D* texture, Vector3 position, Camera3D* camera)
//     : _board(board), _boardTexture(texture), _position(position), _camera(camera) {
//     Mesh mesh = GenMeshPlane(_boardSize, _boardSize, 1, 1);
//     _boardModel = LoadModelFromMesh(mesh);
//     _boardModel.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = *_boardTexture;
// }

// BoardView3D::~BoardView3D() {
//     UnloadModel(_boardModel);
// }

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

// void BoardView3D::setBoard(std::shared_ptr<Chess::Board> board) {
//     _board = board;
// }

// void BoardView3D::setBoardTexture(Texture2D* texture) {
//     _boardTexture = texture;
//     _boardModel.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = *texture;
// }

// void BoardView3D::setActive(bool active) {
//     _isActive = active;
// }

// bool BoardView3D::isActive() const {
//     return _isActive;
// }

// void BoardView3D::updateHighlightedPositions(const std::vector<Chess::Position2D>& positions) {
//     _highlightedPositions = positions;
// }

// bool BoardView3D::is3D() const {
//     return true;
// }

// void BoardView3D::setPosition(Vector3 position) {
//     _position = position;
// }

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

