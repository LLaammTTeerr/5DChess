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
  /// Put the panel next to `square` (screen rectangle of the promotion square), inside `bounds`: above it, or beside
  /// it when there is no room above. Call every frame while open: the camera may still be moving.
  void place(Rectangle square, Rectangle bounds);
  /// Updates the buttons (they take the pointer, as does the panel) and returns the piece chosen this frame.
  std::optional<Chess::PieceType> update(float dt, bool gray, const ui::Skin* skin);
  void draw(const BoardStyle& style) const;

private:
  static constexpr std::array<Chess::PieceType, 4> kPieces = {Chess::PieceType::Queen, Chess::PieceType::Rook,
                                                              Chess::PieceType::Bishop, Chess::PieceType::Knight};
  bool _open = false;
  Chess::PieceColor _color = Chess::PieceColor::PIECEWHITE;
  Rectangle _panel{};
  std::array<ui::Button, 4> _buttons;
};

} // namespace play
