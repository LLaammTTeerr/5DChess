#pragma once
#include <array>
#include <optional>
#include <raylib.h>
#include "chess.h"
#include "play/BoardStyle.h"
#include "ui/Widgets.h"

namespace play {

/// The small chooser that opens next to a promotion square: Queen, Rook, Bishop and Knight as ui::Buttons showing the
/// piece sprites of the current theme (Q / R / B / N choose from the keyboard). Screen space.
class PromotionPicker {
public:
  PromotionPicker();

  bool isOpen() const { return _open; }
  void show(Chess::PieceColor color);
  void hide() { _open = false; }
  /// Put the panel next to `square` (screen rectangle of the promotion square), inside `bounds`. With the board's `card` (screen
  /// rectangle, empty: unknown) it goes above or below the card, on the side the square is nearest to, so that it covers no square
  /// of the board; a thin line joins it to the square. Without room there it sits beside the square instead. Call every frame while
  /// open: the camera may still be moving.
  void place(Rectangle square, Rectangle bounds, Rectangle card = {});
  /// Updates the buttons (they take the pointer, as does the panel) and returns the piece chosen this frame.
  std::optional<Chess::PieceType> update(float dt, bool gray, const ui::Skin* skin);
  void draw(const BoardStyle& style) const;

private:
  static constexpr std::array<Chess::PieceType, 4> kPieces = {Chess::PieceType::Queen, Chess::PieceType::Rook,
                                                              Chess::PieceType::Bishop, Chess::PieceType::Knight};
  bool _open = false;
  Chess::PieceColor _color = Chess::PieceColor::PIECEWHITE;
  Rectangle _panel{};
  bool _connected = false;        // a line joins the panel to the promotion square
  Vector2 _lineFrom{}, _lineTo{};
  std::array<ui::Button, 4> _buttons;
};

} // namespace play
