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

    for (int i = 0; i < _boardDim; ++i) {
        for (int j = 0; j < _boardDim; ++j) {
            Vector2 position = {
                _area.x + float(i) * (_area.width / float(1.0 * _boardDim)),
                _area.y + float(j) * (_area.height / float(1.0 * _boardDim))
            };
            DrawRectangle(
                position.x,
                position.y,
                float(_area.width) / float(1.0 * _boardDim),
                float(_area.height) / float(1.0 * _boardDim),
                (i + j) % 2 == 0 ? Color{243, 233, 220, 255} : Color{248, 178, 89, 255} // Alternate colors
            );
        }
    }
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
            Color{0, 255, 0, 100} // Semi-transparent green
        );
    }
}

void BoardView2D::render_highlightPiece(Chess::Position2D piecePosition) const {
    if (piecePosition.x() < 0 || piecePosition.y() < 0 || 
        piecePosition.x() >= _boardDim || piecePosition.y() >= _boardDim) {
        return;
    }
    std::string pieceName;
    for (auto & piece : _piecePositions) {
        if (piece.first == piecePosition) {
            pieceName = piece.second;
            break;
        }
    }
    if (pieceName.empty()) {
        return;
    }
    Texture2D& texture = ThemeManager::getInstance().getPieceTexture(pieceName);
    Vector2 position = {
        _area.x + piecePosition.x() * _area.width / _boardDim,
        _area.y + piecePosition.y() * _area.height / _boardDim
    };
    
    float squareWidth = _area.width / _boardDim;
    float squareHeight = _area.height / _boardDim;
    
    // Draw a highlight background behind the piece
    DrawRectangle(
        position.x,
        position.y,
        squareWidth,
        squareHeight,
        Color{228, 0, 75, 100} // Semi-transparent pink highlight rgb(228, 0, 75)
    );
    
    // Draw the piece with a slight glow effect
    DrawTexturePro(
        texture,
        Rectangle{0, 0, static_cast<float>(texture.width), static_cast<float>(texture.height)},
        Rectangle{position.x, position.y, squareWidth, squareHeight},
        Vector2{0, 0},
        0.0f,
        WHITE
    );
}

void BoardView2D::render_pieces() const {
    for (const auto& [pos, pieceName] : _piecePositions) {
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
