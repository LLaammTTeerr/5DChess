#include "play/BoardCamera.h"
#include <algorithm>
#include <cmath>
#include <raymath.h>
#include "Input.h"
#include "Render/Motion.h"

namespace play {

namespace {
constexpr float kPanSmoothTime = 0.30f;  // seconds to (mostly) arrive
constexpr float kZoomSmoothTime = 0.32f;
constexpr float kReducedSmoothTime = 0.06f; // Reduce motion: a quick settle instead of snapping
constexpr float kPadding = 24.0f;           // inside the safe area
constexpr float kReleaseAfter = 10.0f;      // seconds of free camera before it may fly back to the boards
constexpr float kReleaseDistance = 500.0f;  // ... if it is farther than this from them
constexpr float kFarZoom = 0.8f;            // below this zoom the camera moves in on a picked-up piece
constexpr float kMinZoom = 0.1f, kMaxZoom = 5.0f;      // the player's wheel range
constexpr float kMaxFollowZoom = 3.0f;                 // upper clamp of the automatic zoom
constexpr float kMinAutoZoom = 0.12f, kMaxAutoZoom = 2.5f; // range of the fit-all zoom
constexpr float kMinFitZoom = 0.5f, kMaxFitZoom = 1.5f;
}

BoardCamera::BoardCamera() {
  _camera.target = _target;
  _camera.offset = {GetScreenWidth() / 2.0f, GetScreenHeight() / 2.0f};
  _camera.rotation = 0.0f;
  _camera.zoom = 1.0f;
}

void BoardCamera::setInsets(float top, float right, float bottom, float left) {
  _insetTop = top;
  _insetRight = right;
  _insetBottom = bottom;
  _insetLeft = left;
  applySafeArea();
}

Vector2 BoardCamera::safeSize() const {
  return {std::max(100.0f, GetScreenWidth() - _insetLeft - _insetRight),
          std::max(100.0f, GetScreenHeight() - _insetTop - _insetBottom)};
}

// The camera's screen anchor sits at the centre of the safe area, so targets are centred there
void BoardCamera::applySafeArea() {
  _camera.offset = {_insetLeft + safeSize().x / 2.0f, _insetTop + safeSize().y / 2.0f};
}

void BoardCamera::handleInput() {
  if (Input::keyPressed(KEY_Z)) _autoZoom = !_autoZoom;
  if (Input::keyPressed(KEY_X)) {
    _mode = Mode::Fit;
    _timeSinceInput = 0.0f;
  }

  // Drag to pan: only after sustained movement, so a click never nudges the camera
  if (Input::mouseDown(MOUSE_LEFT_BUTTON)) {
    const Vector2 delta = Input::mouseDelta();
    const float length = Vector2Length(delta);
    const float minDragPixels = 5.0f, minDragTime = 0.1f;
    if (length > minDragPixels) {
      if (!_dragging) {
        _dragging = true;
        _dragTime = 0.0f;
      } else {
        _dragTime += Input::frameTime();
      }
      if (_dragTime >= minDragTime && length > 1.0f) {
        pan(Vector2Scale(delta, -1.0f / _camera.zoom));
        _mode = Mode::Free;
        _timeSinceInput = 0.0f;
      }
    }
  } else {
    _dragging = false;
    _dragTime = 0.0f;
  }

  // Wheel zoom: tiny wheel movements are accumulated before they count
  const float wheel = Input::mouseWheel();
  if (wheel != 0) {
    _wheelSum += wheel;
    _wheelTimer = 0.2f;
    if (std::abs(_wheelSum) >= 0.5f) {
      setZoom(_camera.zoom + _wheelSum * 0.1f);
      _mode = Mode::Free;
      _timeSinceInput = 0.0f;
      _wheelSum = 0.0f;
    }
  }
  if (_wheelTimer > 0.0f) {
    _wheelTimer -= Input::frameTime();
    if (_wheelTimer <= 0.0f) _wheelSum = 0.0f;
  }
}

void BoardCamera::setZoom(float zoom) { _camera.zoom = std::max(kMinZoom, std::min(zoom, kMaxZoom)); }

void BoardCamera::pan(Vector2 delta) {
  _camera.target = Vector2Add(_camera.target, delta);
  clampToBounds();
}

void BoardCamera::clampToBounds() {
  _camera.target.x = std::max(_boundsMin.x, std::min(_camera.target.x, _boundsMax.x));
  _camera.target.y = std::max(_boundsMin.y, std::min(_camera.target.y, _boundsMax.y));
}

void BoardCamera::update(float dt, const BoardLayout& layout) {
  applySafeArea();
  const Rect& b = layout.bounds();
  const float margin = BoardLayout::kBoardSize; // the player may pan one board beyond the outermost boards
  _boundsMin = {b.x - margin, b.y - margin};
  _boundsMax = {b.x + b.w + margin, b.y + b.h + margin};
  _center = {b.x + b.w / 2.0f, b.y + b.h / 2.0f};
  if (_autoZoom) fitZoom(b);

  switch (_mode) {
    case Mode::Follow:
      _target = _center;
      followTarget(dt);
      followZoom(dt);
      break;

    case Mode::Free:
      _panVelX = _panVelY = _zoomVel = 0.0f; // the player took over: drop any follow momentum
      _timeSinceInput += dt;
      if (_timeSinceInput >= kReleaseAfter && Vector2Distance(_camera.target, _center) > kReleaseDistance) {
        _mode = Mode::Focus;
        _target = _center;
      }
      break;

    case Mode::Fit:
      followZoom(dt);
      if (std::abs(_camera.zoom - _targetZoom) < 0.05f || (_camera.zoom >= kMinFitZoom && _camera.zoom <= kMaxFitZoom))
        _mode = Mode::Follow;
      break;

    case Mode::Focus:
      followTarget(dt);
      followZoom(dt);
      if (Vector2Distance(_camera.target, _target) < 10.0f) _mode = Mode::Follow;
      break;
  }
}

// Zoom at which all boards fit the safe area
void BoardCamera::fitZoom(const Rect& bounds) {
  const Vector2 safe = safeSize();
  const float zoom = std::min((safe.x - kPadding * 2) / bounds.w, (safe.y - kPadding * 2) / bounds.h);
  _targetZoom = std::max(kMinAutoZoom, std::min(zoom, kMaxAutoZoom));
}

void BoardCamera::followTarget(float dt) {
  const float smooth = UI::Motion::reduced() ? kReducedSmoothTime : kPanSmoothTime;
  _camera.target.x = UI::Motion::smoothDamp(_camera.target.x, _target.x, _panVelX, smooth, dt);
  _camera.target.y = UI::Motion::smoothDamp(_camera.target.y, _target.y, _panVelY, smooth, dt);
  clampToBounds();
}

void BoardCamera::followZoom(float dt) {
  if (!_autoZoom) return;
  if (std::abs(_camera.zoom - _targetZoom) > 0.002f) {
    _camera.zoom = UI::Motion::smoothDamp(_camera.zoom, _targetZoom, _zoomVel,
                                          UI::Motion::reduced() ? kReducedSmoothTime : kZoomSmoothTime, dt);
    _camera.zoom = std::max(kMinZoom, std::min(_camera.zoom, kMaxFollowZoom));
  }
  clampToBounds();
}

// Focusing only moves the camera; the zoom is always fit-all (auto zoom).
void BoardCamera::focusSelected(const Rect& board) {
  if (_camera.zoom >= kFarZoom) return;
  _target = {board.centerX(), board.centerY()};
  _mode = Mode::Focus;
  _timeSinceInput = 0.0f;
}

void BoardCamera::focusNewest(const Rect& board) {
  _target = {board.centerX(), board.centerY()};
  _mode = Mode::Focus;
  _timeSinceInput = 0.0f;
}

} // namespace play
