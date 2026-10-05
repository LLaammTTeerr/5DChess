#include "BoardView.h"
#include "View.h"
#include "chess.h"
#include "raymath.h"
#include "PieceTheme.h"
#include "Render/UITheme.h"
#include "Render/Motion.h"
#include <rlgl.h>
#include <algorithm>
#include <iostream>


const float BOARD_WORLD_SIZE = 250.0f; // Assuming a standard chess board size
const float HORIZONTAL_SPACING = 60.0f; // Size of each square on the board
const float VERTICAL_SPACING = 60.0f; // Size of each square on the board
const int STANDARD_BOARD_DIM = 8;


Rectangle boardWorldArea(BoardKey key) {
    return {static_cast<float>(key.second) * (BOARD_WORLD_SIZE + HORIZONTAL_SPACING),
            static_cast<float>(key.first) * (BOARD_WORLD_SIZE + VERTICAL_SPACING),
            BOARD_WORLD_SIZE, BOARD_WORLD_SIZE};
}

Rectangle squareRectFor(const Rectangle& area, int dim, Chess::Position2D pos) {
    const float sw = area.width / dim;
    const float sh = area.height / dim;
    return Rectangle{area.x + (dim - 1 - pos.x()) * sw, area.y + (dim - 1 - pos.y()) * sh, sw, sh};
}

namespace {
inline Color fadeColor(Color c, float a) {
    c.a = static_cast<unsigned char>(c.a * (a < 0.0f ? 0.0f : (a > 1.0f ? 1.0f : a)));
    return c;
}
}

void BoardView2D::render() const {
    if (!_boardTexture) {
        std::cerr << "Board texture not set!" << std::endl;
        return;
    }
    if (_area.width <= 0 || _area.height <= 0) {
        std::cerr << "Invalid render area dimensions!" << std::endl;
        return;
    }

    // Newly created boards grow in from 0.92x while fading (scaled about their centre; hit-testing uses _area)
    const bool entering = _enter < 0.999f;
    const float alpha = entering ? _enter : 1.0f;
    if (entering) {
        const float s = UI::Motion::lerp(0.92f, 1.0f, _enter);
        const Vector2 c = getBoardCenter();
        rlPushMatrix();
        rlTranslatef(c.x, c.y, 0.0f);
        rlScalef(s, s, 1.0f);
        rlTranslatef(-c.x, -c.y, 0.0f);
    }

    for (int x = 0; x < _boardDim; ++x) {
        for (int y = 0; y < _boardDim; ++y) {
            const Rectangle sq = squareRect(Chess::Position2D(x, y));
            // Engine (0,0) is light; after the rotation it is White's bottom-right corner
            const bool light = (x + y) % 2 == 0;
            DrawRectangle(sq.x, sq.y, sq.width, sq.height,
                          fadeColor(light ? UI::Color::squareLight : UI::Color::squareDark, alpha));
        }
    }
    render_pieces();

    // Border: accent for boards the current player may still move from, neutral otherwise
    if (_moveable) {
        DrawRectangleLinesEx(_area, worldThickness(3.0f), fadeColor(UI::withAlpha(UI::Color::accent, 170), alpha));
    } else {
        DrawRectangleLinesEx(_area, worldThickness(2.0f), fadeColor(UI::Color::border, alpha));
    }

    if (entering) rlPopMatrix();
}

// The view is the engine grid rotated 180 degrees: White (y=0) is at the BOTTOM and the
// engine's x=0 file is at the RIGHT, which puts the queen on d1 and the king on e1.
// Each mapping is its own inverse, so one helper serves both directions.
int BoardView2D::colToScreen(int x) const { return _boardDim - 1 - x; }
int BoardView2D::screenToCol(int screenCol) const { return _boardDim - 1 - screenCol; }
int BoardView2D::rowToScreen(int y) const { return _boardDim - 1 - y; }
int BoardView2D::screenToRow(int screenRow) const { return _boardDim - 1 - screenRow; }

