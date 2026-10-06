#include "play/PromotionPicker.h"
#include <algorithm>
#include <cmath>
#include "App.h"
#include "Input.h"
#include "Render/PieceTheme.h"
#include "Render/UITheme.h"
#include "play/BoardRenderer.h"

namespace play {

namespace {
constexpr float kButton = 56.0f, kGap = 6.0f, kPad = 8.0f, kOffset = 10.0f;
const char* const kNames[] = {"Queen", "Rook", "Bishop", "Knight"};
}

PromotionPicker::PromotionPicker() {
  for (size_t i = 0; i < _buttons.size(); ++i) _buttons[i].label = kNames[i];
}

void PromotionPicker::show(Chess::PieceColor color) {
  _open = true;
  _color = color;
}

void PromotionPicker::place(Rectangle square, Rectangle bounds, Rectangle card) {
  const float w = 4 * kButton + 3 * kGap + 2 * kPad, h = kButton + 2 * kPad;
  float x = square.x + square.width / 2 - w / 2;
  float y = 0.0f;
  _connected = false;
  bool placed = false;
  if (card.width > 0.0f) {
    // Beside the board, not on it: above the card when the square is nearer its top, below it when nearer its bottom
    const float mid = square.y + square.height / 2;
    const bool nearTop = mid - card.y < card.y + card.height - mid;
    const float above = card.y - h - kOffset, below = card.y + card.height + kOffset;
    const bool fitsAbove = above >= bounds.y, fitsBelow = below + h <= bounds.y + bounds.height;
    // (no room on the near side: beside the square below, never on the far side of the board)
    if (nearTop && fitsAbove) {
      y = above;
      placed = true;
    } else if (!nearTop && fitsBelow) {
      y = below;
      placed = true;
    }
    if (placed) {
      const float lx = std::clamp(square.x + square.width / 2, x + 14.0f, x + w - 14.0f);
      _connected = true;
      _lineFrom = {lx, y < card.y ? y + h : y};
      _lineTo = {lx, y < card.y ? square.y : square.y + square.height};
      _connected = std::fabs(_lineTo.y - _lineFrom.y) < 90.0f * std::max(1.0f, square.width / 31.0f); // not across a whole board
    }
  }
  if (!placed) {
    y = square.y - h - kOffset;                                      // above the square
    if (y < bounds.y) { // no room above: beside the square, so that neither the square nor the pawn below it is covered
      x = square.x + square.width + kOffset;
      y = square.y + square.height / 2 - h / 2;
      if (x + w > bounds.x + bounds.width - 6.0f) x = square.x - kOffset - w; // no room on the right either
    }
  }
  x = std::clamp(x, bounds.x + 6.0f, std::max(bounds.x + 6.0f, bounds.x + bounds.width - w - 6.0f));
  y = std::clamp(y, bounds.y, std::max(bounds.y, bounds.y + bounds.height - h));
  _panel = {std::floor(x), std::floor(y), w, h};
  if (_connected) { // the line follows the panel as it was finally placed
    const float lx = std::clamp(square.x + square.width / 2, _panel.x + 14.0f, _panel.x + _panel.width - 14.0f);
    const bool panelAbove = _panel.y + _panel.height <= square.y + 1.0f;
    _lineFrom = {lx, panelAbove ? _panel.y + _panel.height : _panel.y};
    _lineTo = {lx, panelAbove ? square.y : square.y + square.height};
    _connected = std::fabs(_lineTo.y - _lineFrom.y) < 90.0f * std::max(1.0f, square.width / 31.0f);
  }
  for (size_t i = 0; i < _buttons.size(); ++i)
    _buttons[i].rect = {_panel.x + kPad + static_cast<float>(i) * (kButton + kGap), _panel.y + kPad, kButton, kButton};
}

std::optional<Chess::PieceType> PromotionPicker::update(float dt, bool gray, const ui::Skin* skin) {
  if (!_open) return std::nullopt;
  ThemeManager& themes = App::current().themes;
  std::optional<Chess::PieceType> chosen;
  for (size_t i = 0; i < _buttons.size(); ++i) {
    _buttons[i].skin = skin;
    _buttons[i].icon = themes.getPieceTextures(pieceKey(_color, kPieces[i]), gray).open;
    if (_buttons[i].update(dt) && !chosen) chosen = kPieces[i];
  }
  const Vector2 mouse = Input::mousePosition();
  if (CheckCollisionPointRec(mouse, _panel)) ui::consumePointer(); // the panel is no click on the board behind it
  static constexpr int keys[4] = {KEY_Q, KEY_R, KEY_B, KEY_N};
  for (size_t i = 0; i < 4 && !chosen; ++i)
    if (Input::keyPressed(keys[i])) chosen = kPieces[i];
  return chosen;
}

void PromotionPicker::draw(const BoardStyle& style) const {
  if (!_open) return;
  if (_connected) DrawLineEx(_lineFrom, _lineTo, 1.0f, style.hudBorder);
  if (style.hudSquare) {
    DrawRectangleRec(_panel, style.hudFill);
    DrawRectangleLinesEx(_panel, 1.0f, style.hudBorder);
  } else {
    if (style.card == BoardStyle::Card::Glow)
      for (int k = 3; k >= 1; --k)
        DrawRectangleRounded({_panel.x - k * 4.0f, _panel.y - k * 4.0f, _panel.width + k * 8.0f, _panel.height + k * 8.0f}, 0.3f, 10,
                             UI::withAlpha(style.hudShadow, static_cast<unsigned char>(style.hudShadow.a * 0.22f / k)));
    else
      DrawRectangleRounded({_panel.x + 2, _panel.y + 3, _panel.width, _panel.height}, 0.3f, 10, style.hudShadow);
    DrawRectangleRounded(_panel, 0.3f, 10, style.hudFill);
    DrawRectangleRoundedLinesEx(_panel, 0.3f, 10, 1.0f, style.hudBorder);
  }
  for (const auto& b : _buttons) b.draw();
}

} // namespace play
