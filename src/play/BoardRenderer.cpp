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

float worldThickness(float px, float zoom) { return px / (zoom > 0.01f ? zoom : 1.0f); } // constant on screen

bool isHidden(const BoardLook& look, int x, int y) {
  for (int i = 0; i < look.hiddenCount; ++i)
    if (look.hidden[i].x == x && look.hidden[i].y == y) return true;
  return false;
}

// Boards grow in from 0.92x about their centre while they fade in
struct EnterScope {
  bool active;
  EnterScope(const Rect& area, float enter) : active(enter < 0.999f) {
    if (!active) return;
    const float s = UI::Motion::lerp(0.92f, 1.0f, enter);
    rlPushMatrix();
    rlTranslatef(area.centerX(), area.centerY(), 0.0f);
    rlScalef(s, s, 1.0f);
    rlTranslatef(-area.centerX(), -area.centerY(), 0.0f);
  }
  ~EnterScope() {
    if (active) rlPopMatrix();
  }
};

float boardAlpha(const BoardLook& look) {
  return (look.enter < 0.999f ? look.enter : 1.0f) * (look.inactive ? 0.55f : 1.0f);
}

// raylib culls clockwise triangles: draw both windings so the caller need not care
void fillTriangle(Vector2 a, Vector2 b, Vector2 c, Color color) {
  DrawTriangle(a, b, c, color);
  DrawTriangle(a, c, b, color);
}

} // namespace

Color fade(Color c, float a) {
  c.a = static_cast<unsigned char>(c.a * (a < 0.0f ? 0.0f : (a > 1.0f ? 1.0f : a)));
  return c;
}

void drawRoundedRect(Rectangle r, float radius, Color fill) {
  DrawRectangleRounded(r, 2.0f * radius / std::min(r.width, r.height), 8, fill);
}

void drawRoundedLines(Rectangle r, float radius, float thickness, Color color) {
  DrawRectangleRoundedLinesEx(r, 2.0f * radius / std::min(r.width, r.height), 8, thickness, color);
}

const std::string& pieceKey(const Chess::Piece& piece) { return pieceKey(piece.color, piece.type); }

const std::string& pieceKey(Chess::PieceColor color, Chess::PieceType type) {
  static const std::array<std::string, 12> keys = [] {
    std::array<std::string, 12> k;
    for (int c = 0; c < 2; ++c)
      for (int t = 0; t < 6; ++t)
        k[c * 6 + t] = std::string(c == 0 ? "white_" : "black_") + Chess::pieceName(static_cast<Chess::PieceType>(t));
    return k;
  }();
  return keys[static_cast<int>(color) * 6 + static_cast<int>(type)];
}

// ---------------------------------------------------------------------------------------------------------------------

SoftBox::SoftBox() {
  constexpr int size = 128;
  constexpr float spread = kSpread, cornerRadius = 14.0f;
  Image image = GenImageColor(size, size, BLANK);
  Color* pixels = static_cast<Color*>(image.data);
  const float half = size / 2.0f, inner = half - spread - cornerRadius; // half extent of the box without its corners
  for (int y = 0; y < size; ++y) {
    for (int x = 0; x < size; ++x) {
      const float qx = std::fabs(x + 0.5f - half) - inner, qy = std::fabs(y + 0.5f - half) - inner;
      const float d = std::hypot(std::max(qx, 0.0f), std::max(qy, 0.0f)) + std::min(std::max(qx, qy), 0.0f) - cornerRadius;
      const float t = std::clamp(d / spread, 0.0f, 1.0f);
      pixels[y * size + x] = {255, 255, 255, static_cast<unsigned char>(255.0f * (1.0f - t) * (1.0f - t))};
    }
  }
  _texture = LoadTextureFromImage(image);
  UnloadImage(image);
  SetTextureFilter(_texture, TEXTURE_FILTER_BILINEAR);
}

SoftBox::~SoftBox() {
  if (_texture.id != 0) UnloadTexture(_texture);
}

void SoftBox::draw(Rectangle box, Color tint) const {
  if (tint.a == 0) return;
  const NPatchInfo patch = {{0, 0, 128, 128}, 64, 64, 64, 64, NPATCH_NINE_PATCH};
  DrawTextureNPatch(_texture, patch, {box.x - kSpread, box.y - kSpread, box.width + 2 * kSpread, box.height + 2 * kSpread}, {0, 0},
                    0.0f, tint);
}