Rectangle BoardView2D::squareRect(Chess::Position2D pos) const {
    return squareRectFor(_area, _boardDim, pos);
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

void BoardView2D::render_legalTarget(Chess::Position2D pos, float scale) const {
    if (scale <= 0.0f) return;
    const Rectangle sq = squareRect(pos);
    const Vector2 center = {sq.x + sq.width / 2, sq.y + sq.height / 2};
    const float a = scale > 1.0f ? 1.0f : scale;
    const bool occupied = pieceNameAt(pos) != nullptr;
    if (occupied) {
        // Capture: ring around the enemy piece
        DrawRing(center, sq.width * 0.40f * scale, sq.width * 0.46f * scale, 0, 360, 36, fadeColor(UI::Color::capture, a));
    } else {
        // Legal empty target: sage dot
        DrawCircleV(center, sq.width * 0.17f * scale, fadeColor(UI::withAlpha(UI::Color::legalTarget, 220), a));
    }
}

void BoardView2D::render_hoverSquare(Chess::Position2D pos, float alpha) const {
    if (pos.x() < 0 || pos.y() < 0 || pos.x() >= _boardDim || pos.y() >= _boardDim) return;
    const Rectangle sq = squareRect(pos);
    DrawRectangle(sq.x, sq.y, sq.width, sq.height, fadeColor(UI::Color::hover, alpha));
}

void BoardView2D::render_highlightPiece(Chess::Position2D piecePosition) const {
    if (piecePosition.x() < 0 || piecePosition.y() < 0 || 
        piecePosition.x() >= _boardDim || piecePosition.y() >= _boardDim) {
        return;
    }
    const Rectangle square = squareRect(piecePosition);
    // Selected piece: accent tint plus a 3 px accent outline (the piece itself is drawn lifted on top)
    DrawRectangleRec(square, UI::Color::hover);
    DrawRectangleLinesEx(square, worldThickness(UI::Space::outline), UI::Color::selected);
}

const std::string* BoardView2D::pieceNameAt(Chess::Position2D pos) const {
    for (const auto& p : _piecePositions) if (p.first == pos) return &p.second;
    return nullptr;
}

void BoardView2D::render_liftedPiece(Chess::Position2D pos, float lift) const {
    const std::string* name = pieceNameAt(pos);
    if (!name) return;
    const PieceTextures& tex = ThemeManager::getInstance().getPieceTextures(*name);
    const Rectangle sq = squareRect(pos);
    const float up = lift * sq.height * 0.09f;
    const float grow = 1.0f + 0.06f * lift;
    // Soft shadow stays on the ground while the piece rises
    DrawEllipse(static_cast<int>(sq.x + sq.width / 2), static_cast<int>(sq.y + sq.height * 0.9f),
                sq.width * (0.28f + 0.03f * lift), sq.height * 0.06f,
                fadeColor(UI::Color::shadow, 0.5f + 0.5f * (lift > 1.0f ? 1.0f : lift)));
    const Rectangle dst = {sq.x - sq.width * (grow - 1.0f) / 2, sq.y - up - sq.height * (grow - 1.0f) / 2,
                           sq.width * grow, sq.height * grow};
    DrawTexturePro(*tex.open, {0, 0, static_cast<float>(tex.open->width), static_cast<float>(tex.open->height)},
                   dst, {0, 0}, 0.0f, WHITE);
}

void BoardView2D::render_pieces() const {
    const float alpha = _enter < 0.999f ? _enter : 1.0f;
    const Color tint = fadeColor(WHITE, alpha);
    const double now = (_blinkEnabled ? GetTime() : 0.0);
    for (const auto& [pos, pieceName] : _piecePositions) {
        if (isHiddenSquare(pos)) continue;
        const PieceTextures& tex = ThemeManager::getInstance().getPieceTextures(pieceName);
        Texture2D* texture = tex.open;
        if (_blinkEnabled && tex.blink) {
            const unsigned id = _blinkSeed * 64u + static_cast<unsigned>(pos.x() * 8 + pos.y()) * 2654435761u
                                + static_cast<unsigned>(pieceName.size() * 31 + pieceName[0] * 7 + pieceName[6]);
            if (UI::Motion::blinkClosed(id, now)) texture = tex.blink;
        }
        const Rectangle sq = squareRect(pos);
        DrawTexturePro(
                *texture,
                Rectangle{0, 0, static_cast<float>(texture->width), static_cast<float>(texture->height)},
                sq,
                Vector2{0, 0},
                0.0f,
                tint
        );
    }
}
