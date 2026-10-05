#include "BoardView.h"
#include "View.h"
#include "chess.h"
#include "raymath.h"
#include "PieceTheme.h"
#include "Render/UITheme.h"
#include <algorithm>
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

    for (int x = 0; x < _boardDim; ++x) {
        for (int y = 0; y < _boardDim; ++y) {
            const Rectangle sq = squareRect(Chess::Position2D(x, y));
            // Engine (0,0) is light; after the rotation it is White's bottom-right corner
            const bool light = (x + y) % 2 == 0;
            DrawRectangle(sq.x, sq.y, sq.width, sq.height,
                          light ? UI::Color::squareLight : UI::Color::squareDark);
        }
    }
    render_pieces();

    // Border: accent for boards the current player may still move from, neutral otherwise
    if (_moveable) {
        DrawRectangleLinesEx(_area, worldThickness(3.0f), UI::withAlpha(UI::Color::accent, 170));
    } else {
        DrawRectangleLinesEx(_area, worldThickness(2.0f), UI::Color::border);
    }
}

// The view is the engine grid rotated 180 degrees: White (y=0) is at the BOTTOM and the
// engine's x=0 file is at the RIGHT, which puts the queen on d1 and the king on e1.
// Each mapping is its own inverse, so one helper serves both directions.
int BoardView2D::colToScreen(int x) const { return _boardDim - 1 - x; }
int BoardView2D::screenToCol(int screenCol) const { return _boardDim - 1 - screenCol; }
int BoardView2D::rowToScreen(int y) const { return _boardDim - 1 - y; }
int BoardView2D::screenToRow(int screenRow) const { return _boardDim - 1 - screenRow; }

Rectangle BoardView2D::squareRect(Chess::Position2D pos) const {
    const float sw = _area.width / _boardDim;
    const float sh = _area.height / _boardDim;
    return Rectangle{_area.x + colToScreen(pos.x()) * sw, _area.y + rowToScreen(pos.y()) * sh, sw, sh};
}

Chess::Position2D BoardView2D::worldToPosition(Vector2 world) const {
    const int col = static_cast<int>((world.x - _area.x) / (_area.width / _boardDim));
    const int row = static_cast<int>((world.y - _area.y) / (_area.height / _boardDim));
    return Chess::Position2D{screenToCol(col), screenToRow(row)};
}

Chess::Position2D BoardView2D::mouseToPosition() const {
    Vector2 mousePos = GetMousePosition();
    if (_camera) mousePos = GetScreenToWorld2D(mousePos, *_camera);
    return worldToPosition(mousePos);
}

float BoardView2D::worldThickness(float px) const {
    const float zoom = (_camera && _camera->zoom > 0.01f) ? _camera->zoom : 1.0f;
    return px / zoom; // constant on screen
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
    if (isMouseOverBoard()) return mouseToPosition();
    return Chess::Position2D{-1, -1}; // Invalid position
}

Chess::Position2D BoardView2D::getMouseClickedPosition() const {
    if (isMouseClickedOnBoard()) return mouseToPosition();
    return Chess::Position2D{-1, -1};
}

void BoardView2D::render_highlightBoundaries() const {
    DrawRectangleLinesEx(_area, worldThickness(4.0f), UI::Color::selected);
}

void BoardView2D::render_highlightedPositions(std::vector<Chess::Position2D> positions) const {
    const float sw = _area.width / _boardDim;
    const float sh = _area.height / _boardDim;
    for (const auto& pos : positions) {
        const Rectangle sq = squareRect(pos);
        const Vector2 center = {sq.x + sw / 2, sq.y + sh / 2};
        const bool occupied = std::any_of(_piecePositions.begin(), _piecePositions.end(),
                                          [&](const auto& p) { return p.first == pos; });
        if (occupied) {
            // Capture: ring around the enemy piece
            DrawRing(center, sw * 0.40f, sw * 0.46f, 0, 360, 36, UI::Color::capture);
        } else {
            // Legal empty target: sage dot
            DrawCircleV(center, sw * 0.17f, UI::withAlpha(UI::Color::legalTarget, 220));
        }
    }
}

void BoardView2D::render_hoverSquare(Chess::Position2D pos) const {
    if (pos.x() < 0 || pos.y() < 0 || pos.x() >= _boardDim || pos.y() >= _boardDim) return;
    const Rectangle sq = squareRect(pos);
    DrawRectangle(sq.x, sq.y, sq.width, sq.height, UI::Color::hover);
}

void BoardView2D::render_highlightPiece(Chess::Position2D piecePosition) const {
    if (piecePosition.x() < 0 || piecePosition.y() < 0 || 
        piecePosition.x() >= _boardDim || piecePosition.y() >= _boardDim) {
        return;
    }
    const Rectangle square = squareRect(piecePosition);
    // Selected piece: accent tint plus a 3 px accent outline (pieces are drawn underneath)
    DrawRectangleRec(square, UI::Color::hover);
    DrawRectangleLinesEx(square, worldThickness(UI::Space::outline), UI::Color::selected);
}

void BoardView2D::render_pieces() const {
    for (const auto& [pos, pieceName] : _piecePositions) {
        Texture2D& texture = ThemeManager::getInstance().getPieceTexture(pieceName);
        const Rectangle sq = squareRect(pos);
        DrawTexturePro(
                texture,
                Rectangle{0, 0, static_cast<float>(texture.width), static_cast<float>(texture.height)},
                sq,
                Vector2{0, 0},
                0.0f,
                WHITE
        );
    }
}
