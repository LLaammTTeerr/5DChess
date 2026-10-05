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
#include "Render/Motion.h"
#include <rlgl.h>
#include <climits>

ChessView::ChessView(Vector3 worldSize)
    : _worldSize(worldSize) {
    // Initialize camera controller
    _cameraController = std::make_unique<CameraController>(worldSize);
    _cameraController->setViewportInsets(UI::Layout::safeTop, UI::Layout::sideInset, UI::Layout::safeBottom, UI::Layout::sideInset);
    // Initialize arrow renderer
    _arrowRenderer = std::make_unique<TimelineArrowRenderer>();
    // Initialize present line renderer
    _presentLineRenderer = std::make_unique<PresentLineRenderer>();
    _lift.init(0.0f, 520.0f, 0.55f);       // slightly underdamped: a small pop when a piece is picked up
    _endPop.init(0.0f, 260.0f, 0.62f);
    _knownBoards.reserve(64);
    _scratchKeys.reserve(64);
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
    const float dt = UI::Motion::safeDt(deltaTime);
    _cameraController->update(dt, _boardViews);

    // Update arrow animations using the renderer
    _arrowRenderer->update(dt);

    // Update present line animations
    _presentLineRenderer->update(dt);

    updateMotion(dt);
}

// ---------------------------------------------------------------------------------------------
// Motion
// ---------------------------------------------------------------------------------------------

void ChessView::updateMotion(float dt) {
    using namespace UI::Motion;

    for (auto& f : _flights) { f.move.update(dt); f.victimFade.update(dt); }
    _flights.erase(std::remove_if(_flights.begin(), _flights.end(),
                                  [](const Flight& f) { return f.move.done() && f.victimFade.done(); }),
                   _flights.end());
    for (auto& e : _enters) e.t.update(dt);
    _enters.erase(std::remove_if(_enters.begin(), _enters.end(), [](const BoardEnter& e) { return e.t.done(); }),
                  _enters.end());

    _lift.update(dt);
    _dotClock += dt;

    // Hover tint: the square under the pointer fades in, the one just left fades out (fast)
    const bool haveHover = _hoverPosition.first && _hoverPosition.first->getBoard();
    const float step = dt / fast;
    if (haveHover) {
        const BoardKey key = boardKeyOf(*_hoverPosition.first->getBoard());
        const Chess::Position2D pos = _hoverPosition.second;
        if (_hoverCur.pos == pos && _hoverCur.key == key) {
            _hoverCur.alpha = std::fmin(1.0f, _hoverCur.alpha + step);
        } else {
            if (_hoverCur.alpha > 0.01f) _hoverOld = _hoverCur;
            _hoverCur = {key, pos, std::fmin(1.0f, step)};
        }
    } else if (_hoverCur.alpha > 0.0f) {
        _hoverOld = _hoverCur;
        _hoverCur = HoverSlot{};
    }
    _hoverOld.alpha = std::fmax(0.0f, _hoverOld.alpha - step);
    if (reduced()) { _hoverCur.alpha = haveHover ? 1.0f : 0.0f; _hoverOld.alpha = 0.0f; }

    if (_bannerActive) {
        _bannerClock += dt;
        if (_bannerClock >= 1.2f) _bannerActive = false;
    }
    _chipWhite.update(dt);

    if (_endActive) {
        _endClock += dt;
        _endScrim.update(dt);
        _endPop.update(dt);
    }
}

BoardView* ChessView::findBoardView(BoardKey key) const {
    for (const auto& bv : _boardViews) {
        if (bv && bv->getBoard() && boardKeyOf(*bv->getBoard()) == key) return bv.get();
    }
    return nullptr;
}

