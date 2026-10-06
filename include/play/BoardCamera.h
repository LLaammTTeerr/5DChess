#pragma once
#include <raylib.h>
#include "play/BoardLayout.h"

namespace play {

/// The 2D camera over the boards. By default it follows the boards: it centres on all of them and zooms to fit them
/// in the safe area (the screen minus the HUD). Dragging or the wheel hands control to the player; focusing a board
/// or the newest board flies to it; Z toggles the auto zoom and X refits. All motion is a critically damped follow,
/// so it is frame-rate independent and never overshoots.
class BoardCamera {
public:
  BoardCamera();

  const Camera2D& view() const { return _camera; }
  Vector2 screenToWorld(Vector2 screen) const { return GetScreenToWorld2D(screen, _camera); }

  /// Reserved screen margins (px) so that content is placed in the free area between the HUD bars.
  void setInsets(float top, float right, float bottom, float left);
  /// Pan (drag), wheel zoom and the Z / X keys.
  void handleInput();
  /// Advance the camera one frame towards what `layout` currently holds.
  void update(float dt, const BoardLayout& layout);

  /// Fly to `board` if the camera is zoomed far out (so a selected piece is comfortable to see).
  void focusSelected(const Rect& board);
  /// Fly to a board that a move just created; `layout` still holds the boards before the move.
  void focusNewest(const BoardLayout& layout, const Rect& board);

private:
  enum class Mode {
    Follow,  // centre on all boards, zoom to fit
    Fit,     // X: zoom only, then back to Follow
    Free,    // the player pans / zooms
    Focus,   // flying to _target
  };

  Camera2D _camera{};
  Vector2 _boundsMin{0, 0}, _boundsMax{5000, 5000}; // pan limits: the boards plus one board of margin
  float _insetTop = 0, _insetRight = 0, _insetBottom = 0, _insetLeft = 0;

  Mode _mode = Mode::Follow;
  Vector2 _target{2500, 2500}, _center{2500, 2500};
  float _targetZoom = 1.0f;
  bool _autoZoom = true;
  float _panVelX = 0, _panVelY = 0, _zoomVel = 0;
  float _timeSinceInput = 0;

  bool _dragging = false;
  float _dragTime = 0, _wheelSum = 0, _wheelTimer = 0;

  Vector2 safeSize() const;
  void applySafeArea();
  void clampToBounds();
  void setZoom(float zoom);
  void pan(Vector2 delta);
  void fitZoom(const Rect& bounds);
  void followTarget(float dt);
  void followZoom(float dt);
};

} // namespace play
