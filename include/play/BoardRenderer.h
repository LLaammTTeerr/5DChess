#pragma once
#include <string>
#include <raylib.h>
#include "chess.h"
#include "play/BoardLayout.h"
#include "play/BoardStyle.h"
#include "play/MoveAnimator.h"
#include "play/MultiverseView.h"

namespace play {

inline Rectangle toRay(const Rect& r) { return {r.x, r.y, r.w, r.h}; }

/// Texture key of a piece in the current theme: "white_pawn", "black_queen", ...
const std::string& pieceKey(const Chess::Piece& piece);
/// The key of a piece type of a colour (the promotion picker).
const std::string& pieceKey(Chess::PieceColor color, Chess::PieceType type);

/// A soft rounded halo texture: a card's shadow (Atlas) or glow (Deep space), one nine-patch quad each.
class SoftBox {
public:
  SoftBox();
  ~SoftBox();
  SoftBox(const SoftBox&) = delete;
  SoftBox& operator=(const SoftBox&) = delete;
  /// Draws the halo around `box` (it extends kSpread beyond it) in `tint` (its alpha is the halo's peak).
  void draw(Rectangle box, Color tint) const;
  static constexpr float kSpread = 32.0f;

private:
  Texture2D _texture{};
};

/// How one board is drawn this frame (everything the Board itself does not know).
struct BoardLook {
  float enter = 1.0f;       // 0..1: the board grows from 0.92x and fades in while < 1
  BoardRole role = BoardRole::Past;
  bool inactive = false;    // on an inactive timeline: dimmed, pieces in grey
  bool whiteToMove = true;  // frame / halo by the side to move on this board
  bool blink = false;       // Pixel theme: pieces blink at random intervals (seeded per board)
  unsigned blinkSeed = 0;
  int hiddenCount = 0;      // squares whose piece an overlay draws instead (a flying or lifted piece)
  struct Square { int x = -1, y = -1; } hidden[3];
  void hide(int x, int y) { if (hiddenCount < 3) hidden[hiddenCount++] = {x, y}; }
  int checkedCount = 0;     // king squares a checking attack aims at
  Square checked[4];
  void markChecked(int x, int y) { if (checkedCount < 4) checked[checkedCount++] = {x, y}; }
};

// Everything below draws in world space, i.e. inside BeginMode2D(camera). `zoom` is the camera zoom: outlines keep a
// constant thickness on screen.

/// The halo of a board card (shadow / glow); drawn for all boards before any card so a halo never covers a neighbour.
void drawBoardHalo(const Rect& board, const BoardLook& look, const BoardStyle& style, const SoftBox& soft);
/// One board: card, checkerboard, pieces and the frame that says whether it must / may be moved on.
void drawBoard(const Chess::Board& board, const Rect& area, const BoardLook& look, const BoardStyle& style, float zoom);
/// The outline of the board a piece was picked up on.
void drawBoardOutline(const Rect& board, const BoardStyle& style, float zoom);
/// Legal-target marker: a dot, or a ring around a capturable piece; `scale` animates the pop-in.
void drawLegalTarget(const Rect& square, bool occupied, float scale, const BoardStyle& style);
void drawHoverSquare(const Rect& square, float alpha, const BoardStyle& style);
/// The picked-up piece's square (tint and outline) and the piece itself drawn raised (lift 0..1) over a small shadow.
void drawSelectedSquare(const Rect& square, float zoom, const BoardStyle& style);
void drawLiftedPiece(const Rect& square, const std::string& pieceKey, float lift, bool gray);
/// Pieces in flight: the captured piece fading under the mover.
void drawFlights(const std::vector<MoveAnimator::Flight>& flights, bool gray);
/// A rounded rectangle from a radius in pixels (raylib wants a roundness).
void drawRoundedRect(Rectangle r, float radius, Color fill);
void drawRoundedLines(Rectangle r, float radius, float thickness, Color color);

Color fade(Color c, float alpha);

} // namespace play
