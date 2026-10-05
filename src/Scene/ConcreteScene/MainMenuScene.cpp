#include "App.h"
#include "MainMenuScene.h"
#include "Render/UITheme.h"
#include "Render/Motion.h"
#include <cmath>
#include <iostream>
#include <string>
#include <raylib.h>

namespace {

// Small integer hash for the deterministic, scrolling board pattern
unsigned hash2(int a, int b) {
  unsigned h = static_cast<unsigned>(a) * 73856093u ^ static_cast<unsigned>(b) * 19349663u;
  h ^= h >> 13; h *= 0x5bd1e995u; h ^= h >> 15;
  return h;
}

// One parallax layer of the timeline field: rows of tiny boards drifting left, joined by thin lines.
struct FieldLayer {
  float size;       // board edge (px)
  float gap;        // horizontal gap between boards
  float rowPitch;   // distance between rows
  float speed;      // px/s leftwards
  float strength;   // 0..1 contrast multiplier
  int   squares;    // checker squares per edge (4 or 8)
  float yShift;
};

void drawField(const FieldLayer& L, float time, float screenW, float screenH) {
  const float pitch = L.size + L.gap;
  const float scroll = time * L.speed;
  const int firstCol = static_cast<int>(std::floor(scroll / pitch));
  const float frac = scroll - firstCol * pitch;
  const int cols = static_cast<int>(screenW / pitch) + 3;
  const int rows = static_cast<int>(screenH / L.rowPitch) + 2;

  const Color lineCol = UI::withAlpha(UI::Color::border, static_cast<unsigned char>(150 * L.strength));
  const Color branchCol = UI::withAlpha(UI::Color::accent, static_cast<unsigned char>(70 * L.strength));
  const Color fillCol = UI::withAlpha(UI::Color::surfaceAlt, static_cast<unsigned char>(210 * L.strength));
  const Color edgeCol = UI::withAlpha(UI::Color::border, static_cast<unsigned char>(190 * L.strength));
  const Color darkCol = UI::withAlpha(UI::Color::squareDark, static_cast<unsigned char>(48 * L.strength));
  const float sq = L.size / L.squares;

  for (int r = 0; r < rows; ++r) {
    const float y = L.yShift + r * L.rowPitch;
    for (int c = 0; c < cols; ++c) {
      const int k = firstCol + c;                // global column: stable while the sheet scrolls
      const float x = c * pitch - frac - pitch;
      if (hash2(r, k) % 11 == 0) continue;       // a missing board breaks the timeline
      // Timeline line to the next board in the row
      if (hash2(r, k + 1) % 11 != 0)
        DrawLineEx({x + L.size, y + L.size / 2}, {x + pitch, y + L.size / 2}, 1.0f, lineCol);
      // Branch: a fork curving down into the next row
      if (r + 1 < rows && hash2(r, k) % 4 == 1 && hash2(r + 1, k + 1) % 11 != 0) {
        const Vector2 a = {x + L.size, y + L.size / 2};
        const Vector2 b = {x + pitch, y + L.rowPitch + L.size / 2};
        DrawSplineSegmentBezierCubic(a, {a.x + L.gap * 0.7f, a.y}, {b.x - L.gap * 0.7f, b.y}, b, 1.0f, branchCol);
      }
      DrawRectangleRec({x, y, L.size, L.size}, fillCol);
      for (int i = 0; i < L.squares; ++i)
        for (int j = 0; j < L.squares; ++j)
          if ((i + j) % 2 == 1) DrawRectangleRec({x + i * sq, y + j * sq, sq, sq}, darkCol);
      DrawRectangleLinesEx({x, y, L.size, L.size}, 1.0f, edgeCol);
    }
  }
}

constexpr const char* kPieces[] = {"king", "queen", "rook", "bishop", "knight", "pawn"};

} // namespace

void MainMenuScene::init(void) {}

void MainMenuScene::handleInput(void) {}

void MainMenuScene::update(float deltaTime) {
  const float dt = UI::Motion::safeDt(deltaTime);
  // The drifting field, the bob and the blink all freeze under Reduce motion
  if (!UI::Motion::reduced()) _time += dt;
  _enter.update(dt);
  _enterClock += dt;
}