// ---------------------------------------------------------------------------------------------------------------------

void drawBoardHalo(const Rect& areaRect, const BoardLook& look, const BoardStyle& style, const SoftBox& soft) {
  if (style.card == BoardStyle::Card::Ink) return;
  const float a = boardAlpha(look);
  if (a <= 0.01f) return;
  EnterScope scope(areaRect, look.enter);
  Rectangle card = toRay(BoardLayout::cardRect(areaRect));
  if (style.card == BoardStyle::Card::Paper) { // a soft drop shadow
    card.y += 8.0f;
    soft.draw(card, fade(look.whiteToMove ? style.glowWhite : style.glowBlack, a));
    return;
  }
  // Deep space: the side to move lights the card (warm / cool); a board that must be moved on burns brighter, in teal
  if (look.role == BoardRole::Mandatory) {
    soft.draw(card, fade(style.mandatory, 0.75f * a));
    soft.draw(card, fade(style.mandatory, 0.55f * a));
  } else {
    const float strength = look.role == BoardRole::Optional ? 0.60f : 0.32f;
    soft.draw(card, fade(look.whiteToMove ? style.glowWhite : style.glowBlack, strength * a));
  }
}

void drawBoard(const Chess::Board& board, const Rect& areaRect, const BoardLook& look, const BoardStyle& style, float zoom) {
  const Rectangle area = toRay(areaRect);
  const Rectangle card = toRay(BoardLayout::cardRect(areaRect));
  const int dim = board.dim();
  const float a = boardAlpha(look);
  const float px = worldThickness(1.0f, zoom);
  const bool white = look.whiteToMove;
  EnterScope scope(areaRect, look.enter);

  // ---- Card ----
  switch (style.card) {
    case BoardStyle::Card::Paper:
      drawRoundedRect(card, 12.0f, fade(white ? style.cardWhite : style.cardBlack, a));
      drawRoundedLines(card, 12.0f, 1.2f * px, fade(white ? style.cardEdgeWhite : style.cardEdgeBlack, a));
      break;
    case BoardStyle::Card::Glow:
      drawRoundedRect(card, 7.0f, fade(style.cardWhite, a));
      break;
    case BoardStyle::Card::Ink:
      DrawRectangleRec(card, fade(style.cardWhite, a));
      if (!white)
        DrawRectangleRec({card.x, area.y + area.height + BoardLayout::kCardPad, card.width, BoardLayout::kCardFooter},
                         fade(style.cardBlack, a));
      break;
  }

  // ---- Squares: a light board with the dark squares on top (on an 8x8 board a1 (0,0) is dark) ----
  DrawRectangleRec(area, fade(style.squareLight, a));
  for (int x = 0; x < dim; ++x) {
    for (int y = 0; y < dim; ++y) {
      if ((dim - 1 - x + y) % 2 == 0) continue; // light
      const Rect sq = BoardLayout::squareRect(areaRect, dim, x, y);
      DrawRectangleRec(toRay(sq), fade(style.squareDark, a));
    }
  }
  for (int i = 0; i < look.checkedCount; ++i) {
    const Rect sq = BoardLayout::squareRect(areaRect, dim, look.checked[i].x, look.checked[i].y);
    DrawRectangleRec(toRay(sq), fade(style.checkTint, a));
  }
  if (style.card == BoardStyle::Card::Ink) DrawRectangleLinesEx(area, px, fade(style.ink, a));

  // ---- Pieces ----
  const Color tint = fade(WHITE, a);
  const bool gray = style.grayPieces || look.inactive;
  const double now = look.blink ? Input::time() : 0.0;
  ThemeManager& themes = App::current().themes;
  for (int x = 0; x < dim; ++x) {
    for (int y = 0; y < dim; ++y) {
      const auto piece = board.at(Chess::Position2D(x, y));
      if (!piece || isHidden(look, x, y)) continue;
      const std::string& name = pieceKey(*piece);
      const PieceTextures& tex = themes.getPieceTextures(name, gray);
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

  // ---- Frame: must move / may move / history ----
  const bool mandatory = look.role == BoardRole::Mandatory, optional = look.role == BoardRole::Optional;
  switch (style.card) {
    case BoardStyle::Card::Paper: {
      if (mandatory) {
        const Rectangle ring = {card.x - 5, card.y - 5, card.width + 10, card.height + 10};
        drawRoundedLines(ring, 16.0f, 2.5f * px, fade(style.mandatory, a));
      } else if (optional) {
        const Rectangle ring = {card.x - 4, card.y - 4, card.width + 8, card.height + 8};
        drawRoundedLines(ring, 15.0f, 1.6f * px, fade(style.optional, a));
      }
      // the side to move: a chip in the corner
      const Vector2 chip = {card.x + card.width - 14.0f, card.y + card.height - 13.0f};
      DrawCircleV(chip, 4.5f, fade(white ? WHITE : Color{23, 18, 14, 255}, a));
      DrawCircleLinesV(chip, 4.5f, fade(white ? style.muted : style.cardEdgeWhite, a));
      break;
    }
    case BoardStyle::Card::Glow: {
      const Color side = white ? style.cardEdgeWhite : style.cardEdgeBlack;
      if (mandatory) drawRoundedLines(card, 7.0f, 2.4f * px, fade(style.mandatory, a));
      else if (optional) drawRoundedLines(card, 7.0f, 1.8f * px, fade(side, a));
      else drawRoundedLines(card, 7.0f, 1.2f * px, fade(side, 0.55f * a));
      const Vector2 chip = {card.x + card.width - 13.0f, card.y + card.height - 13.0f};
      DrawCircleV(chip, 3.0f, fade(white ? style.glowWhite : style.glowBlack, a));
      break;
    }
    case BoardStyle::Card::Ink: {
      const float w = mandatory ? 2.5f : optional ? 1.6f : 1.0f;
      const Color c = mandatory ? style.mandatory : optional ? style.ink : fade(style.ink, 0.55f);
      DrawRectangleLinesEx(card, w * px, fade(c, a));
      if (mandatory) {
        const float s = 26.0f;
        const Color corner = fade(style.ink, a);
        fillTriangle({card.x + card.width - s, card.y + card.height}, {card.x + card.width, card.y + card.height},
                     {card.x + card.width, card.y + card.height - s}, corner);
      }
      break;
    }
  }
}

void drawBoardOutline(const Rect& board, const BoardStyle& style, float zoom) {
  const Rectangle card = toRay(BoardLayout::cardRect(board));
  const Rectangle ring = {card.x - 4, card.y - 4, card.width + 8, card.height + 8};
  const float radius = style.card == BoardStyle::Card::Ink ? 0.0f : 15.0f;
  const float thickness = worldThickness(3.0f, zoom);
  if (radius > 0) drawRoundedLines(ring, radius, thickness, style.accent2);
  else DrawRectangleLinesEx(ring, thickness, style.accent2);
}

void drawLegalTarget(const Rect& square, bool occupied, float scale, const BoardStyle& style) {
  if (scale <= 0.0f) return;
  const Vector2 center = {square.centerX(), square.centerY()};
  const float a = scale > 1.0f ? 1.0f : scale;
  if (occupied) {
    // Capture: ring around the enemy piece
    DrawRing(center, square.w * 0.40f * scale, square.w * 0.46f * scale, 0, 360, 36, fade(style.check, a));
  } else {
    // Legal empty target
    DrawCircleV(center, square.w * 0.17f * scale, fade(style.targetDot, a));
  }
}

void drawHoverSquare(const Rect& sq, float alpha, const BoardStyle& style) {
  DrawRectangleRec(toRay(sq), fade(UI::withAlpha(style.accent, 80), alpha));
}

void drawSelectedSquare(const Rect& square, float zoom, const BoardStyle& style) {
  // Selected piece: tint plus an outline (the piece itself is drawn lifted on top)
  DrawRectangleRec(toRay(square), style.selectTint);
  DrawRectangleLinesEx(toRay(square), worldThickness(UI::Space::outline, zoom), style.accent2);
}

void drawLiftedPiece(const Rect& sq, const std::string& key, float lift, bool gray) {
  const PieceTextures& tex = App::current().themes.getPieceTextures(key, gray);
  const float up = lift * sq.h * 0.09f;
  const float grow = 1.0f + 0.06f * lift;
  // Soft shadow stays on the ground while the piece rises
  DrawEllipse(static_cast<int>(sq.x + sq.w / 2), static_cast<int>(sq.y + sq.h * 0.9f), sq.w * (0.28f + 0.03f * lift),
              sq.h * 0.06f, fade(UI::Color::shadow, 0.5f + 0.5f * (lift > 1.0f ? 1.0f : lift)));
  const Rectangle dst = {sq.x - sq.w * (grow - 1.0f) / 2, sq.y - up - sq.h * (grow - 1.0f) / 2, sq.w * grow, sq.h * grow};
  DrawTexturePro(*tex.open, {0, 0, static_cast<float>(tex.open->width), static_cast<float>(tex.open->height)}, dst, {0, 0},
                 0.0f, WHITE);
}

void drawFlights(const std::vector<MoveAnimator::Flight>& flights, bool gray) {
  ThemeManager& themes = App::current().themes;
  for (const auto& f : flights) {
    // Captured piece shrinks and fades where it stood (drawn under the mover)
    if (!f.spec.victim.empty() && !f.victimFade.done()) {
      const float p = f.victimFade.progress();
      Texture2D& tex = *themes.getPieceTextures(f.spec.victim, gray).open;
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
      Texture2D& tex = *themes.getPieceTextures(f.spec.piece, gray).open;
      DrawTexturePro(tex, {0, 0, (float)tex.width, (float)tex.height}, {pos.x - s / 2, pos.y - s / 2, s, s}, {0, 0}, 0.0f,
                     WHITE);
    }
  }
}

void drawFocusRing(const Rect& card, const BoardStyle& style, float zoom, float alpha) {
  const Rectangle c = toRay(card);
  const float gap = worldThickness(5.0f, zoom);
  const Rectangle ring = {c.x - gap, c.y - gap, c.width + 2 * gap, c.height + 2 * gap};
  const float thickness = worldThickness(3.0f, zoom);
  if (style.card == BoardStyle::Card::Ink) DrawRectangleLinesEx(ring, thickness, fade(style.accent2, alpha));
  else drawRoundedLines(ring, worldThickness(16.0f, zoom), thickness, fade(style.accent2, alpha));
}

void drawGhostCard(Rectangle where, Vector2 direction, const std::string& label, const BoardStyle& style, float alpha, bool hovered) {
  const bool square = style.hudSquare || style.card == BoardStyle::Card::Ink;
  const float radius = square ? 0.0f : 10.0f;
  Color fill = style.cardWhite;
  fill.a = 235;
  const Rectangle shadow = {where.x + 1, where.y + 3, where.width, where.height};
  if (radius > 0) drawRoundedRect(shadow, radius, fade(style.hudShadow, alpha));
  else DrawRectangleRec(shadow, fade(style.hudShadow, alpha));
  if (radius > 0) drawRoundedRect(where, radius, fade(fill, alpha));
  else DrawRectangleRec(where, fade(fill, alpha));
  const Color edge = hovered ? style.accent : style.accent2;
  if (radius > 0) drawRoundedLines(where, radius, hovered ? 3.0f : 2.0f, fade(edge, alpha));
  else DrawRectangleLinesEx(where, hovered ? 3.0f : 2.0f, fade(edge, alpha));
  const Font font = App::current().assets.font("ui.mono", 14);
  const Vector2 size = MeasureTextEx(font, label.c_str(), 14.0f, 0.0f);
  const float arrowRoom = 22.0f;
  DrawTextEx(font, label.c_str(), {where.x + (where.width - arrowRoom - size.x) / 2.0f, where.y + (where.height - size.y) / 2.0f}, 14.0f,
             0.0f, fade(style.hudText, alpha));
  // A small arrowhead at the right pointing the way the board lies
  const Vector2 c = {where.x + where.width - arrowRoom / 2.0f - 4.0f, where.y + where.height / 2.0f};
  const float len = std::sqrt(direction.x * direction.x + direction.y * direction.y);
  const Vector2 d = len > 0.0001f ? Vector2{direction.x / len, direction.y / len} : Vector2{1.0f, 0.0f};
  const Vector2 n = {-d.y, d.x};
  const float r = 7.0f;
  fillTriangle({c.x + d.x * r, c.y + d.y * r}, {c.x - d.x * r * 0.6f + n.x * r * 0.8f, c.y - d.y * r * 0.6f + n.y * r * 0.8f},
               {c.x - d.x * r * 0.6f - n.x * r * 0.8f, c.y - d.y * r * 0.6f - n.y * r * 0.8f}, fade(edge, alpha));
}

} // namespace play
