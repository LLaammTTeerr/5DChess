#include "play/BoardRenderer.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <rlgl.h>
#include "App.h"
#include "Input.h"
#include "Render/Motion.h"
#include "Render/PieceTheme.h"
#include "Render/UITheme.h"

namespace play {

namespace {

Color fade(Color c, float a) {
  c.a = static_cast<unsigned char>(c.a * (a < 0.0f ? 0.0f : (a > 1.0f ? 1.0f : a)));
  return c;
}

float worldThickness(float px, float zoom) { return px / (zoom > 0.01f ? zoom : 1.0f); } // constant on screen

bool isHidden(const BoardLook& look, int x, int y) {
  for (int i = 0; i < look.hiddenCount; ++i)
    if (look.hidden[i].x == x && look.hidden[i].y == y) return true;
  return false;
}

} // namespace

const std::string& pieceKey(const Chess::Piece& piece) {
  static const std::array<std::string, 12> keys = [] {
    std::array<std::string, 12> k;
    for (int c = 0; c < 2; ++c)
      for (int t = 0; t < 6; ++t)
        k[c * 6 + t] = std::string(c == 0 ? "white_" : "black_") + Chess::pieceName(static_cast<Chess::PieceType>(t));
    return k;
  }();
  return keys[static_cast<int>(piece.color) * 6 + static_cast<int>(piece.type)];
}

void drawBoard(const Chess::Board& board, const Rect& areaRect, const BoardLook& look, float zoom) {
  const Rectangle area = toRay(areaRect);
  const int dim = board.dim();

  // Newly created boards grow in from 0.92x while fading (scaled about their centre)
  const bool entering = look.enter < 0.999f;
  const float alpha = entering ? look.enter : 1.0f;
  if (entering) {
    const float s = UI::Motion::lerp(0.92f, 1.0f, look.enter);
    rlPushMatrix();
    rlTranslatef(areaRect.centerX(), areaRect.centerY(), 0.0f);
    rlScalef(s, s, 1.0f);
    rlTranslatef(-areaRect.centerX(), -areaRect.centerY(), 0.0f);
  }

  for (int x = 0; x < dim; ++x) {
    for (int y = 0; y < dim; ++y) {
      const Rect sq = BoardLayout::squareRect(areaRect, dim, x, y);
      // Checkerboard: on an 8x8 board a1 (0,0) is dark; the corner opposite to it (N-1,0) is light
      const bool light = (dim - 1 - x + y) % 2 == 0;
      DrawRectangle(sq.x, sq.y, sq.w, sq.h, fade(light ? UI::Color::squareLight : UI::Color::squareDark, alpha));
    }
  }

  // Pieces
  const Color tint = fade(WHITE, alpha);
  const double now = look.blink ? Input::time() : 0.0;
  for (int x = 0; x < dim; ++x) {
    for (int y = 0; y < dim; ++y) {
      const auto piece = board.at(Chess::Position2D(x, y));
      if (!piece || isHidden(look, x, y)) continue;
      const std::string& name = pieceKey(*piece);
      const PieceTextures& tex = App::current().themes.getPieceTextures(name);
      Texture2D* texture = tex.open;
      if (look.blink && tex.blink) {
        const unsigned id = look.blinkSeed * 64u + static_cast<unsigned>(x * 8 + y) * 2654435761u +
                            static_cast<unsigned>(name.size() * 31 + name[0] * 7 + name[6]);
        if (UI::Motion::blinkClosed(id, now)) texture = tex.blink;
      }
      DrawTexturePro(*texture, Rectangle{0, 0, static_cast<float>(texture->width), static_cast<float>(texture->height)},
                     toRay(BoardLayout::squareRect(areaRect, dim, x, y)), Vector2{0, 0}, 0.0f, tint);
    }
  }

  // Border: accent for boards the current player may still move from, neutral otherwise
  if (look.moveable)
    DrawRectangleLinesEx(area, worldThickness(3.0f, zoom), fade(UI::withAlpha(UI::Color::accent, 170), alpha));
  else
    DrawRectangleLinesEx(area, worldThickness(2.0f, zoom), fade(UI::Color::border, alpha));

  if (entering) rlPopMatrix();
}

void drawBoardOutline(const Rect& area, float zoom) {
  DrawRectangleLinesEx(toRay(area), worldThickness(4.0f, zoom), UI::Color::selected);
}

void drawLegalTarget(const Rect& square, bool occupied, float scale) {
  if (scale <= 0.0f) return;
  const Vector2 center = {square.centerX(), square.centerY()};
  const float a = scale > 1.0f ? 1.0f : scale;
  if (occupied) {
    // Capture: ring around the enemy piece
    DrawRing(center, square.w * 0.40f * scale, square.w * 0.46f * scale, 0, 360, 36, fade(UI::Color::capture, a));
  } else {
    // Legal empty target: sage dot
    DrawCircleV(center, square.w * 0.17f * scale, fade(UI::withAlpha(UI::Color::legalTarget, 220), a));
  }
}

void drawHoverSquare(const Rect& sq, float alpha) {
  DrawRectangle(sq.x, sq.y, sq.w, sq.h, fade(UI::Color::hover, alpha));
}

void drawSelectedSquare(const Rect& square, float zoom) {
  // Selected piece: accent tint plus a 3 px accent outline (the piece itself is drawn lifted on top)
  DrawRectangleRec(toRay(square), UI::Color::hover);
  DrawRectangleLinesEx(toRay(square), worldThickness(UI::Space::outline, zoom), UI::Color::selected);
}

void drawLiftedPiece(const Rect& sq, const std::string& key, float lift) {
  const PieceTextures& tex = App::current().themes.getPieceTextures(key);
  const float up = lift * sq.h * 0.09f;
  const float grow = 1.0f + 0.06f * lift;
  // Soft shadow stays on the ground while the piece rises
  DrawEllipse(static_cast<int>(sq.x + sq.w / 2), static_cast<int>(sq.y + sq.h * 0.9f), sq.w * (0.28f + 0.03f * lift),
              sq.h * 0.06f, fade(UI::Color::shadow, 0.5f + 0.5f * (lift > 1.0f ? 1.0f : lift)));
  const Rectangle dst = {sq.x - sq.w * (grow - 1.0f) / 2, sq.y - up - sq.h * (grow - 1.0f) / 2, sq.w * grow, sq.h * grow};
  DrawTexturePro(*tex.open, {0, 0, static_cast<float>(tex.open->width), static_cast<float>(tex.open->height)}, dst, {0, 0},
                 0.0f, WHITE);
}

void drawFlights(const std::vector<MoveAnimator::Flight>& flights) {
  ThemeManager& themes = App::current().themes;
  for (const auto& f : flights) {
    // Captured piece shrinks and fades where it stood (drawn under the mover)
    if (!f.spec.victim.empty() && !f.victimFade.done()) {
      const float p = f.victimFade.progress();
      Texture2D& tex = *themes.getPieceTextures(f.spec.victim).open;
      const float s = f.size * UI::Motion::lerp(1.0f, 0.55f, p);
      DrawTexturePro(tex, {0, 0, (float)tex.width, (float)tex.height}, {f.to.x - s / 2, f.to.y - s / 2, s, s}, {0, 0}, 0.0f,
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
      Texture2D& tex = *themes.getPieceTextures(f.spec.piece).open;
      DrawTexturePro(tex, {0, 0, (float)tex.width, (float)tex.height}, {pos.x - s / 2, pos.y - s / 2, s, s}, {0, 0}, 0.0f,
                     WHITE);
    }
  }
}

void drawPresentLine(float halfTurn, const Rect& bounds, float zoom) {
  const float x = halfTurn * BoardLayout::kPitch + BoardLayout::kBoardSize / 2.0f;
  const float padding = BoardLayout::kBoardSize * 1.5f; // reaches well past the outermost boards
  const float yStart = bounds.y - padding, yEnd = bounds.y + bounds.h + padding;
  // 3 screen pixels wide at any zoom, over one faint glow step
  const float px = 1.0f / std::max(zoom, 0.05f);
  const float core = 3.0f * px;
  DrawLineEx({x, yStart}, {x, yEnd}, core + 4.0f * px, UI::withAlpha(UI::Color::presentLine, 40));
  DrawLineEx({x, yStart}, {x, yEnd}, core, UI::Color::presentLine);
}

} // namespace play