void ChessView::endBoardViewSync() {
    using namespace UI::Motion;
    _scratchKeys.clear();
    for (const auto& bv : _boardViews)
        if (bv && bv->getBoard()) _scratchKeys.push_back(boardKeyOf(*bv->getBoard()));
    std::sort(_scratchKeys.begin(), _scratchKeys.end());

    if (_boardsSeeded) {
        // Boards that exist now but did not last frame were just created by a move: grow them in
        for (const auto& k : _scratchKeys) {
            if (std::binary_search(_knownBoards.begin(), _knownBoards.end(), k)) continue;
            BoardEnter e{k, {}};
            e.t.start(0.0f, 1.0f, base, easeOutCubic, 0.0f, true); // under Reduce motion: a plain fade
            if (!e.t.done()) _enters.push_back(e);
        }
    }
    _knownBoards.swap(_scratchKeys);
    _boardsSeeded = true;

    // Re-apply persistent motion state to this frame's (fresh) board views
    const bool blink = ThemeManager::getInstance().currentThemeHasBlink() && !reduced() &&
                       _cameraController->getCamera2D()->zoom >= 0.8f; // skip blinking when zoomed far out
    const bool selected = _fromPosition.first && _fromPosition.first->getBoard() && _fromPosition.second.x() >= 0;
    const BoardKey selKey = selected ? boardKeyOf(*_fromPosition.first->getBoard()) : BoardKey{0, 0};
    for (const auto& bv : _boardViews) {
        if (!bv || !bv->getBoard()) continue;
        const BoardKey key = boardKeyOf(*bv->getBoard());
        float progress = 1.0f;
        for (const auto& e : _enters) if (e.key == key) { progress = e.t.progress(); break; }
        bv->setEnterProgress(progress);
        bv->clearHiddenSquares();
        if (selected && key == selKey) bv->hideSquare(_fromPosition.second); // drawn lifted by an overlay
        for (const auto& f : _flights)
            if (f.info.dstKey == key && !f.move.done()) bv->hideSquare(f.info.dstPos);
        bv->setBlink(blink, static_cast<unsigned>((key.first + 1000) * 7919 + key.second));
    }
}

void ChessView::startMoveFlight(const MoveFlight& info) {
    using namespace UI::Motion;
    if (reduced()) return; // pieces just appear
    Flight f;
    f.info = info;
    const Rectangle srcSq = squareRectFor(boardWorldArea(info.srcKey), info.dim, info.srcPos);
    const Rectangle dstSq = squareRectFor(boardWorldArea(info.dstKey), info.dim, info.dstPos);
    f.from = {srcSq.x + srcSq.width / 2, srcSq.y + srcSq.height / 2};
    f.to = {dstSq.x + dstSq.width / 2, dstSq.y + dstSq.height / 2};
    f.size = dstSq.width;
    const bool crossBoard = !(info.srcKey == info.dstKey);
    if (crossBoard) {
        // Gentle arc across boards / through time
        const float dist = Vector2Distance(f.from, f.to);
        f.arcHeight = std::fmin(140.0f, 24.0f + dist * 0.12f);
        f.move.start(0.0f, 1.0f, slow, easeInOutCubic);
    } else {
        f.move.start(0.0f, 1.0f, base, easeOutCubic);
    }
    if (!info.victim.empty()) f.victimFade.start(0.0f, 1.0f, fast, easeInCubic);
    _flights.push_back(std::move(f));
}

void ChessView::finishAnimations() {
    _flights.clear();
    _enters.clear();
    _arrowRenderer->finishAnimations();
}

void ChessView::setEndGame(bool ended, bool whiteWon) {
    if (ended && !_endActive) {
        _endActive = true;
        _endWhiteWon = whiteWon;
        _endClock = 0.0f;
        _endScrim.start(0.0f, 1.0f, UI::Motion::base, UI::Motion::easeOutCubic, 0.0f, true);
        _endPop.init(0.0f, 260.0f, 0.62f);
        _endPop.setTarget(1.0f);
    } else if (!ended && _endActive) {
        _endActive = false;
        _endPop.snap(0.0f);
    }
}

