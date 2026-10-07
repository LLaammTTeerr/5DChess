#include "play/BoardScene.h"
#include <algorithm>
#include <cmath>
#include <string>
#include <rlgl.h>
#include "App.h"
#include "Input.h"
#include "Render/Motion.h"
#include "Render/PieceTheme.h"
#include "Render/UITheme.h"
#include "play/Paths.h"
#include "ui/Audit.h"
#include "ui/TextFit.h"
#include "ui/Widgets.h"

namespace play {

namespace {

constexpr const char* kDot = "\xC2\xB7"; // middle dot

Font fontOf(const char* id, int size) { return App::current().assets.font(id, size); }

float textWidth(Font f, const std::string& s, float size, float spacing = 0.0f) { return MeasureTextEx(f, s.c_str(), size, spacing).x; }

void drawText(Font f, const std::string& s, float x, float y, float size, Color c, float spacing = 0.0f) {
  DrawTextEx(f, s.c_str(), {std::floor(x), std::floor(y)}, size, spacing, c);
}

void drawTextCentered(Font f, const std::string& s, float cx, float y, float size, Color c, float spacing = 0.0f) {
  drawText(f, s, cx - textWidth(f, s, size, spacing) / 2.0f, y, size, c, spacing);
}

float onePx(float zoom) { return 1.0f / std::max(zoom, 0.05f); }

float hash01(unsigned a) {
  a ^= a >> 16; a *= 0x7feb352dU; a ^= a >> 15; a *= 0x846ca68bU; a ^= a >> 16;
  return static_cast<float>(a & 0xFFFFFF) / 16777216.0f;
}

Color mix(Color a, Color b, float t) {
  auto ch = [t](unsigned char x, unsigned char y) { return static_cast<unsigned char>(x + (y - x) * t); };
  return {ch(a.r, b.r), ch(a.g, b.g), ch(a.b, b.b), ch(a.a, b.a)};
}

// World y of the middle of a timeline's lane (the card's middle) and the lane's height
float laneCenterY(int timeline) {
  const Rect card = BoardLayout::cardRect(BoardLayout::boardRect(timeline, 0));
  return card.centerY();
}

Rect cardOf(int timeline, int halfTurn) { return BoardLayout::cardRect(BoardLayout::boardRect(timeline, halfTurn)); }

int laneColor(int timeline) { return ((timeline % 4) + 4) % 4; }

void triangleBoth(Vector2 a, Vector2 b, Vector2 c, Color color) {
  DrawTriangle(a, b, c, color);
  DrawTriangle(a, c, b, color);
}

} // namespace

float BoardScene::presentColumnX(float halfTurn) {
  const float x0 = BoardLayout::boardRect(0, 0).centerX();
  return x0 + halfTurn * BoardLayout::kPitch;
}

void BoardScene::queueTip(Rectangle anchor, std::string text) const {
  if (!ui::pointerConsumed() && ui::hovered(anchor)) _tip = PendingTip{anchor, std::move(text)};
}

std::string timelineLabel(int id) { return id > 0 ? "L+" + std::to_string(id) : "L" + std::to_string(id); }

std::string boardLabel(int halfTurn) { return "T" + std::to_string(halfTurn / 2 + 1) + (halfTurn % 2 == 0 ? "w" : "b"); }

// ---------------------------------------------------------------------------------------------------------------------

BoardScene::BoardScene() {
  for (size_t i = 0; i < _stars.size(); ++i) {
    const unsigned s = static_cast<unsigned>(i) * 4u + 7u;
    const float r = hash01(s + 3);
    _stars[i] = {hash01(s), hash01(s + 1), 0.35f + hash01(s + 2) * r * 1.5f, 0.2f + hash01(s + 4) * 0.7f,
                 hash01(s + 5) * 6.28f, 0.5f + hash01(s + 6) * 1.6f};
  }
}

BoardScene::~BoardScene() {
  if (_background.id != 0) UnloadTexture(_background);
  if (_aurora.id != 0) UnloadTexture(_aurora);
}

// ---- Background -----------------------------------------------------------------------------------------------

void BoardScene::bakeBackground(const BoardStyle& style, int w, int h) {
  if (_background.id != 0) UnloadTexture(_background);
  _background = {};
  Image image{};
  if (style.background == BoardStyle::Background::Paper) {
    image = GenImageColor(w, h, style.bg0);
    for (int y = 14; y < h; y += 26)
      for (int x = 14; x < w; x += 26) ImageDrawRectangle(&image, x, y, 2, 2, style.grid);
  } else if (style.background == BoardStyle::Background::Space) {
    // Half resolution is plenty for smooth gradients; the quad is stretched on draw
    const int iw = w / 2, ih = h / 2;
    image = GenImageColor(iw, ih, BLACK);
    Color* px = static_cast<Color*>(image.data);
    for (int y = 0; y < ih; ++y) {
      for (int x = 0; x < iw; ++x) {
        const float u = (x + 0.5f) / iw * 1400.0f, v = (y + 0.5f) / ih * 800.0f; // the mockup's 1400 x 800 space
        // Base: radial indigo -> navy -> night
        const float d = std::hypot(u - 700.0f, v - 380.0f);
        const float t = std::clamp((d - 60.0f) / 990.0f, 0.0f, 1.0f);
        Color c = t < 0.5f ? mix(style.bg0, style.bg1, t * 2.0f) : mix(style.bg1, style.bg2, (t - 0.5f) * 2.0f);
        float r = c.r, g = c.g, b = c.b;
        // Nebulae: violet top right, cyan bottom left
        const float n1 = std::max(0.0f, 1.0f - std::hypot(u - 1120.0f, v - 120.0f) / 520.0f) * 0.20f;
        const float n2 = std::max(0.0f, 1.0f - std::hypot(u - 220.0f, v - 720.0f) / 460.0f) * 0.12f;
        r += (140 - r) * n1 + (50 - r) * n2;
        g += (90 - g) * n1 + (200 - g) * n2;
        b += (255 - b) * n1 + (230 - b) * n2;
        // Vignette
        const float dv = std::hypot(u - 700.0f, v - 400.0f);
        const float vt = std::clamp((dv - 304.0f) / 564.0f, 0.0f, 1.0f) * 0.62f;
        r += (2 - r) * vt; g += (3 - g) * vt; b += (18 - b) * vt;
        const float dither = hash01(static_cast<unsigned>(y * iw + x)) - 0.5f; // hides banding in the dark gradient
        auto q = [dither](float value) { return static_cast<unsigned char>(std::clamp(value + dither, 0.0f, 255.0f)); };
        px[y * iw + x] = {q(r), q(g), q(b), 255};
      }
    }
  } else {
    image = GenImageColor(8, 8, style.bg0);
  }
  _background = LoadTextureFromImage(image);
  UnloadImage(image);
  SetTextureFilter(_background, TEXTURE_FILTER_BILINEAR);
  _backgroundW = w;
  _backgroundH = h;
  _backgroundView = style.id;
}

void BoardScene::drawStars(int w, int h) const {
  const bool still = UI::Motion::reduced();
  const float t = still ? 0.0f : static_cast<float>(std::fmod(Input::time(), 3600.0));
  for (const Star& s : _stars) {
    const float a = still ? s.alpha : s.alpha * (0.72f + 0.28f * std::sin(t * s.speed + s.phase));
    if (a < 0.08f) continue;
    const float side = s.size > 1.1f ? 2.0f : 1.0f;
    const Color c = s.size > 1.1f ? Color{223, 230, 255, 255} : Color{170, 180, 255, 255};
    DrawRectangleRec({std::floor(s.x * w), std::floor(s.y * h), side, side}, fade(c, std::min(1.0f, a)));
  }
}

void BoardScene::drawBackground(const BoardStyle& style) {
  const int w = GetScreenWidth(), h = GetScreenHeight();
  if (_background.id == 0 || _backgroundW != w || _backgroundH != h || _backgroundView != style.id) bakeBackground(style, w, h);
  DrawTexturePro(_background, {0, 0, static_cast<float>(_background.width), static_cast<float>(_background.height)},
                 {0, 0, static_cast<float>(w), static_cast<float>(h)}, {0, 0}, 0.0f, WHITE);
  if (style.background == BoardStyle::Background::Space) drawStars(w, h);
}

void BoardScene::bakeAurora() {
  const int w = 150, h = 400;
  Image image = GenImageColor(w, h, BLANK);
  Color* px = static_cast<Color*>(image.data);
  for (int y = 0; y < h; ++y) {
    const float v = static_cast<float>(y) / h;
    // teal -> violet -> magenta down the column, brightest just below the top
    float r, g, b, a;
    if (v < 0.14f) { const float k = v / 0.14f; r = 56; g = 240; b = 200; a = 0.62f * k; }
    else if (v < 0.55f) { const float k = (v - 0.14f) / 0.41f; r = 56 + 54 * k; g = 240 - 136 * k; b = 200 + 55 * k; a = 0.62f - 0.07f * k; }
    else { const float k = (v - 0.55f) / 0.45f; r = 110 + 90 * k; g = 104 - 24 * k; b = 255; a = 0.55f - 0.23f * k; }
    for (int x = 0; x < w; ++x) {
      const float hx = static_cast<float>(x) / (w - 1); // soft sides: 0 at the edges, 1 in the middle
      const float side = hx < 0.28f ? hx / 0.28f * 0.75f : hx < 0.5f ? 0.75f + (hx - 0.28f) / 0.22f * 0.25f
                         : hx < 0.72f ? 1.0f - (hx - 0.5f) / 0.22f * 0.25f : 0.75f * (1.0f - (hx - 0.72f) / 0.28f);
      const float streak = 1.0f + 0.12f * std::sin(x * 0.9f + 1.7f * std::sin(x * 0.21f)); // faint vertical streaks
      px[y * w + x] = {static_cast<unsigned char>(r), static_cast<unsigned char>(g), static_cast<unsigned char>(b),
                       static_cast<unsigned char>(std::clamp(a * side * streak, 0.0f, 1.0f) * 255.0f)};
    }
  }
  _aurora = LoadTextureFromImage(image);
  UnloadImage(image);
  SetTextureFilter(_aurora, TEXTURE_FILTER_BILINEAR);
}

// ---- Lanes and the present --------------------------------------------------------------------------------------

void BoardScene::drawPresentColumn(const SceneFrame& f, float top) {
  const BoardStyle& st = f.style;
  const float zoom = f.zoom();
  const float cx = f.toScreen({presentColumnX(presentColumn(f)), 0}).x;
  const float halfW = (BoardLayout::kPitch / 2.0f) * zoom;
  const float bottom = static_cast<float>(GetScreenHeight()) - 6.0f;
  if (cx + halfW < 0 || cx - halfW > GetScreenWidth()) return;

  switch (st.present) {
    case BoardStyle::Present::Glow: {
      const Rectangle r = {cx - halfW, top - 6, 2 * halfW, bottom - top + 6};
      _soft.draw(r, fade(st.accent, 0.14f));
      drawRoundedRect(r, std::min(28.0f, halfW), fade({226, 128, 86, 255}, 0.11f));
      drawRoundedLines(r, std::min(28.0f, halfW), 1.5f, fade(st.accent, 0.30f));
      break;
    }
    case BoardStyle::Present::Aurora: {
      if (_aurora.id == 0) bakeAurora();
      const float w = std::clamp(2.0f * halfW * 1.9f, 150.0f, 520.0f);
      BeginBlendMode(BLEND_ADDITIVE);
      DrawTexturePro(_aurora, {0, 0, static_cast<float>(_aurora.width), static_cast<float>(_aurora.height)},
                     {cx - w / 2, top - 8, w, bottom - top + 8}, {0, 0}, 0.0f, WHITE);
      EndBlendMode();
      break;
    }
    case BoardStyle::Present::Rule: {
      const float x0 = std::round(cx - halfW), x1 = std::round(cx + halfW);
      DrawRectangle(static_cast<int>(x0 - 1), static_cast<int>(top), 2, static_cast<int>(bottom - top), st.accent);
      DrawRectangle(static_cast<int>(x1 - 1), static_cast<int>(top), 2, static_cast<int>(bottom - top), st.accent);
      DrawRectangle(static_cast<int>(x0 - 1), static_cast<int>(top), static_cast<int>(x1 - x0 + 2), 3, st.accent);
      break;
    }
  }
}

void BoardScene::drawLanes(const SceneFrame& f, float top) {
  const BoardStyle& st = f.style;
  const float zoom = f.zoom();
  const int W = GetScreenWidth(), H = GetScreenHeight();
  BeginScissorMode(0, static_cast<int>(top), W, H - static_cast<int>(top));

  if (st.lane == BoardStyle::Lane::Hairline) {
    // Faint column guides, one per half-turn
    for (int c = f.view.firstHalfTurn; c <= f.view.lastHalfTurn; ++c) {
      const float x = std::round(f.toScreen({BoardLayout::boardRect(0, c).centerX(), 0}).x);
      if (x < 0 || x > W) continue;
      DrawRectangle(static_cast<int>(x), static_cast<int>(top), 1, H - static_cast<int>(top), st.grid);
    }
  }

  // The lanes themselves never run up under the ruler: they start at the boards' area
  const float laneTop = std::max(top, f.safe.y - 6.0f);
  EndScissorMode();
  BeginScissorMode(0, static_cast<int>(laneTop), W, H - static_cast<int>(laneTop));
  if (!f.view.timelines.empty() && st.lane != BoardStyle::Lane::Thread) {
    const int lo = f.view.timelines.front().id, hi = f.view.timelines.back().id;
    for (int l = lo; l <= hi; ++l) {
      const TimelineInfo* tl = f.view.timeline(l);
      const float y0 = f.toScreen({0, laneCenterY(l) - BoardLayout::kPitch / 2.0f}).y;
      const float h = BoardLayout::kPitch * zoom;
      if (y0 + h < laneTop || y0 > H) continue;
      if (st.lane == BoardStyle::Lane::Band) {
        const Color tint = st.laneTint[laneColor(l)];
        const Rectangle r = {8.0f, y0 + 2.0f, W - 16.0f, h - 4.0f};
        drawRoundedRect(r, std::min(20.0f, h / 4.0f), fade(tint, tl && !tl->active ? 0.45f : 1.0f));
      } else {
        const Color hair = fade(st.muted, 0.45f);
        DrawRectangle(8, static_cast<int>(std::round(y0)), W - 16, 1, hair);
        if (l == lo) DrawRectangle(8, static_cast<int>(std::round(y0 + h)), W - 16, 1, hair);
      }
    }
  }

  EndScissorMode();
  BeginScissorMode(0, static_cast<int>(top), W, H - static_cast<int>(top));
  drawPresentColumn(f, top);
  EndScissorMode();
}

// ---- World-space decoration ---------------------------------------------------------------------------------------

void BoardScene::drawTails(const SceneFrame& f) const {
  const BoardStyle& st = f.style;
  const float px = onePx(f.zoom());
  for (const TimelineInfo& tl : f.view.timelines) {
    if (!tl.active) continue;
    const Rect card = cardOf(tl.id, tl.lastHalfTurn);
    const Vector2 a = {card.x + card.w, BoardLayout::boardRect(tl.id, tl.lastHalfTurn).centerY()};
    const float len = BoardLayout::kPitch * 0.5f;
    path::Poly poly;
    poly.add(a);
    poly.add({a.x + len, a.y});
    if (st.connector == BoardStyle::Connector::Elbow) {
      path::stroke(poly, 1.0f, st.ink, 1.5f * px, 2.0f * px, 5.0f * px);
    } else {
      constexpr int steps = 6; // fades out to the right
      for (int i = 0; i < steps; ++i) {
        path::Poly piece;
        piece.add({a.x + len * i / steps, a.y});
        piece.add({a.x + len * (i + 1) / steps, a.y});
        const float fadeOut = 1.0f - static_cast<float>(i) / steps;
        path::stroke(piece, 1.0f, fade(st.thread, fadeOut * 0.9f), (st.connector == BoardStyle::Connector::Luminous ? 2.0f : 2.5f) * px);
      }
    }
  }
}

namespace {

// Rounded corners through the corners of a polyline (a corner's radius is cut to half of each run it joins)
void roundedRoute(path::Poly& poly, const Vector2* pts, int n, float radius) {
  poly.n = 0;
  poly.add(pts[0]);
  for (int i = 1; i + 1 < n; ++i) {
    const Vector2 a = pts[i - 1], b = pts[i], c = pts[i + 1];
    const float la = std::hypot(b.x - a.x, b.y - a.y), lc = std::hypot(c.x - b.x, c.y - b.y);
    const float r = std::min({radius, la / 2.0f, lc / 2.0f});
    if (r < 0.5f) { poly.add(b); continue; }
    const Vector2 p0 = {b.x + (a.x - b.x) / la * r, b.y + (a.y - b.y) / la * r};
    const Vector2 p1 = {b.x + (c.x - b.x) / lc * r, b.y + (c.y - b.y) / lc * r};
    for (int k = 0; k <= 5; ++k) {
      const float t = k / 5.0f, u = 1.0f - t;
      poly.add({u * u * p0.x + 2 * u * t * b.x + t * t * p1.x, u * u * p0.y + 2 * u * t * b.y + t * t * p1.y});
    }
  }
  poly.add(pts[n - 1]);
}

// The line of a jump: from the bottom centre of the source card down into the gap under its lane, along that gap and down (or up) the
// column gap next to the source to the gap under the target's lane, then along it and up into the bottom centre of the target card, so it
// runs between the boards instead of across them. Jumps within one column leave through the gap to the right of the cards instead.
void jumpPath(path::Poly& poly, const BoardStyle& style, Vector2 s, Vector2 d) {
  const bool sameColumn = std::fabs(d.x - s.x) < 100.0f;
  if (sameColumn) {
    const float gx = BoardLayout::kBoardSize / 2.0f + BoardLayout::kCardPad + BoardLayout::kSpacing / 2.0f; // gap centre
    s = {s.x + gx - 30.0f, s.y - 34.0f};
    d = {d.x + gx - 30.0f, d.y - 34.0f};
    const float bulge = 64.0f;
    if (style.connector == BoardStyle::Connector::Elbow) {
      poly.n = 0;
      poly.add(s);
      poly.add({s.x + bulge, s.y});
      poly.add({s.x + bulge, d.y});
      poly.add(d);
    } else {
      path::bezier(poly, s, {s.x + bulge, s.y + (d.y > s.y ? 30.0f : -30.0f)}, {d.x + bulge, d.y - (d.y > s.y ? 30.0f : -30.0f)}, d);
    }
    return;
  }
  const float rowGap = BoardLayout::kPitch - (BoardLayout::kBoardSize + 2 * BoardLayout::kCardPad + BoardLayout::kCardFooter);
  const float ys = s.y + rowGap / 2.0f, yd = d.y + rowGap / 2.0f;
  const float dir = d.x >= s.x ? 1.0f : -1.0f;
  Vector2 pts[6];
  int n = 0;
  pts[n++] = s;
  pts[n++] = {s.x, ys};
  if (std::fabs(yd - ys) > 1.0f) {
    const float xv = s.x + dir * BoardLayout::kPitch / 2.0f; // the column gap beside the source board
    pts[n++] = {xv, ys};
    pts[n++] = {xv, yd};
  }
  pts[n++] = {d.x, yd};
  pts[n++] = d;
  roundedRoute(poly, pts, n, style.connector == BoardStyle::Connector::Elbow ? 5.0f : 9.0f);
}

constexpr float kBadgeRadius = 21.0f; // fits the column gap between two cards (44 world units)

struct BadgeSpot {
  Vector2 at;
  bool clear; // the badge touches no card
};

// Where the badge of a jump goes: the point of its line nearest the middle that no card lies under (a badge over a board hides the
// pieces the player is reading); if there is none, the middle (and the caller drops the text).
BadgeSpot badgeSpot(const MultiverseView& view, const path::Poly& poly) {
  static constexpr float tries[] = {0.5f, 0.44f, 0.56f, 0.38f, 0.62f, 0.32f, 0.68f, 0.26f, 0.74f, 0.2f, 0.8f};
  auto clearAt = [&](Vector2 m) {
    for (const BoardInfo& b : view.boards) {
      const Rect c = BoardLayout::cardRect(BoardLayout::boardRect(b.timeline, b.halfTurn));
      const float nx = std::clamp(m.x, c.x, c.x + c.w), ny = std::clamp(m.y, c.y, c.y + c.h);
      if (std::hypot(m.x - nx, m.y - ny) < kBadgeRadius - 0.5f) return false;
    }
    return true;
  };
  for (const float t : tries) {
    const Vector2 m = poly.at(t);
    if (clearAt(m)) return {m, true};
  }
  return {poly.at(0.5f), false};
}

Vector2 cardBottom(int timeline, int halfTurn) {
  const Rect c = cardOf(timeline, halfTurn);
  return {c.centerX(), c.y + c.h};
}

} // namespace

std::optional<Rect> BoardScene::jumpBounds(const BoardStyle& style, const MultiverseView& view) {
  if (view.jumps.empty()) return std::nullopt;
  float x0 = 1e9f, y0 = 1e9f, x1 = -1e9f, y1 = -1e9f;
  for (const JumpInfo& jump : view.jumps) {
    path::Poly poly;
    jumpPath(poly, style, cardBottom(jump.move.from.l, jump.move.from.t), cardBottom(jump.move.to.l, jump.move.to.t));
    for (int i = 0; i < poly.n; ++i) {
      x0 = std::min(x0, poly.p[i].x); x1 = std::max(x1, poly.p[i].x);
      y0 = std::min(y0, poly.p[i].y); y1 = std::max(y1, poly.p[i].y);
    }
  }
  constexpr float badge = 28.0f, label = 26.0f; // the badge round the midpoint and the label under it
  return Rect{x0 - badge, y0 - badge, x1 - x0 + 2 * badge, y1 - y0 + 2 * badge + label};
}

void BoardScene::drawJumpArcs(const SceneFrame& f) const {
  const BoardStyle& st = f.style;
  const float px = onePx(f.zoom());
  for (const JumpInfo& jump : f.view.jumps) {
    const float alpha = jump.pending ? 1.0f : 0.65f;
    const Vector2 s = cardBottom(jump.move.from.l, jump.move.from.t), d = cardBottom(jump.move.to.l, jump.move.to.t);
    path::Poly poly;
    jumpPath(poly, st, s, d);
    float headSize = 12.0f * px;
    switch (st.connector) {
      case BoardStyle::Connector::Curve:
        path::stroke(poly, 1.0f, fade(st.accent, alpha), 3.0f * px, 9.0f * px, 8.0f * px);
        break;
      case BoardStyle::Connector::Luminous:
        path::stroke(poly, 1.0f, fade(st.whiteBranch, 0.25f * alpha), 8.0f * px, 2.0f * px, 9.0f * px);
        path::stroke(poly, 1.0f, fade(st.whiteBranch, alpha), 2.4f * px, 2.0f * px, 9.0f * px);
        break;
      case BoardStyle::Connector::Elbow:
        path::stroke(poly, 1.0f, fade(st.ink, alpha), 1.5f * px, 6.0f * px, 5.0f * px);
        headSize = 10.0f * px;
        break;
    }
    const Color head = fade(st.connector == BoardStyle::Connector::Elbow ? st.ink : st.connector == BoardStyle::Connector::Luminous ? st.whiteBranch : st.accent, alpha);
    // the head follows the end tangent of the path
    const Vector2 tip = poly.p[poly.n - 1], before = poly.p[poly.n - 2];
    const float tl = std::max(1.0f, std::hypot(tip.x - before.x, tip.y - before.y));
    const Vector2 dir = {(tip.x - before.x) / tl, (tip.y - before.y) / tl}, side = {-dir.y, dir.x};
    triangleBoth(tip, {tip.x - dir.x * headSize + side.x * headSize * 0.62f, tip.y - dir.y * headSize + side.y * headSize * 0.62f},
                 {tip.x - dir.x * headSize - side.x * headSize * 0.62f, tip.y - dir.y * headSize - side.y * headSize * 0.62f}, head);
  }
}

void BoardScene::drawJumpBadges(const SceneFrame& f) const {
  const BoardStyle& st = f.style;
  const float px = onePx(f.zoom());
  ThemeManager& themes = App::current().themes;
  for (const JumpInfo& jump : f.view.jumps) {
    const float alpha = jump.pending ? 1.0f : 0.65f;
    const Vector2 s = cardBottom(jump.move.from.l, jump.move.from.t), d = cardBottom(jump.move.to.l, jump.move.to.t);
    path::Poly poly;
    jumpPath(poly, st, s, d);
    const Vector2 m = badgeSpot(f.view, poly).at;
    const float radius = kBadgeRadius;
    switch (st.connector) {
      case BoardStyle::Connector::Curve:
        _soft.draw({m.x - radius, m.y - radius, 2 * radius, 2 * radius}, fade(st.glowWhite, alpha));
        DrawCircleV(m, radius, fade(st.cardWhite, alpha));
        DrawRing(m, radius - 2.5f * px, radius, 0, 360, 36, fade(st.accent, alpha));
        break;
      case BoardStyle::Connector::Luminous:
        _soft.draw({m.x - radius, m.y - radius, 2 * radius, 2 * radius}, fade(st.whiteBranch, 0.6f * alpha));
        DrawCircleV(m, radius, fade({20, 24, 90, 255}, alpha));
        DrawRing(m, radius - 2.0f * px, radius, 0, 360, 36, fade(st.whiteBranch, alpha));
        break;
      case BoardStyle::Connector::Elbow:
        DrawRectangleRec({m.x - radius * 0.8f, m.y - radius * 0.8f, radius * 1.6f, radius * 1.6f}, fade(WHITE, alpha));
        DrawRectangleLinesEx({m.x - radius * 0.8f, m.y - radius * 0.8f, radius * 1.6f, radius * 1.6f}, 1.5f * px, fade(st.ink, alpha));
        break;
    }
    const Texture2D& tex = *themes.getPieceTextures(pieceKey(jump.piece), st.grayPieces).open;
    const float side = radius * 1.35f;
    DrawTexturePro(tex, {0, 0, static_cast<float>(tex.width), static_cast<float>(tex.height)},
                   {m.x - side / 2, m.y - side / 2, side, side}, {0, 0}, 0.0f, fade(WHITE, alpha));
  }
}

void BoardScene::drawChecks(const SceneFrame& f, float reveal) const {
  const BoardStyle& st = f.style;
  if (f.view.checks.empty()) return;
  const float px = onePx(f.zoom());
  // Pulses with the clock; static under Reduce motion
  const float pulse = UI::Motion::reduced() ? 0.5f : 0.5f + 0.5f * std::sin(static_cast<float>(std::fmod(Input::time(), 3600.0)) * 3.2f);
  for (const CheckLine& c : f.view.checks) {
    const Rect a = BoardLayout::squareRect(BoardLayout::boardRect(c.attacker.l, c.attacker.t), f.dim, c.attacker.x, c.attacker.y);
    const Rect k = BoardLayout::squareRect(BoardLayout::boardRect(c.king.l, c.king.t), f.dim, c.king.x, c.king.y);
    const Vector2 from = {a.centerX(), a.centerY()}, to = {k.centerX(), k.centerY()};
    const float len = std::hypot(to.x - from.x, to.y - from.y);
    if (len < 1.0f) continue;
    const Vector2 dir = {(to.x - from.x) / len, (to.y - from.y) / len};
    const float ringR = k.w * 0.62f;
    const Vector2 tip = {to.x - dir.x * ringR, to.y - dir.y * ringR};
    path::Poly poly;
    poly.add(from);
    poly.add({tip.x - dir.x * 6.0f * px, tip.y - dir.y * 6.0f * px});
    path::stroke(poly, reveal, fade(st.check, 0.16f + 0.22f * pulse), (9.0f + 4.0f * pulse) * px);
    path::stroke(poly, reveal, st.check, (2.6f + 0.8f * pulse) * px);
    DrawCircleV(from, 4.0f * px, st.check);
    if (reveal < 0.999f) continue; // the head and the ring at the king appear once the line has arrived
    // arrow head at the king, a dot on the attacker, a ring round the king
    const Vector2 side = {-dir.y, dir.x};
    const float hs = 9.0f * px;
    triangleBoth(tip, {tip.x - dir.x * hs + side.x * hs * 0.6f, tip.y - dir.y * hs + side.y * hs * 0.6f},
                 {tip.x - dir.x * hs - side.x * hs * 0.6f, tip.y - dir.y * hs - side.y * hs * 0.6f}, st.check);
    DrawRing(to, ringR - 1.0f * px, ringR + 1.0f * px, 0, 360, 32, fade(st.check, 0.75f + 0.25f * pulse));
  }
}

// ---- Screen-space labels --------------------------------------------------------------------------------------------

void BoardScene::drawCardLabels(const SceneFrame& f, const BoardLayout& layout) const {
  const BoardStyle& st = f.style;
  const float zoom = f.zoom();
  constexpr float size = 12.0f; // a fixed size on screen: the label does not scale with the board
  const Font font = fontOf("ui.mono", 12);
  // The label strip below the board holds the text while it is tall enough; smaller, the board under the pointer names itself
  const bool inStrip = BoardLayout::kCardFooter * zoom >= 14.0f;
  const Vector2 mouse = Input::mousePosition();
  const auto& slots = layout.boards();
  for (size_t i = 0; i < slots.size() && i < f.view.boards.size(); ++i) {
    const Rect card = BoardLayout::cardRect(slots[i].rect);
    const BoardInfo& info = f.view.boards[i];
    const Vector2 p = f.toScreen({card.x, card.y + card.h - BoardLayout::kCardFooter});
    if (p.x > f.safe.x + f.safe.width || p.y > f.safe.y + f.safe.height || p.x + card.w * zoom < f.safe.x ||
        p.y < f.safe.y - 40.0f)
      continue;
    const bool white = info.whiteToMove;
    const float footer = BoardLayout::kCardFooter * zoom;
    const float a = info.inactive ? 0.55f : 1.0f;
    std::string label;
    Color color;
    switch (st.card) {
      case BoardStyle::Card::Paper:
        label = timelineLabel(info.timeline) + " " + kDot + " " + boardLabel(info.halfTurn);
        color = white ? st.muted : Color{232, 220, 196, 255};
        break;
      case BoardStyle::Card::Glow:
        label = boardLabel(info.halfTurn) + " " + kDot + " " + timelineLabel(info.timeline);
        color = info.role == BoardRole::Mandatory ? Color{197, 255, 243, 255} : st.hudMuted;
        break;
      default:
        label = "T" + std::to_string(info.halfTurn / 2 + 1) + kDot + timelineLabel(info.timeline) + (white ? " w" : " b");
        color = white ? st.ink : WHITE;
        break;
    }
    if (inStrip) {
      // Clipped to the card (to the left of the marker dot at its right end), never spilling onto the neighbours
      const float left = 9.0f * std::min(zoom, 1.0f);
      const float room = card.w * zoom - left - 22.0f * std::min(zoom, 1.0f);
      const std::string shown = ui::ellipsized(label, room, [&](const std::string& t) { return textWidth(font, t, size); });
      drawText(font, shown, p.x + left, p.y + (footer - size) / 2.0f - 1.0f, size, fade(color, a));
      if (ui::audit::enabled()) ui::audit::fit("card label", shown, textWidth(font, shown, size), size, {0, 0, card.w * zoom, footer});
    } else {
      const Vector2 top = f.toScreen({card.x, card.y});
      if (CheckCollisionPointRec(mouse, {top.x, top.y, card.w * zoom, card.h * zoom}) && !ui::pointerConsumed()) {
        const float tw = textWidth(font, label, size);
        const Rectangle tag = {std::clamp(top.x, f.safe.x, std::max(f.safe.x, f.safe.x + f.safe.width - tw - 14.0f)),
                               std::max(top.y - 24.0f, f.safe.y + 2.0f), tw + 14.0f, 20.0f};
        drawRoundedRect(tag, 10.0f, st.hudFill);
        drawRoundedLines(tag, 10.0f, 1.0f, st.hudBorder);
        drawText(font, label, tag.x + 7.0f, tag.y + 3.0f, size, st.hudText);
      }
    }
    if (inStrip && st.card == BoardStyle::Card::Ink && info.role == BoardRole::Mandatory) {
      const Vector2 corner = f.toScreen({card.x + card.w, card.y + card.h});
      drawTextCentered(font, "!", corner.x - 8.0f, corner.y - 17.0f, 12.0f, white ? WHITE : st.ink);
    }
  }
}

void BoardScene::drawJumpLabels(const SceneFrame& f) const {
  const BoardStyle& st = f.style;
  const Font font = fontOf(st.card == BoardStyle::Card::Paper ? "ui.public_sans" : "ui.mono", UI::Font::minimum);
  const float zoom = f.zoom();
  for (const JumpInfo& jump : f.view.jumps) {
    const Vector2 s = cardBottom(jump.move.from.l, jump.move.from.t), d = cardBottom(jump.move.to.l, jump.move.to.t);
    path::Poly poly;
    jumpPath(poly, st, s, d);
    const BadgeSpot spot = badgeSpot(f.view, poly);
    const Vector2 m = f.toScreen(spot.at);
    const auto& from = jump.move.from;
    const auto& to = jump.move.to;
    std::string text = "jump ";
    const std::string a = "T" + std::to_string(from.t / 2 + 1), b = "T" + std::to_string(to.t / 2 + 1);
    if (from.l != to.l && from.t != to.t) text += timelineLabel(from.l) + " " + a + " -> " + timelineLabel(to.l) + " " + b;
    else if (from.t != to.t) text += a + " -> " + b;
    else text += timelineLabel(from.l) + " -> " + timelineLabel(to.l);

    // The text sits under the badge only where it covers no board; otherwise it is the badge's tooltip
    const float tw = textWidth(font, text, UI::Font::minimum);
    const float r = kBadgeRadius * zoom;
    const Rectangle label = {m.x - tw / 2.0f, m.y + r + 3.0f, tw, UI::Font::minimum + 2.0f};
    bool free = spot.clear && zoom >= 0.45f;
    for (const BoardInfo& bi : f.view.boards) {
      if (!free) break;
      const Rect c = cardOf(bi.timeline, bi.halfTurn);
      const Vector2 p = f.toScreen({c.x, c.y});
      if (CheckCollisionRecs(label, {p.x, p.y, c.w * zoom, c.h * zoom})) free = false;
    }
    if (free) {
      Color color = st.connector == BoardStyle::Connector::Luminous ? st.whiteBranch : st.connector == BoardStyle::Connector::Elbow ? st.ink : st.accent;
      if (st.connector == BoardStyle::Connector::Curve) color = {184, 83, 47, 255}; // the accent is too light for small text
      drawTextCentered(font, text, m.x, label.y, UI::Font::minimum, fade(color, jump.pending ? 1.0f : 0.75f));
    } else {
      queueTip({m.x - std::max(r, 10.0f), m.y - std::max(r, 10.0f), 2 * std::max(r, 10.0f), 2 * std::max(r, 10.0f)}, text);
    }
  }
}

namespace {
std::string upper(std::string s) {
  for (char& c : s) if (c >= 'a' && c <= 'z') c = static_cast<char>(c - 'a' + 'A');
  return s;
}
} // namespace

std::optional<BoardScene::Minimap> BoardScene::minimap(const SceneFrame& f, float y, float height) {
  if (f.view.boards.empty() || f.view.timelines.empty()) return std::nullopt;
  const float zoom = f.zoom();
  bool off = zoom < 0.3f;
  for (const BoardInfo& b : f.view.boards) {
    if (off) break;
    const Rect c = cardOf(b.timeline, b.halfTurn);
    const Vector2 p = f.toScreen({c.x, c.y});
    off = p.x < f.safe.x - 1.0f || p.y < f.safe.y - 1.0f || p.x + c.w * zoom > f.safe.x + f.safe.width + 1.0f ||
          p.y + c.h * zoom > f.safe.y + f.safe.height + 1.0f;
  }
  if (!off) return std::nullopt;
  const int rows = f.view.timelines.back().id - f.view.timelines.front().id + 1;
  const int cols = f.view.lastHalfTurn - f.view.firstHalfTurn + 1;
  Minimap m;
  m.cell = std::clamp(std::floor((height - 6.0f) / static_cast<float>(rows)) - 1.0f, 2.0f, 6.0f);
  const float step = m.cell + 1.0f;
  const float w = static_cast<float>(cols) * step - 1.0f, h = static_cast<float>(rows) * step - 1.0f;
  m.rect = {f.safe.x + f.safe.width - w - 10.0f, y + (height - h) / 2.0f - 1.0f, w, h};
  m.firstHalfTurn = f.view.firstHalfTurn;
  m.topTimeline = f.view.timelines.back().id;
  return m;
}

std::optional<std::pair<int, int>> BoardScene::minimapBoardAt(const SceneFrame& f, float y, float height, Vector2 point) {
  const auto m = minimap(f, y, height);
  if (!m || !CheckCollisionPointRec(point, {m->rect.x - 2.0f, m->rect.y - 2.0f, m->rect.width + 4.0f, m->rect.height + 4.0f})) return std::nullopt;
  const float step = m->cell + 1.0f;
  const int col = std::clamp(static_cast<int>(std::floor((point.x - m->rect.x) / step)), 0, f.view.lastHalfTurn - m->firstHalfTurn);
  const int row = std::max(0, static_cast<int>(std::floor((point.y - m->rect.y) / step)));
  const int timeline = m->topTimeline - row, halfTurn = m->firstHalfTurn + col;
  if (!f.view.board(timeline, halfTurn)) return std::nullopt;
  return std::make_pair(timeline, halfTurn);
}

void BoardScene::drawRuler(const SceneFrame& f, float y, float height) const {
  const BoardStyle& st = f.style;
  const float zoom = f.zoom();
  const float pitch = BoardLayout::kPitch * zoom;
  const float baseline = y + height - 2.0f;
  const bool atlas = st.lane == BoardStyle::Lane::Band, deep = st.lane == BoardStyle::Lane::Thread;
  const Font labelFont = atlas ? fontOf("ui.montserrat_bold", 14) : fontOf("ui.mono", 13);
  const Font letterFont = fontOf(atlas ? "ui.public_sans" : "ui.mono", 10);

  BeginScissorMode(static_cast<int>(f.safe.x - 6), static_cast<int>(y - 4), static_cast<int>(f.safe.width + 12), static_cast<int>(height + 8));
  // The ruler line
  const float x0 = f.safe.x, x1 = f.safe.x + f.safe.width;
  if (deep) DrawRectangleGradientH(static_cast<int>(x0), static_cast<int>(baseline), static_cast<int>(x1 - x0), 1, fade(st.grid, 0.0f), fade(st.grid, 0.45f));
  else if (atlas) DrawRectangle(static_cast<int>(x0), static_cast<int>(baseline), static_cast<int>(x1 - x0), 2, st.cardEdgeWhite);
  else DrawRectangle(static_cast<int>(x0), static_cast<int>(baseline), static_cast<int>(x1 - x0), 1, st.ink);

  const int present = f.view.presentHalfTurn;
  const float presentX = f.toScreen({presentColumnX(presentColumn(f)), 0}).x;

  // The present marker names the turn it is on ("Present . T3w"): it sits where the turn's own label would be
  const std::string badgeText = atlas ? std::string("Present ") + kDot + " " + boardLabel(present) : upper(std::string("Present ") + kDot + " " + boardLabel(present));
  const Font badgeFont = atlas ? fontOf("ui.public_sans_bold", 14) : fontOf("ui.mono", 14);
  const float badgeSpacing = atlas ? 0.0f : 1.0f;
  const float badgeW = textWidth(badgeFont, badgeText, 14.0f, badgeSpacing) + 22.0f;
  const float badgeH = 22.0f;

  const int stride = std::max(1, static_cast<int>(std::ceil(46.0f / (2.0f * pitch)))); // label every n-th turn when crowded
  const Color tickColor = atlas ? Color{185, 165, 129, 255} : deep ? fade(st.grid, 0.6f) : st.ink;
  const Color labelColor = atlas ? st.ink : deep ? st.hudMuted : st.ink;
  const Vector2 mouse = Input::mousePosition();
  const bool overRuler = mouse.y >= y - 2.0f && mouse.y <= y + height && mouse.x >= x0 && mouse.x <= x1;
  float bestTick = 1e9f;
  Rectangle tickTip{};
  std::string tickText;

  for (int c = f.view.firstHalfTurn; c <= f.view.lastHalfTurn; ++c) {
    const float x = f.toScreen({BoardLayout::boardRect(0, c).centerX(), 0}).x;
    if (x < x0 - 30 || x > x1 + 30) continue;
    const bool white = c % 2 == 0, isPresent = c == present;
    const float tick = white ? 8.0f : 5.0f;
    const bool underBadge = std::fabs(x - presentX) < badgeW / 2.0f + 6.0f;
    DrawRectangle(static_cast<int>(std::round(x)), static_cast<int>(baseline - tick), 1 + (atlas ? 1 : 0), static_cast<int>(tick),
                  isPresent ? st.accent : tickColor);
    if (white && ((c / 2) % stride == 0) && !underBadge) {
      const std::string label = "T" + std::to_string(c / 2 + 1);
      drawTextCentered(labelFont, label, x, y + 1.0f, 14.0f, labelColor);
    }
    if (pitch >= 40.0f && !underBadge)
      drawText(letterFont, white ? "w" : "b", x + 4.0f, baseline - 12.0f, 10.0f, st.muted);
    if (overRuler && std::fabs(mouse.x - x) < bestTick && std::fabs(mouse.x - x) < std::max(10.0f, pitch / 2.0f)) {
      bestTick = std::fabs(mouse.x - x);
      tickTip = {x - 10.0f, y, 20.0f, height};
      tickText = "Turn " + std::to_string(c / 2 + 1) + ", " + (white ? "White" : "Black");
    }
  }
  if (!tickText.empty()) queueTip(tickTip, tickText);

  // The present marker sits on the ruler at the present column
  if (presentX > x0 - 40 && presentX < x1 + 40) {
    const float bx = std::clamp(presentX, x0 + badgeW / 2.0f, std::max(x0 + badgeW / 2.0f, x1 - badgeW / 2.0f));
    const Rectangle pill = {std::floor(bx - badgeW / 2.0f), y, badgeW, badgeH};
    const float ty = y + (badgeH - 14.0f) / 2.0f - 1.0f;
    if (atlas) {
      drawRoundedRect(pill, 11.0f, st.accent);
      drawTextCentered(badgeFont, badgeText, bx, ty, 14.0f, WHITE);
    } else if (deep) {
      drawRoundedRect(pill, 11.0f, {14, 30, 70, 255}); // opaque, so the ruler tick does not show through
      drawRoundedRect(pill, 11.0f, fade(st.accent, 0.16f));
      drawRoundedLines(pill, 11.0f, 1.0f, fade(st.accent, 0.8f));
      drawTextCentered(badgeFont, badgeText, bx, ty, 14.0f, {197, 255, 243, 255}, badgeSpacing);
    } else {
      DrawRectangleRec(pill, st.accent);
      drawTextCentered(badgeFont, badgeText, bx, ty, 14.0f, WHITE, badgeSpacing);
    }
    if (ui::audit::enabled()) ui::audit::fit("present badge", badgeText, textWidth(badgeFont, badgeText, 14.0f, badgeSpacing), 14.0f, {0, 0, badgeW - 8.0f, badgeH});
  }

  // The strip of the whole multiverse at the right end, while part of it is out of view
  if (const auto m = minimap(f, y, height)) {
    const float step = m->cell + 1.0f;
    const Rectangle back = {m->rect.x - 5.0f, m->rect.y - 4.0f, m->rect.width + 10.0f, m->rect.height + 8.0f};
    drawRoundedRect(back, 5.0f, st.hudFill);
    drawRoundedLines(back, 5.0f, 1.0f, st.hudBorder);
    const float presentCol = (presentColumn(f) - static_cast<float>(m->firstHalfTurn)) * step;
    DrawRectangleRec({m->rect.x + std::floor(presentCol) - 0.5f, m->rect.y - 2.0f, step, m->rect.height + 4.0f}, fade(st.accent, 0.3f));
    for (const BoardInfo& b : f.view.boards) {
      const float cx = m->rect.x + static_cast<float>(b.halfTurn - m->firstHalfTurn) * step;
      const float cy = m->rect.y + static_cast<float>(m->topTimeline - b.timeline) * step;
      Color c = b.role == BoardRole::Mandatory ? st.accent : b.role == BoardRole::Optional ? fade(st.muted, 0.9f) : fade(st.muted, 0.42f);
      if (b.inactive) c = fade(c, 0.6f);
      DrawRectangleRec({cx, cy, m->cell, m->cell}, c);
    }
    // The part the camera shows: from the safe area's corners back into board coordinates
    const Vector2 w0 = GetScreenToWorld2D({f.safe.x, f.safe.y}, f.camera), w1 = GetScreenToWorld2D({f.safe.x + f.safe.width, f.safe.y + f.safe.height}, f.camera);
    const float col0 = (w0.x - presentColumnX(0.0f)) / BoardLayout::kPitch + 0.5f - static_cast<float>(m->firstHalfTurn);
    const float col1 = (w1.x - presentColumnX(0.0f)) / BoardLayout::kPitch + 0.5f - static_cast<float>(m->firstHalfTurn);
    const float topY = laneCenterY(m->topTimeline);
    const float row0 = (w0.y - topY) / BoardLayout::kPitch + 0.5f, row1 = (w1.y - topY) / BoardLayout::kPitch + 0.5f;
    Rectangle view = {m->rect.x + col0 * step - 0.5f, m->rect.y + row0 * step - 0.5f, (col1 - col0) * step, (row1 - row0) * step};
    const float vx0 = std::max(view.x, m->rect.x - 2.0f), vy0 = std::max(view.y, m->rect.y - 2.0f);
    const float vx1 = std::min(view.x + view.width, m->rect.x + m->rect.width + 2.0f), vy1 = std::min(view.y + view.height, m->rect.y + m->rect.height + 2.0f);
    if (vx1 > vx0 && vy1 > vy0) DrawRectangleLinesEx({vx0, vy0, vx1 - vx0, vy1 - vy0}, 1.0f, st.hudText);
    queueTip(back, "The whole multiverse: click a board to go there");
  }
  EndScissorMode();
}

void BoardScene::drawLaneLabels(const SceneFrame& f, Rectangle area) const {
  const BoardStyle& st = f.style;
  const float zoom = f.zoom();
  const float laneH = BoardLayout::kPitch * zoom;
  const bool atlas = st.lane == BoardStyle::Lane::Band, deep = st.lane == BoardStyle::Lane::Thread;
  const float x = area.x + 14.0f;
  const float textRoom = area.x + area.width - x - 6.0f; // the column ends where the boards begin
  constexpr float small = static_cast<float>(UI::Font::minimum);
  auto fitted = [&](const Font& font, const std::string& t, float size, float spacing = 0.0f) {
    return ui::ellipsized(t, textRoom, [&](const std::string& u) { return textWidth(font, u, size, spacing); });
  };
  if (laneH >= 22.0f) {
    BeginScissorMode(static_cast<int>(area.x), static_cast<int>(area.y), static_cast<int>(area.width), static_cast<int>(area.height));
    for (const TimelineInfo& tl : f.view.timelines) {
      const float cy = f.toScreen({0, laneCenterY(tl.id)}).y;
      // The sub line and the inactive tag need room; on a short lane only the pill is left (its tooltip says the rest)
      const bool roomy = laneH >= 88.0f;
      const float blockH = roomy ? (tl.active ? 54.0f : 82.0f) : 24.0f;
      const float top = cy - blockH / 2.0f;
      if (top + blockH < area.y || top > area.y + area.height) continue;
      const float alpha = tl.active ? 1.0f : 0.72f;
      const std::string label = timelineLabel(tl.id);
      const std::string sub = tl.created ? "from " + timelineLabel(tl.parent) + " " + kDot + " T" + std::to_string(tl.forkHalfTurn / 2 + 1)
                                         : (tl.id == 0 ? "origin timeline" : "original");
      const Color pillColor = st.lanePill[laneColor(tl.id)];
      float yy = top;
      std::string tip = label + ": ";
      tip += tl.created ? std::string("created by ") + (tl.byWhite ? "White" : "Black") + " at T" + std::to_string(tl.forkHalfTurn / 2 + 1) +
                              ", branching from " + timelineLabel(tl.parent)
                        : (tl.id == 0 ? "the timeline the game started on" : "one of the timelines the game started with");
      if (!tl.active) tip += "\nInactive: this timeline is too far behind to need moves.";
      if (atlas) {
        drawRoundedRect({x, yy, 66, 24}, 12.0f, fade(pillColor, alpha));
        drawTextCentered(fontOf("ui.montserrat_bold", 16), label, x + 33, yy + 2.0f, 16.0f, fade(WHITE, alpha));
        yy += 28.0f;
        if (roomy) {
          const Font font = fontOf("ui.public_sans", UI::Font::minimum);
          drawText(font, fitted(font, sub, small), x, yy, small, fade(st.muted, alpha));
        }
        yy += 20.0f;
        if (!tl.active && roomy) {
          const Font font = fontOf("ui.public_sans", UI::Font::minimum);
          drawRoundedLines({x, yy, 68, 24}, 12.0f, 1.2f, fade(st.muted, alpha));
          drawTextCentered(font, "inactive", x + 34, yy + 3.0f, small, fade(st.muted, alpha));
          if (ui::audit::enabled()) ui::audit::fit("inactive tag", "inactive", textWidth(font, "inactive", small), small, {0, 0, 64.0f, 24.0f});
          queueTip({x, yy, 68, 24}, "Inactive: this timeline is too far behind to need moves.");
        }
      } else if (deep) {
        DrawCircleV({x + 4, yy + 11}, 4.0f, fade(pillColor, alpha));
        DrawCircleV({x + 4, yy + 11}, 8.0f, fade(pillColor, 0.18f * alpha));
        drawText(fontOf("ui.public_sans_bold", 20), label, x + 18, yy, 20.0f, fade(st.ink, alpha));
        yy += 28.0f;
        if (roomy) {
          const Font font = fontOf("ui.mono", UI::Font::minimum);
          drawText(font, fitted(font, sub, small), x, yy, small, fade(st.muted, alpha));
        }
        yy += 24.0f;
        if (!tl.active && roomy) {
          const Font font = fontOf("ui.mono", UI::Font::minimum);
          drawRoundedLines({x, yy, 92, 24}, 12.0f, 1.0f, fade(st.muted, alpha));
          drawTextCentered(font, "INACTIVE", x + 46, yy + 3.0f, small, fade(st.muted, alpha), 1.0f);
          if (ui::audit::enabled()) ui::audit::fit("inactive tag", "INACTIVE", textWidth(font, "INACTIVE", small, 1.0f), small, {0, 0, 88.0f, 24.0f});
          queueTip({x, yy, 92, 24}, "Inactive: this timeline is too far behind to need moves.");
        }
      } else {
        drawText(fontOf("ui.mono", 20), label, x, yy, 20.0f, fade(st.ink, alpha));
        yy += 26.0f;
        if (roomy) {
          const Font font = fontOf("ui.mono", UI::Font::minimum);
          drawText(font, fitted(font, sub, small), x, yy, small, fade(st.muted, alpha));
        }
        yy += 24.0f;
        if (!tl.active && roomy) {
          const Font font = fontOf("ui.mono", UI::Font::minimum);
          DrawRectangleLinesEx({x, yy, 92, 24}, 1.0f, fade(st.muted, alpha));
          drawTextCentered(font, "INACTIVE", x + 46, yy + 3.0f, small, fade(st.muted, alpha), 1.0f);
          if (ui::audit::enabled()) ui::audit::fit("inactive tag", "INACTIVE", textWidth(font, "INACTIVE", small, 1.0f), small, {0, 0, 88.0f, 24.0f});
          queueTip({x, yy, 92, 24}, "Inactive: this timeline is too far behind to need moves.");
        }
      }
      queueTip({x - 6.0f, top, textRoom + 6.0f, std::min(blockH, 30.0f)}, tip);
    }
    EndScissorMode();
  }
  // Tooltips asked for by the labels of this frame (the lane pills, the ruler, the jump badges): outside every clip, on top
  if (_tip) {
    ui::tooltip(_tip->anchor, _tip->text);
    _tip.reset();
  }
}

} // namespace play
