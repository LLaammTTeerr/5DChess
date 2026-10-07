#pragma once
#include <optional>
#include <string>
#include <raylib.h>
#include "chess.h"
#include "play/BoardRenderer.h"
#include "play/BoardStyle.h"
#include "play/MoveAnimator.h"
#include "play/MultiverseView.h"
#include "play/PlayViewLayout.h"
#include "play/Selection.h"

namespace play {

/// What one frame of the Play view needs besides its layout.
struct PlayViewFrame {
  const BoardStyle& style;
  const MultiverseView& view;
  const Chess::IGame& game;
  const SoftBox& soft;
  const MoveAnimator& animator;
  const Selection& selection;
  std::optional<Chess::Core::Coord> hover;     // the square under the pointer (when the boards take input)
  std::optional<Chess::Core::Coord> highlight; // a puzzle's hint square
  std::optional<int> cursor;                   // the timeline whose card the keyboard cursor rings
  bool blink = false;                          // Pixel theme: pieces blink
  bool showChips = true;                       // the player has a turn: cards say MUST MOVE / optional / waiting / moved
};

/// Draws the Play view in screen space (call after the background and before the HUD): cards, history stacks, the inspector, the collapsed
/// inactive row, the time-travel arcs and the pieces in flight. Everything it draws that is a rectangle is registered with the overlap audit.
/// The layout must have been placed for this frame (PlayViewLayout::place). It reuses the multiverse view's board renderer: a card is
/// drawn by drawBoard() under a matrix that maps the board's world rectangle onto the card's screen rectangle.
void drawPlayView(const PlayViewLayout& layout, const PlayViewFrame& frame);

} // namespace play
