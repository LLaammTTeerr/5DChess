#include "play/MotionOverlay.h"
#include <algorithm>
#include <cmath>
#include <rlgl.h>
#include "Render/Motion.h"
#include "Render/UITheme.h"
#include "play/Feedback.h"
#include "play/BoardScene.h"
#include "play/Hud.h"
#include "play/Paths.h"

namespace play::overlay {

namespace {

float onePx(float zoom) { return 1.0f / std::max(zoom, 0.05f); }

Vector2 squareCentre(const Chess::Core::Coord& c, int dim) {
  const Rect sq = BoardLayout::squareRect(BoardLayout::boardRect(c.l, c.t), dim, c.x, c.y);
  return {sq.centerX(), sq.centerY()};
}

// A curved path between two square centres, bowed upwards in proportion to the distance
void arcPath(path::Poly& poly, Vector2 a, Vector2 b) {
  const float dist = std::hypot(b.x - a.x, b.y - a.y);
  const float h = std::min(190.0f, 50.0f + dist * 0.16f);
  const Vector2 c0 = {a.x + (b.x - a.x) * 0.25f, a.y + (b.y - a.y) * 0.25f - h};
  const Vector2 c1 = {a.x + (b.x - a.x) * 0.75f, a.y + (b.y - a.y) * 0.75f - h};
  path::bezier(poly, a, c0, c1, b, 28);
}

void arrowHead(const path::Poly& poly, float size, Color color) {
  if (poly.n < 2) return;
  const Vector2 tip = poly.p[poly.n - 1], before = poly.p[poly.n - 2];
  const float len = std::max(1.0f, std::hypot(tip.x - before.x, tip.y - before.y));
  const Vector2 dir = {(tip.x - before.x) / len, (tip.y - before.y) / len}, side = {-dir.y, dir.x};
  const Vector2 l = {tip.x - dir.x * size + side.x * size * 0.6f, tip.y - dir.y * size + side.y * size * 0.6f};
  const Vector2 r = {tip.x - dir.x * size - side.x * size * 0.6f, tip.y - dir.y * size - side.y * size * 0.6f};
  DrawTriangle(tip, l, r, color);
  DrawTriangle(tip, r, l, color);
}

// Screen x of the middle of the half-turn column `halfTurn` (fractional columns lie between)
float columnX(const Camera2D& camera, float halfTurn) { return GetWorldToScreen2D({BoardScene::presentColumnX(halfTurn), 0.0f}, camera).x; }

} // namespace

BoardShift::BoardShift(Vector2 offset) : _active(offset.x != 0.0f || offset.y != 0.0f) {
  if (_active) {
    rlPushMatrix();
    rlTranslatef(offset.x, offset.y, 0.0f);
  }
}

BoardShift::~BoardShift() {
  if (_active) rlPopMatrix();
}

void drawLaneUnfold(const std::vector<MoveAnimator::LaneUnfold>& lanes, const MultiverseView& view, const Camera2D& camera,
                    float presentX, const std::function<void()>& paintBackground) {
  const int W = GetScreenWidth(), H = GetScreenHeight();
  const int top = static_cast<int>(UI::Layout::rulerY);
  for (const auto& lane : lanes) {
    const TimelineInfo* tl = view.timeline(lane.timeline);
    // the parent lane lies above when it has the higher id (White's timelines are above)
    const bool fromTop = tl && tl->created ? tl->parent > lane.timeline : lane.timeline < 0;
    const float centre = BoardLayout::cardRect(BoardLayout::boardRect(lane.timeline, 0)).centerY();
    const float y0 = GetWorldToScreen2D({0.0f, centre - BoardLayout::kPitch / 2.0f}, camera).y;
    const float h = BoardLayout::kPitch * camera.zoom;
    const feedback::Span visible = feedback::unfoldedSpan(y0, h, fromTop, lane.progress);
    // the hidden remainder of the lane
    const float hy0 = fromTop ? visible.y1 : y0, hy1 = fromTop ? y0 + h : visible.y0;
    const float cy0 = std::max(hy0, static_cast<float>(top)), cy1 = std::min(hy1, static_cast<float>(H));
    if (cy1 <= cy0) continue;
    // Only this lane's rows are repainted, and the present column (its glow) is left out so it survives the unfold
    const float colHalf = BoardLayout::kPitch * camera.zoom / 2.0f + 8.0f;
    const int sy = static_cast<int>(std::floor(cy0)), sh = static_cast<int>(std::ceil(cy1 - cy0)) + 1;
    const int left = std::clamp(static_cast<int>(presentX - colHalf), 0, W), right = std::clamp(static_cast<int>(presentX + colHalf), 0, W);
    if (left > 0) {
      BeginScissorMode(0, sy, left, sh);
      paintBackground();
      EndScissorMode();
    }
    if (right < W) {
      BeginScissorMode(right, sy, W - right, sh);
      paintBackground();
      EndScissorMode();
    }
  }
}

void drawBoardGlow(const Rect& board, float alpha, const BoardStyle& style, const SoftBox& soft, bool source, float zoom) {
  if (alpha <= 0.01f) return;
  const Rectangle card = toRay(BoardLayout::cardRect(board));
  if (!source) { // the live arc's target: one soft pass
    soft.draw(card, fade(style.accent, 0.5f * alpha));
    return;
  }
  // The board a move came from: a dashed outline (it does not stack with the glows)
  const float px = onePx(zoom), pad = 7.0f;
  path::Poly poly;
  const Rectangle r = {card.x - pad, card.y - pad, card.width + 2 * pad, card.height + 2 * pad};
  poly.add({r.x, r.y});
  poly.add({r.x + r.width, r.y});
  poly.add({r.x + r.width, r.y + r.height});
  poly.add({r.x, r.y + r.height});
  poly.add({r.x, r.y});
  path::stroke(poly, 1.0f, fade(style.accent2, alpha), 2.5f * px, 10.0f * px, 7.0f * px);
}

void drawRejectFlash(const Rect& square, float alpha) {
  if (alpha <= 0.0f) return;
  DrawRectangleRec(toRay(square), fade(UI::Color::capture, alpha));
}

void drawMovePreview(const MoveAnimator::PreviewLayer& layer, const Chess::IGame& game, const BoardStyle& style, float zoom, bool arcs) {
  if (!layer.valid || layer.alpha <= 0.0f || layer.targets.empty()) return;
  const int dim = game.dim();
  const float px = onePx(zoom);
  const float a = std::min(1.0f, layer.alpha);
  std::vector<std::pair<int, int>> arcBoards; // boards already given an arc
  const Vector2 from = squareCentre(layer.from, dim);
  for (const auto& t : layer.targets) {
    if (!game.boardExists(t)) continue;
    const Rect sq = BoardLayout::squareRect(BoardLayout::boardRect(t.l, t.t), dim, t.x, t.y);
    const Vector2 c = {sq.centerX(), sq.centerY()};
    const bool occupied = game.board(t.l, t.t).at(Chess::Position2D(t.x, t.y)).has_value();
    if (occupied) DrawRing(c, sq.w * 0.40f, sq.w * 0.40f + 1.8f * px, 0, 360, 32, fade(style.check, feedback::kPreviewAlpha * a));
    else DrawRing(c, sq.w * 0.13f, sq.w * 0.13f + 2.0f * px, 0, 360, 24, fade(style.targetDot, std::min(1.0f, 2.0f * feedback::kPreviewAlpha) * a));
    const bool other = t.l != layer.from.l || t.t != layer.from.t;
    if (arcs && other && arcBoards.size() < 4 && std::find(arcBoards.begin(), arcBoards.end(), std::pair<int, int>{t.l, t.t}) == arcBoards.end()) {
      arcBoards.emplace_back(t.l, t.t);
      path::Poly poly;
      arcPath(poly, from, c);
      path::stroke(poly, 1.0f, fade(style.accent, feedback::kPreviewAlpha * a), 1.4f * px, 7.0f * px, 6.0f * px);
    }
  }
}

void drawLiveArc(const MoveAnimator::LiveArc& arc, int dim, const BoardStyle& style, float zoom) {
  const float px = onePx(zoom);
  path::Poly poly;
  arcPath(poly, squareCentre(arc.from, dim), squareCentre(arc.to, dim));
  path::stroke(poly, arc.grow, fade(style.accent, 0.28f), 7.0f * px);
  path::stroke(poly, arc.grow, style.accent, 2.4f * px, 9.0f * px, 6.0f * px);
  if (arc.grow > 0.97f) arrowHead(poly, 11.0f * px, style.accent);
}

void drawCheckPulse(const std::vector<Chess::Core::Coord>& kings, float pulse, int dim, const BoardStyle& style, float zoom) {
  if (pulse <= 0.0f) return;
  const float px = onePx(zoom);
  for (const auto& k : kings) {
    const Rect sq = BoardLayout::squareRect(BoardLayout::boardRect(k.l, k.t), dim, k.x, k.y);
    DrawRectangleRec(toRay(sq), fade(UI::Color::capture, 0.55f * pulse));
    const float r = sq.w * (0.55f + 0.5f * (1.0f - pulse));
    DrawRing({sq.centerX(), sq.centerY()}, r, r + 2.5f * px, 0, 360, 32, fade(style.check, 0.9f * pulse));
  }
}

void drawTargetRuler(const Chess::Core::Coord& target, float alpha, const Camera2D& camera, const BoardStyle& style) {
  if (alpha <= 0.01f) return;
  const float pitch = BoardLayout::kPitch * camera.zoom;
  const float x = columnX(camera, static_cast<float>(target.t));
  // the column in the ruler row
  const Rectangle col = {x - std::min(pitch / 2.0f, 40.0f), UI::Layout::rulerY, std::min(pitch, 80.0f), UI::Layout::rulerH};
  BeginScissorMode(static_cast<int>(UI::Layout::laneLabelW), static_cast<int>(UI::Layout::rulerY), GetScreenWidth(), static_cast<int>(UI::Layout::rulerH));
  drawRoundedRect(col, 8.0f, fade(style.accent, 0.28f * alpha));
  drawRoundedLines(col, 8.0f, 1.5f, fade(style.accent, 0.9f * alpha));
  EndScissorMode();
  // its lane in the label column
  const float centre = BoardLayout::cardRect(BoardLayout::boardRect(target.l, 0)).centerY();
  const float y = GetWorldToScreen2D({0.0f, centre}, camera).y;
  const float h = std::min(34.0f, BoardLayout::kPitch * camera.zoom - 4.0f);
  if (y < UI::Layout::safeTop || y > static_cast<float>(GetScreenHeight())) return;
  const Rectangle lab = {6.0f, y - h / 2.0f, UI::Layout::laneLabelW - 12.0f, h};
  drawRoundedRect(lab, 10.0f, fade(style.accent, 0.22f * alpha));
  drawRoundedLines(lab, 10.0f, 1.5f, fade(style.accent, 0.9f * alpha));
}

void drawHintAlert(const std::string& text, float alpha, const BoardStyle& style) {
  if (alpha <= 0.01f || text.empty()) return;
  const Font font = UI::Fonts::body();
  const Vector2 size = MeasureTextEx(font, text.c_str(), UI::Font::body, 0.0f);
  const float padX = 18.0f, h = 34.0f;
  const float w = size.x + 2 * padX;
  const float lift = UI::Motion::reduced() ? 0.0f : (1.0f - alpha) * 6.0f;
  const Rectangle r = {std::floor((GetScreenWidth() - w) / 2.0f), UI::Layout::rulerY + UI::Layout::rulerH + 4.0f - lift, w, h};
  drawPanel(r, style, alpha);
  DrawTextEx(font, text.c_str(), {std::floor(r.x + padX), std::floor(r.y + (h - size.y) / 2.0f)}, UI::Font::body, 0.0f,
             fade(UI::Color::capture, alpha));
}

} // namespace play::overlay
