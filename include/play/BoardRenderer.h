#pragma once
#include <string>
#include <raylib.h>
#include "chess.h"
#include "play/BoardLayout.h"
#include "play/MoveAnimator.h"

namespace play {

inline Rectangle toRay(const Rect& r) { return {r.x, r.y, r.w, r.h}; }

/// Texture key of a piece in the current theme: "white_pawn", "black_queen", ...
const std::string& pieceKey(const Chess::Piece& piece);

/// How one board is drawn this frame (everything the Board itself does not know).
struct BoardLook {
  float enter = 1.0f;       // 0..1: the board grows from 0.92x and fades in while < 1
  bool moveable = false;    // accent border: the side to move may still move from this board
  bool blink = false;       // Pixel theme: pieces blink at random intervals (seeded per board)
  unsigned blinkSeed = 0;
  int hiddenCount = 0;      // squares whose piece an overlay draws instead (a flying or lifted piece)
  struct Square { int x = -1, y = -1; } hidden[3];
  void hide(int x, int y) { if (hiddenCount < 3) hidden[hiddenCount++] = {x, y}; }
};

// Everything below draws in world space, i.e. inside BeginMode2D(camera). `zoom` is the camera zoom: outlines keep a
// constant thickness on screen.

/// One board: checkerboard, pieces and border.
void drawBoard(const Chess::Board& board, const Rect& area, const BoardLook& look, float zoom);
/// The outline of the board a piece was picked up on.
void drawBoardOutline(const Rect& area, float zoom);
/// Legal-target marker: a dot, or a ring around a capturable piece; `scale` animates the pop-in.
void drawLegalTarget(const Rect& square, bool occupied, float scale);
void drawHoverSquare(const Rect& square, float alpha);
/// The picked-up piece's square (tint and outline) and the piece itself drawn raised (lift 0..1) over a small shadow.
void drawSelectedSquare(const Rect& square, float zoom);
void drawLiftedPiece(const Rect& square, const std::string& pieceKey, float lift);
/// Pieces in flight: the captured piece fading under the mover.
void drawFlights(const std::vector<MoveAnimator::Flight>& flights);
/// The vertical "present" line at a half-turn, spanning the boards' vertical extent.
void drawPresentLine(float halfTurn, const Rect& boardsBounds, float zoom);

} // namespace play