void ChessView::updateHud(const HudData& hud) {
    const bool changed = _hudSeeded && hud.whiteToMove != _hud.whiteToMove;
    if (changed) {
        // Turn change: a slim banner drops in below the action row; the HUD chip cross-fades
        _bannerActive = true;
        _bannerWhite = hud.whiteToMove;
        _bannerClock = 0.0f;
        _chipWhite.start(_chipWhite.value(), hud.whiteToMove ? 1.0f : 0.0f, UI::Motion::fast, UI::Motion::easeOutCubic, 0.0f, true);
    } else if (!_hudSeeded) {
        _chipWhite.start(hud.whiteToMove ? 1.0f : 0.0f, hud.whiteToMove ? 1.0f : 0.0f, 0.0f);
    }
    _hudSeeded = true;
    _hud = hud;
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
    render_flights();
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
    const bool valid = fromPosition.first && fromPosition.second.x() >= 0 && fromPosition.second.y() >= 0;
    const bool changed = valid && (!_fromPosition.first || !(_fromPosition.second == fromPosition.second) ||
                                   !_fromPosition.first->getBoard() || !fromPosition.first->getBoard() ||
                                   boardKeyOf(*_fromPosition.first->getBoard()) != boardKeyOf(*fromPosition.first->getBoard()));
    _fromPosition = fromPosition;
    if (!valid) { _lift.snap(0.0f); }
    else if (changed) { _lift.snap(0.0f); _lift.setTarget(1.0f); } // pick-up: spring lifts the piece
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
    if (_cameraController->isUsing3DRendering()) return;
    if (_hoverCur.alpha <= 0.0f && _hoverOld.alpha <= 0.0f) return;
    BeginMode2D(*_cameraController->getCamera2D());
    if (_hoverOld.alpha > 0.0f) {
        if (BoardView* bv = findBoardView(_hoverOld.key)) bv->render_hoverSquare(_hoverOld.pos, _hoverOld.alpha);
    }
    if (_hoverCur.alpha > 0.0f && _hoverPosition.first) {
        _hoverPosition.first->render_hoverSquare(_hoverCur.pos, _hoverCur.alpha);
    }
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
        piecePosition.first->render_liftedPiece(piecePosition.second, _lift.value);
    } else {
        std::cerr << "Null BoardView encountered in highlighted piece!" << std::endl;
    }
    EndMode2D();
}

void ChessView::update_highlightedPositions(const std::vector<std::pair<std::shared_ptr<BoardView>, Chess::Position2D>>& positions) {
    _highlightedPositions = positions;
    // Pop in with a 30 ms stagger per ring of squares away from the picked-up piece (capped at 10 rings)
    _dotDelay.assign(positions.size(), 0.0f);
    _dotClock = 0.0f;
    if (_fromPosition.first && !positions.empty()) {
        const Rectangle fromSq = _fromPosition.first->squareWorldRect(_fromPosition.second);
        const Vector2 fc = {fromSq.x + fromSq.width / 2, fromSq.y + fromSq.height / 2};
        for (size_t i = 0; i < positions.size(); ++i) {
            if (!positions[i].first) continue;
            const Rectangle sq = positions[i].first->squareWorldRect(positions[i].second);
            const float dist = Vector2Distance(fc, {sq.x + sq.width / 2, sq.y + sq.height / 2});
            const float ring = std::fmin(10.0f, std::floor(dist / std::fmax(1.0f, fromSq.width)));
            _dotDelay[i] = ring * UI::Motion::staggerDots;
        }
    }
}


void ChessView::render_highlightedPositions() const {
    if (_cameraController->isUsing3DRendering()) {
        // 3D rendering code for highlighted positions
        return;
    }
    BeginMode2D(*_cameraController->getCamera2D());
    for (size_t i = 0; i < _highlightedPositions.size(); ++i) {
        const auto& position = _highlightedPositions[i];
        if (position.first) {
            const float delay = i < _dotDelay.size() ? _dotDelay[i] : 0.0f;
            const float scale = UI::Motion::reduced()
                ? 1.0f : UI::Motion::easeOutBack(UI::Motion::clamp01((_dotClock - delay) / UI::Motion::fast));
            position.first->render_legalTarget(position.second, scale);
        } else {
            std::cerr << "Null BoardView encountered in highlighted positions!" << std::endl;
        }
    }
    EndMode2D();
}