void MainMenuScene::render() {
  ClearBackground(UI::Color::bg);
  UI::Cursor::beginFrame();

  const float W = static_cast<float>(GetScreenWidth());
  const float H = static_cast<float>(GetScreenHeight());
  const bool reduced = UI::Motion::reduced();

  // ---- Background: two parallax layers of tiny boards laid out like timelines ----
  drawField({28.0f, 30.0f, 84.0f, 3.5f, 0.55f, 4, 20.0f}, _time, W, H);
  drawField({52.0f, 44.0f, 132.0f, 7.0f, 1.0f, 8, 54.0f}, _time, W, H);

  // ---- Hero block, centred in the area right of the navigation column ----
  const float navW = 300.0f;
  const float cx = navW + (W - navW) / 2.0f;
  const float blockTop = std::floor(H / 2.0f - 175.0f);

  // Soft halo keeps the title readable over the field
  DrawCircleGradient(static_cast<int>(cx), static_cast<int>(blockTop + 120), 430.0f,
                     UI::withAlpha(UI::Color::bg, 235), UI::withAlpha(UI::Color::bg, 0));

  const float enter = _enter.progress();                      // 0..1
  const float rise = reduced ? 0.0f : (1.0f - enter) * 14.0f;
  const unsigned char a = static_cast<unsigned char>(255.0f * enter);

  // Title: "5D" in the accent, "Chess" in ink
  const ::Font heroFont = UI::Fonts::hero();
  const float fs = static_cast<float>(UI::Font::hero);
  const Vector2 w5d = MeasureTextEx(heroFont, "5D", fs, 0.0f);
  const Vector2 wChess = MeasureTextEx(heroFont, "Chess", fs, 0.0f);
  const float titleX = std::floor(cx - (w5d.x + wChess.x) / 2.0f);
  const float titleY = std::floor(blockTop + rise);
  DrawTextEx(heroFont, "5D", {titleX, titleY}, fs, 0.0f, UI::withAlpha(UI::Color::primary, a));
  DrawTextEx(heroFont, "Chess", {titleX + w5d.x, titleY}, fs, 0.0f, UI::withAlpha(UI::Color::text, a));

  UI::drawTextCentered(UI::Fonts::subtitle(), "Chess with multiverse time travel", cx,
                       blockTop + 112.0f + rise, static_cast<float>(UI::Font::subtitle),
                       UI::withAlpha(UI::Color::textMuted, a));

  // ---- Pixel creatures: one of each piece, both colours, idling ----
  constexpr int kCount = 12;
  const float sprite = 64.0f, innerGap = 6.0f, outerGap = 28.0f;
  const float rowW = kCount * sprite + 6 * innerGap + 5 * outerGap;
  float x = std::floor(cx - rowW / 2.0f);
  const float baseY = blockTop + 218.0f;
  Assets& assets = App::current().assets;
  for (int i = 0; i < kCount; ++i) {
    const int pieceIdx = i / 2;
    const bool white = (i % 2) == 0;
    const std::string name = std::string(white ? "white_" : "black_") + kPieces[pieceIdx];
    const bool closed = UI::Motion::blinkClosed(static_cast<unsigned>(i + 1) * 7919u, _time);
    Texture2D& tex = assets.texture("piece.pixel." + name + (closed ? "_blink" : ""));

    // Staggered pop-in (30-50 ms per item), then a gentle two-pixel bob
    const float dur = reduced ? UI::Motion::fast : UI::Motion::base;
    const float delay = reduced ? 0.0f : 0.25f + i * UI::Motion::stagger;
    const float p = UI::Motion::easeOutCubic(UI::Motion::clamp01((_enterClock - delay) / dur));
    const float bob = reduced ? 0.0f : std::round(std::sin(_time * 2.4f + i * 0.9f) * 2.0f);
    const float lift = reduced ? 0.0f : (1.0f - p) * 12.0f;
    const unsigned char ca = static_cast<unsigned char>(255.0f * p);

    DrawEllipse(static_cast<int>(x + sprite / 2), static_cast<int>(baseY + sprite - 2), 22.0f, 5.0f,
                UI::withAlpha(UI::Color::shadow, static_cast<unsigned char>(UI::Color::shadow.a * p)));
    DrawTexturePro(tex, {0, 0, static_cast<float>(tex.width), static_cast<float>(tex.height)},
                   {x, baseY + bob + lift, sprite, sprite}, {0, 0}, 0.0f, UI::withAlpha(WHITE, ca));
    x += sprite + ((i % 2 == 0) ? innerGap : outerGap);
  }
}

void MainMenuScene::cleanup(void) {}

bool MainMenuScene::isActive(void) const { return _isActive; }

std::string MainMenuScene::getName(void) const { return "MainMenuScene"; }

std::string MainMenuScene::getGameStateName(void) const {
  return "MAIN_MENU";
}

void MainMenuScene::onEnter() {
  _isActive = true;
  _enterClock = 0.0f;
  _enter.start(0.0f, 1.0f, UI::Motion::slow, UI::Motion::easeOutCubic, 0.0f, true);
}

void MainMenuScene::onExit() { _isActive = false; }

bool MainMenuScene::shouldTransition() const { return false; }
