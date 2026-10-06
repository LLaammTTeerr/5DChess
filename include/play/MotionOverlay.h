#pragma once
#include <functional>
#include <string>
#include <vector>
#include <raylib.h>
#include "chess.h"
#include "play/BoardLayout.h"
#include "play/BoardRenderer.h"
#include "play/BoardStyle.h"
#include "play/MoveAnimator.h"
#include "play/MultiverseView.h"

/// The drawing half of MoveAnimator's feedback motion (state in MoveAnimator, numbers in play/Feedback.h). Everything here is an
/// overlay: it paints over what BoardScene / BoardRenderer already drew and never changes how those draw, so the layers that belong to
/// other modules need no hooks. World-space functions are called inside BeginMode2D(camera), screen-space ones outside it.
namespace play::overlay {

/// Draws everything made while it is alive offset by `offset` (world units): a shaking or lifted card.
class BoardShift {
public:
  explicit BoardShift(Vector2 offset);
  ~BoardShift();
  BoardShift(const BoardShift&) = delete;
  BoardShift& operator=(const BoardShift&) = delete;
private:
  bool _active;
};

// ---- Screen space, right after the lanes and before the boards ----
/// A new timeline's lane unfolds: the part of its band that has not unfolded yet is painted over with the background again
/// (`paintBackground` draws the view's background). The lane grows from the side of its parent lane.
void drawLaneUnfold(const std::vector<MoveAnimator::LaneUnfold>& lanes, const MultiverseView& view, const Camera2D& camera,
                    const std::function<void()>& paintBackground);

// ---- World space, before the boards (with the halos) ----
/// The glow of a board (the target of the live arc, the source of the last time-travel move).
void drawBoardGlow(const Rect& board, float alpha, const BoardStyle& style, const SoftBox& soft, bool source);

// ---- World space, over the boards ----
void drawRejectFlash(const Rect& square, float alpha);
/// Targets of the piece the pointer rests on: hollow dots, and a dashed arc to every other board they lie on.
void drawMovePreview(const MoveAnimator::PreviewLayer& layer, const Chess::IGame& game, const BoardStyle& style, float zoom,
                     bool arcs);
/// The arc of a time-travel move to the hovered target, drawn up to `grow` of its length.
void drawLiveArc(const MoveAnimator::LiveArc& arc, int dim, const BoardStyle& style, float zoom);
/// The attacked king's square pulses (pulse 0..1).
void drawCheckPulse(const std::vector<Chess::Core::Coord>& kings, float pulse, int dim, const BoardStyle& style, float zoom);

// ---- Screen space, over everything of the board view ----
/// The column and the lane of the hovered time-travel target light up in the ruler and the lane labels.
void drawTargetRuler(const Chess::Core::Coord& target, float alpha, const Camera2D& camera, const BoardStyle& style);
/// The reason for a rejected click, in red, just below the turn ruler.
void drawHintAlert(const std::string& text, float alpha, const BoardStyle& style);

} // namespace play::overlay