void ChessView::render_flights() const {
    if (_flights.empty() || _cameraController->isUsing3DRendering()) return;
    ThemeManager& themes = ThemeManager::getInstance();
    BeginMode2D(*_cameraController->getCamera2D());
    for (const auto& f : _flights) {
        // Captured piece shrinks and fades where it stood (drawn under the mover)
        if (!f.info.victim.empty() && !f.victimFade.done()) {
            const float p = f.victimFade.progress();
            Texture2D& tex = *themes.getPieceTextures(f.info.victim).open;
            const float s = f.size * UI::Motion::lerp(1.0f, 0.55f, p);
            DrawTexturePro(tex, {0, 0, (float)tex.width, (float)tex.height},
                           {f.to.x - s / 2, f.to.y - s / 2, s, s}, {0, 0}, 0.0f,
                           UI::withAlpha(WHITE, static_cast<unsigned char>(255.0f * (1.0f - p))));
        }
        if (!f.move.done()) {
            const float t = f.move.progress();
            Vector2 pos = {UI::Motion::lerp(f.from.x, f.to.x, t), UI::Motion::lerp(f.from.y, f.to.y, t)};
            float s = f.size;
            if (f.arcHeight > 0.0f) {
                pos.y -= f.arcHeight * 4.0f * t * (1.0f - t);
                s *= 1.0f + 0.18f * std::sin(t * PI);
                // ground shadow stays on the straight path
                DrawEllipse(static_cast<int>(pos.x), static_cast<int>(UI::Motion::lerp(f.from.y, f.to.y, t) + f.size * 0.4f),
                            f.size * 0.25f, f.size * 0.06f, UI::withAlpha(UI::Color::shadow, 40));
            }
            Texture2D& tex = *themes.getPieceTextures(f.info.piece).open;
            DrawTexturePro(tex, {0, 0, (float)tex.width, (float)tex.height},
                           {pos.x - s / 2, pos.y - s / 2, s, s}, {0, 0}, 0.0f, WHITE);
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
    {
        // Chip cross-fades between the two colours on a turn change
        const float m = _chipWhite.value();
        const ::Color chip = {
            static_cast<unsigned char>(UI::Motion::lerp(UI::Color::blackChip.r, UI::Color::whiteChip.r, m)),
            static_cast<unsigned char>(UI::Motion::lerp(UI::Color::blackChip.g, UI::Color::whiteChip.g, m)),
            static_cast<unsigned char>(UI::Motion::lerp(UI::Color::blackChip.b, UI::Color::whiteChip.b, m)), 255};
        DrawCircleV({x + chipD / 2, cy}, chipD / 2, chip);
    }
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

    renderTurnBanner();
}

// Slim "<Colour> to move" banner: drops in below the action row (fast), leaves faster, auto-dismisses at ~1.2 s.
void ChessView::renderTurnBanner() const {
    if (!_bannerActive) return;
    using namespace UI::Motion;
    const float outDur = exitDuration(fast);
    const float in = easeOutCubic(clamp01(_bannerClock / fast));
    const float out = easeInCubic(clamp01((_bannerClock - (1.2f - outDur)) / outDur));
    const float a = in * (1.0f - out);
    if (a <= 0.003f) return;
    const float slide = reduced() ? 0.0f : (1.0f - in) * -16.0f + out * -8.0f;

    const std::string text = _bannerWhite ? "White to move" : "Black to move";
    const ::Font font = UI::Fonts::body();
    const float tw = MeasureTextEx(font, text.c_str(), UI::Font::body, 0).x;
    const float chipD = 10.0f, pad = UI::Space::md, h = 30.0f;
    const float w = pad + chipD + UI::Space::sm + tw + pad;
    const float screenW = static_cast<float>(GetScreenWidth());
    const Rectangle r = {std::floor((screenW - w) / 2), UI::Layout::actionRowBottom + 8.0f + slide, w, h};
    auto fade = [a](::Color c) { c.a = static_cast<unsigned char>(c.a * a); return c; };
    DrawRectangleRounded({r.x + 2, r.y + 3, r.width, r.height}, 1.0f, 12, fade(UI::Color::shadow));
    DrawRectangleRounded(r, 1.0f, 12, fade(UI::Color::surface));
    DrawRectangleRoundedLinesEx(r, 1.0f, 12, 1.0f, fade(UI::Color::border));
    const Vector2 c = {r.x + pad + chipD / 2, r.y + h / 2};
    DrawCircleV(c, chipD / 2, fade(_bannerWhite ? UI::Color::whiteChip : UI::Color::blackChip));
    DrawCircleLinesV(c, chipD / 2, fade(UI::Color::text));
    DrawTextEx(font, text.c_str(), {std::floor(c.x + chipD / 2 + UI::Space::sm), std::floor(r.y + (h - UI::Font::body) / 2 - 1)},
               UI::Font::body, 0, fade(UI::Color::text));
}

void ChessView::renderEndGameScreen(std::string winnerText) const {
    using namespace UI::Motion;
    const float screenW = static_cast<float>(GetScreenWidth());
    const float screenH = static_cast<float>(GetScreenHeight());
    const float fade = _endActive ? _endScrim.progress() : 1.0f;     // scrim + text fade
    const float pop = _endActive ? _endPop.value : 1.0f;            // spring: slight overshoot
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), UI::withAlpha(UI::Color::scrim, static_cast<unsigned char>(UI::Color::scrim.a * fade)));

    const float cardW = 460.0f, cardH = 220.0f;
    Rectangle card = {std::floor((screenW - cardW) / 2), std::floor((screenH - cardH) / 2), cardW, cardH};
    const float scale = reduced() ? 1.0f : UI::Motion::lerp(0.88f, 1.0f, pop);
    auto fa = [fade](::Color c) { c.a = static_cast<unsigned char>(c.a * clamp01(fade)); return c; };

    rlPushMatrix();
    rlTranslatef(card.x + card.width / 2, card.y + card.height / 2, 0.0f);
    rlScalef(scale, scale, 1.0f);
    rlTranslatef(-(card.x + card.width / 2), -(card.y + card.height / 2), 0.0f);

    DrawRectangleRounded({card.x + 4, card.y + 6, card.width, card.height}, 0.08f, 8, fa(UI::Color::shadow));
    DrawRectangleRounded(card, 0.08f, 8, fa(UI::Color::surface));
    DrawRectangleRoundedLinesEx(card, 0.08f, 8, 1.0f, fa(UI::Color::border));

    const float cx = card.x + card.width / 2;
    UI::drawTextCentered(UI::Fonts::title(), winnerText.c_str(), cx, card.y + 34, UI::Font::title, fa(UI::Color::text));
    UI::drawTextCentered(UI::Fonts::body(), "King captured", cx, card.y + 102, UI::Font::body, fa(UI::Color::textMuted));
    DrawRectangle(static_cast<int>(card.x + 40), static_cast<int>(card.y + 146), static_cast<int>(card.width - 80), 1, fa(UI::Color::border));
    UI::drawTextCentered(UI::Fonts::body(), "Use Back to return to game selection", cx, card.y + 164, UI::Font::body, fa(UI::Color::primary));

    // Pixel theme: the winner's king hops a few times on top of the card
    if (ThemeManager::getInstance().isPixelTheme()) {
        const char* name = _endWhiteWon ? "white_king" : "black_king";
        const PieceTextures& tex = ThemeManager::getInstance().getPieceTextures(name);
        const float size = 64.0f;
        float hop = 0.0f;
        if (!reduced()) {
            float t = _endClock - 0.30f;                 // start once the card has popped
            const float period = 0.46f;
            for (int i = 0; i < 3 && t >= 0.0f; ++i, t -= period) {
                if (t < period) { const float u = t / period; hop = (26.0f - i * 8.0f) * 4.0f * u * (1.0f - u); break; }
            }
        }
        const bool closed = blinkClosed(77u, GetTime());
        Texture2D& sprite = (closed && tex.blink) ? *tex.blink : *tex.open;
        const float sx = std::floor(cx - size / 2), sy = std::floor(card.y - size + 6.0f - hop);
        DrawEllipse(static_cast<int>(cx), static_cast<int>(card.y + 1), 20.0f - hop * 0.25f, 4.0f, fa(UI::withAlpha(UI::Color::shadow, 70)));
        DrawTexturePro(sprite, {0, 0, (float)sprite.width, (float)sprite.height}, {sx, sy, size, size}, {0, 0}, 0.0f,
                       UI::withAlpha(WHITE, static_cast<unsigned char>(255.0f * clamp01(fade))));
    }
    rlPopMatrix();
}
