#include "play/BoardCamera.h"
#include <algorithm>
#include <cmath>
#include "Render/Motion.h" // the inline easing functions only: this file links without graphics

namespace play {

namespace {
constexpr float kSteps[] = {0.6f, 0.8f, 1.0f, 1.25f};
constexpr float kFlingDecay = 0.12f; // seconds: the coast after a drag
constexpr float kMinFitZoom = 0.12f; // the lowest zoom a fit may choose

Rect unite(const Rect& a, const Rect& b) {
  const float x0 = std::min(a.x, b.x), y0 = std::min(a.y, b.y);
  const float x1 = std::max(a.x + a.w, b.x + b.w), y1 = std::max(a.y + a.h, b.y + b.h);
  return {x0, y0, x1 - x0, y1 - y0};
}
} // namespace

float BoardCamera::snapZoom(float zoom) {
  if (zoom >= kSteps[3]) return zoom;
  float best = kSteps[0], bestDistance = 1e9f;
  for (float step : kSteps) {
    const float d = std::fabs(std::log(zoom / step));
    if (d < bestDistance) {
      bestDistance = d;
      best = step;
    }
  }
  return best;
}

float BoardCamera::snapDown(float zoom) {
  float best = kSteps[0];
  for (float step : kSteps)
    if (step <= zoom * 1.0001f) best = step;
  return best;
}

void BoardCamera::setViewport(float width, float height) {
  if (width == _viewW && height == _viewH) return;
  _viewW = width;
  _viewH = height;
  reframe();
}

void BoardCamera::setInsets(float top, float right, float bottom, float left) {
  if (top == _insetTop && right == _insetRight && bottom == _insetBottom && left == _insetLeft) return;
  _insetTop = top;
  _insetRight = right;
  _insetBottom = bottom;
  _insetLeft = left;
  reframe();
}

void BoardCamera::setWorldBounds(const Rect& bounds) {
  const float margin = BoardLayout::kBoardSize;
  _bounds = {bounds.x - margin, bounds.y - margin, bounds.w + 2 * margin, bounds.h + 2 * margin};
  _hasBounds = true;
}

Vec2 BoardCamera::safeSize() const {
  return {std::max(100.0f, _viewW - _insetLeft - _insetRight), std::max(100.0f, _viewH - _insetTop - _insetBottom)};
}

Vec2 BoardCamera::offset() const { return {_insetLeft + safeSize().x / 2.0f, _insetTop + safeSize().y / 2.0f}; }

Vec2 BoardCamera::screenToWorld(Vec2 s) const {
  const Vec2 o = offset();
  return {(s.x - o.x) / _zoom + _target.x, (s.y - o.y) / _zoom + _target.y};
}

Vec2 BoardCamera::worldToScreen(Vec2 w) const {
  const Vec2 o = offset();
  return {(w.x - _target.x) * _zoom + o.x, (w.y - _target.y) * _zoom + o.y};
}

Rect BoardCamera::visibleWorld() const {
  const Vec2 a = screenToWorld({_insetLeft, _insetTop});
  const Vec2 b = screenToWorld({_viewW - _insetRight, _viewH - _insetBottom});
  return {a.x, a.y, b.x - a.x, b.y - a.y};
}

bool BoardCamera::isBoardVisible(const Rect& card, float fraction) const {
  const Rect v = visibleWorld();
  const float w = std::min(card.x + card.w, v.x + v.w) - std::max(card.x, v.x);
  const float h = std::min(card.y + card.h, v.y + v.h) - std::max(card.y, v.y);
  if (w <= 0 || h <= 0) return false;
  return w * h >= fraction * card.w * card.h - 1e-3f;
}

const char* BoardCamera::stateLabel() const {
  switch (_state) {
    case State::Overview: return "Overview";
    case State::Focus: return "Focus";
    case State::Free: return "Free";
  }
  return "";
}

float BoardCamera::fitZoom(const Rect& r) const {
  const Vec2 safe = safeSize();
  return std::min((safe.x - kPadding * 2) / std::max(1.0f, r.w), (safe.y - kPadding * 2) / std::max(1.0f, r.h));
}

float BoardCamera::clampWheel(float current, float wanted) {
  return std::clamp(wanted, std::min(kMinZoom, current), std::max(kMaxZoom, current));
}

void BoardCamera::clampTarget() {
  if (!_hasBounds) return;
  _target.x = std::clamp(_target.x, _bounds.x, _bounds.x + _bounds.w);
  _target.y = std::clamp(_target.y, _bounds.y, _bounds.y + _bounds.h);
}

void BoardCamera::goTo(Vec2 target, float zoom, float duration, bool easeInOut, bool snap) {
  _fling = {};
  if (snap || !_seeded) {
    _seeded = true;
    _tween.active = false;
    _target = target;
    _zoom = zoom;
    return;
  }
  Tween& t = _tween;
  t.active = true;
  t.from = _target;
  t.to = target;
  t.fromLog = std::log(_zoom);
  t.toLog = std::log(zoom);
  t.t = 0.0f;
  t.linear = _reduced;
  t.easeInOut = easeInOut;
  t.duration = _reduced ? kReducedTime : duration;
}

void BoardCamera::cancel() {
  if (_tween.active) {
    _tween.active = false;
    _state = State::Free;
  }
  _fling = {};
}

void BoardCamera::finishTween() {
  if (!_tween.active) return;
  _target = _tween.to;
  _zoom = std::exp(_tween.toLog);
  _tween.active = false;
}

void BoardCamera::update(float dt) {
  if (_tween.active) {
    Tween& t = _tween;
    t.t += dt / t.duration;
    const float u = std::min(1.0f, t.t);
    const float e = t.linear ? u : (t.easeInOut ? UI::Motion::easeInOutCubic(u) : UI::Motion::easeOutCubic(u));
    _target = {t.from.x + (t.to.x - t.from.x) * e, t.from.y + (t.to.y - t.from.y) * e};
    _zoom = std::exp(t.fromLog + (t.toLog - t.fromLog) * e);
    if (u >= 1.0f) finishTween();
  }
  if (_fling.x != 0.0f || _fling.y != 0.0f) {
    pan({_fling.x * dt, _fling.y * dt});
    const float k = std::exp(-dt / kFlingDecay);
    _fling = {_fling.x * k, _fling.y * k};
    if (std::fabs(_fling.x) + std::fabs(_fling.y) < 20.0f) _fling = {};
  }
}

// The window or the insets changed: the same framing again for the new free area (a jump, not a tween)
void BoardCamera::reframe() {
  if (!_seeded || _state == State::Free) return;
  const State state = _state;
  const Rect frame = _frame;
  float z = std::clamp(fitZoom(frame), kMinFitZoom, kMaxOverviewZoom);
  if (state == State::Overview) z = std::max(z, _frameMinZoom);
  else z = std::min(_zoom, z);
  goTo({frame.centerX(), frame.centerY()}, z, 0.0f, false, true);
  _state = state;
  _frame = frame;
}

// ---- framing requests ----------------------------------------------------------------------------------------------------------

bool BoardCamera::overview(const Rect& all, const Rect& present, bool automatic, bool snap) {
  if (automatic && _locked) return false;
  Rect region = all;
  float z = fitZoom(all);
  if (z < kFarOverviewZoom && present.w > 0 && present.h > 0) {
    region = present;
    z = fitZoom(present);
  }
  z = std::clamp(z, kMinFitZoom, kMaxOverviewZoom);
  goTo({region.centerX(), region.centerY()}, z, kOverviewTime, true, snap);
  _state = State::Overview;
  _frame = region;
  _frameMinZoom = 0.0f;
  return true;
}

bool BoardCamera::fitRegion(const Rect& region, float minZoom, bool automatic) {
  if (automatic && _locked) return false;
  const float z = std::clamp(fitZoom(region), minZoom, kMaxOverviewZoom);
  goTo({region.centerX(), region.centerY()}, z, kOverviewTime, true);
  _state = State::Overview;
  _frame = region;
  _frameMinZoom = minZoom;
  return true;
}

bool BoardCamera::focusBoard(const Rect& card, float minZoom, bool automatic, float duration) {
  if (automatic && _locked) return false;
  float z = std::max(minZoom, snapZoom(_zoom));
  z = std::min(z, std::max(fitZoom(card), kMinFitZoom)); // a card always fits the free area
  goTo({card.centerX(), card.centerY()}, z, duration, false);
  _state = State::Focus;
  _frame = card;
  return true;
}

bool BoardCamera::showBoth(const Rect& from, const Rect& to, float minZoom, bool automatic) {
  if (automatic && _locked) return false;
  const Rect both = unite(from, to);
  const float fit = fitZoom(both);
  float z = fit >= _zoom ? _zoom : snapDown(fit); // never zoom in on the player; zoom out onto a step
  Rect centred = both;
  if (z < minZoom) { // the pair does not fit at a readable zoom: the new board matters
    z = minZoom;
    centred = to;
  }
  goTo({centred.centerX(), centred.centerY()}, z, kFocusTime, false);
  _state = State::Focus;
  _frame = centred;
  return true;
}

bool BoardCamera::reveal(const Rect& card, bool automatic) {
  if (automatic && _locked) return false;
  const Rect v = visibleWorld();
  const float pad = 12.0f / _zoom;
  Vec2 t = _target;
  auto fit = [&](float lo, float size, float vLo, float vSize, float& centre) {
    if (size + 2 * pad > vSize) centre = lo + size / 2.0f; // larger than the view: centre it
    else if (lo - pad < vLo) centre -= vLo - (lo - pad);
    else if (lo + size + pad > vLo + vSize) centre += (lo + size + pad) - (vLo + vSize);
  };
  fit(card.x, card.w, v.x, v.w, t.x);
  fit(card.y, card.h, v.y, v.h, t.y);
  if (t.x == _target.x && t.y == _target.y) return true;
  const State state = _state;
  const Rect frame = _frame;
  goTo(t, _zoom, kHopTime, false);
  _state = state;
  _frame = frame;
  return true;
}

bool BoardCamera::peek(const Rect& card, float fraction, bool automatic) {
  if (automatic && _locked) return false;
  if (isBoardVisible(card, 0.5f)) return false;
  const Vec2 t{_target.x + (card.centerX() - _target.x) * fraction, _target.y + (card.centerY() - _target.y) * fraction};
  const State state = _state;
  const Rect frame = _frame;
  goTo(t, _zoom, kHopTime, false);
  _state = state;
  _frame = frame;
  return true;
}

// ---- the player's motion -------------------------------------------------------------------------------------------------------

void BoardCamera::pan(Vec2 d) {
  cancel();
  _state = State::Free;
  _target.x -= d.x / _zoom;
  _target.y -= d.y / _zoom;
  clampTarget();
}

void BoardCamera::zoomAt(Vec2 p, float factor) {
  cancel();
  _state = State::Free;
  const Vec2 anchor = screenToWorld(p);
  _zoom = clampWheel(_zoom, _zoom * factor);
  const Vec2 o = offset();
  _target = {anchor.x - (p.x - o.x) / _zoom, anchor.y - (p.y - o.y) / _zoom};
  clampTarget();
}

void BoardCamera::wheel(Vec2 p, float notches) { zoomAt(p, std::pow(kWheelStep, notches)); }

void BoardCamera::fling(Vec2 v) {
  if (_reduced) return;
  _fling = v;
}

} // namespace play
