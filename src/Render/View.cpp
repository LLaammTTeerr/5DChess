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
#include "Render/UITheme.h"

ChessView::ChessView(Vector3 worldSize)
    : _worldSize(worldSize) {
    // Initialize camera controller
    _cameraController = std::make_unique<CameraController>(worldSize);
    _cameraController->setViewportInsets(UI::Layout::safeTop, UI::Layout::sideInset, UI::Layout::safeBottom, UI::Layout::sideInset);
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
        _hoverPosition = {hoveredBoardView, hoveredPosition};
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

void ChessView::handleInput(bool pointerBlocked) {
    /// @brief Handle input for camera movement and zoom
    update(GetFrameTime());

    /// @brief Handle mouse clicks: selected board and selected position
    _hoverPosition = {nullptr, {-1, -1}};
    if (!pointerBlocked) {
        handleMouseSelection();
        handleMouseOver();
    }
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
    render_hoverSquare();
    render_highlightedPositions();
    render_highlightPiece(_fromPosition);

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

void ChessView::render_hoverSquare() const {
    if (_hoverPosition.first == nullptr || _cameraController->isUsing3DRendering()) return;
    BeginMode2D(*_cameraController->getCamera2D());
    _hoverPosition.first->render_hoverSquare(_hoverPosition.second);
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


void ChessView::renderHud() const {
    const float screenW = static_cast<float>(GetScreenWidth());
    const float screenH = static_cast<float>(GetScreenHeight());

    // ---- Top-centre status pill: [chip] "White to move" | "Turn N · K timelines" | hint ----
    const ::Font statusFont = UI::Fonts::button();
    const ::Font monoFont = UI::Fonts::mono();
    const ::Font bodyFont = UI::Fonts::body();
    const std::string status = _hud.whiteToMove ? "White to move" : "Black to move";
    const std::string turnInfo = "Turn " + std::to_string(_hud.fullTurn) + " \xC2\xB7 " + std::to_string(_hud.timelineCount) +
                              (_hud.timelineCount == 1 ? " timeline" : " timelines");

    const float pad = UI::Space::md;
    const float chipD = 16.0f;
    const float h = UI::Space::buttonHeight;
    const float statusW = MeasureTextEx(statusFont, status.c_str(), UI::Font::button, 0).x;
    const float infoW = MeasureTextEx(monoFont, turnInfo.c_str(), UI::Font::mono, 0).x;
    const float hintW = _hud.hint.empty() ? 0.0f : MeasureTextEx(bodyFont, _hud.hint.c_str(), UI::Font::body, 0).x;
    const float gap = UI::Space::md;
    float w = pad + chipD + UI::Space::sm + statusW + gap + 1 + gap + infoW + pad;
    if (hintW > 0) w += gap + 1 + gap + hintW;

    Rectangle pill = {std::floor((screenW - w) / 2), UI::Layout::hudPillY, w, h};
    DrawRectangleRounded({pill.x + 2, pill.y + 3, pill.width, pill.height}, 1.0f, 12, UI::Color::shadow);
    DrawRectangleRounded(pill, 1.0f, 12, UI::Color::surface);
    DrawRectangleRoundedLinesEx(pill, 1.0f, 12, 1.0f, UI::Color::border);

    float x = pill.x + pad;
    const float cy = pill.y + h / 2;
    DrawCircleV({x + chipD / 2, cy}, chipD / 2, _hud.whiteToMove ? UI::Color::whiteChip : UI::Color::blackChip);
    DrawCircleLinesV({x + chipD / 2, cy}, chipD / 2, UI::Color::text);
    x += chipD + UI::Space::sm;
    DrawTextEx(statusFont, status.c_str(), {std::floor(x), std::floor(cy - UI::Font::button / 2.0f - 1)}, UI::Font::button, 0, UI::Color::text);
    x += statusW + gap;
    DrawRectangle(static_cast<int>(x), static_cast<int>(pill.y + 10), 1, static_cast<int>(h - 20), UI::Color::border);
    x += 1 + gap;
    DrawTextEx(monoFont, turnInfo.c_str(), {std::floor(x), std::floor(cy - UI::Font::mono / 2.0f - 1)}, UI::Font::mono, 0, UI::Color::textMuted);
    x += infoW;
    if (hintW > 0) {
        x += gap;
        DrawRectangle(static_cast<int>(x), static_cast<int>(pill.y + 10), 1, static_cast<int>(h - 20), UI::Color::border);
        x += 1 + gap;
        DrawTextEx(bodyFont, _hud.hint.c_str(), {std::floor(x), std::floor(cy - UI::Font::body / 2.0f - 1)}, UI::Font::body, 0, UI::Color::primary);
    }

    // ---- Bottom controls hint bar (only real controls: see CameraController / SceneManager) ----
    const char* controls = "Click: select  \xC2\xB7  Drag/Wheel: pan/zoom  \xC2\xB7  Z: auto-zoom  \xC2\xB7  X: fit  \xC2\xB7  Esc: menu";
    const float cw = MeasureTextEx(monoFont, controls, UI::Font::mono, 0).x;
    Rectangle bar = {std::floor((screenW - (cw + 2 * pad)) / 2), screenH - UI::Layout::controlsBarMargin - UI::Layout::controlsBarH, cw + 2 * pad, UI::Layout::controlsBarH};
    DrawRectangleRounded(bar, 1.0f, 12, UI::withAlpha(UI::Color::surface, 235));
    DrawRectangleRoundedLinesEx(bar, 1.0f, 12, 1.0f, UI::Color::border);
    DrawTextEx(monoFont, controls, {bar.x + pad, std::floor(bar.y + (bar.height - UI::Font::mono) / 2 - 1)}, UI::Font::mono, 0, UI::Color::textMuted);
}

void ChessView::renderEndGameScreen(std::string winnerText) const {
    const float screenW = static_cast<float>(GetScreenWidth());
    const float screenH = static_cast<float>(GetScreenHeight());
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), UI::Color::scrim);

    const float cardW = 460.0f, cardH = 220.0f;
    Rectangle card = {std::floor((screenW - cardW) / 2), std::floor((screenH - cardH) / 2), cardW, cardH};
    UI::drawCard(card);

    const float cx = card.x + card.width / 2;
    UI::drawTextCentered(UI::Fonts::title(), winnerText.c_str(), cx, card.y + 34, UI::Font::title, UI::Color::text);
    UI::drawTextCentered(UI::Fonts::body(), "King captured", cx, card.y + 102, UI::Font::body, UI::Color::textMuted);
    DrawRectangle(static_cast<int>(card.x + 40), static_cast<int>(card.y + 146), static_cast<int>(card.width - 80), 1, UI::Color::border);
    UI::drawTextCentered(UI::Fonts::body(), "Use Back to return to game selection", cx, card.y + 164, UI::Font::body, UI::Color::primary);
}
