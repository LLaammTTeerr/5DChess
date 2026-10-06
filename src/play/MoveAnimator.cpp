#include "play/MoveAnimator.h"
#include <algorithm>
#include <cmath>
#include <raymath.h>

namespace play {

using namespace UI::Motion;

MoveAnimator::MoveAnimator() {
  _lift.init(0.0f, 520.0f, 0.55f); // slightly underdamped: a small pop when a piece is picked up
  _known.reserve(64);
  _scratch.reserve(64);
}

void MoveAnimator::update(float dt, const std::optional<Chess::Core::Coord>& hover) {
  for (auto& f : _flights) { f.move.update(dt); f.victimFade.update(dt); }
  _flights.erase(std::remove_if(_flights.begin(), _flights.end(),
                                [](const Flight& f) { return f.move.done() && f.victimFade.done(); }),
                 _flights.end());
  for (auto& e : _enters) e.t.update(dt);
  _enters.erase(std::remove_if(_enters.begin(), _enters.end(), [](const Enter& e) { return e.t.done(); }), _enters.end());

  _lift.update(dt);
  _dotClock += dt;

  // Hover tint: the square under the pointer fades in, the one just left fades out (fast)
  const float step = dt / fast;
  if (hover) {
    const BoardKey key = keyOf(*hover);
    if (_hoverCur.x == hover->x && _hoverCur.y == hover->y && _hoverCur.key == key) {
      _hoverCur.alpha = std::fmin(1.0f, _hoverCur.alpha + step);
    } else {
      if (_hoverCur.alpha > 0.01f) _hoverOld = _hoverCur;
      _hoverCur = {key, hover->x, hover->y, std::fmin(1.0f, step)};
    }
  } else if (_hoverCur.alpha > 0.0f) {
    _hoverOld = _hoverCur;
    _hoverCur = Hover{};
  }
  _hoverOld.alpha = std::fmax(0.0f, _hoverOld.alpha - step);
  if (reduced()) { _hoverCur.alpha = hover ? 1.0f : 0.0f; _hoverOld.alpha = 0.0f; }
}

void MoveAnimator::syncBoards(const BoardLayout& layout) {
  _scratch.clear();
  for (const auto& slot : layout.boards()) _scratch.push_back({slot.timeline, slot.halfTurn});
  std::sort(_scratch.begin(), _scratch.end());

  if (_seeded) {
    // Boards that exist now but did not before were just created by a move: grow them in
    for (const auto& k : _scratch) {
      if (std::binary_search(_known.begin(), _known.end(), k)) continue;
      Enter e{k, {}};
      e.t.start(0.0f, 1.0f, base, easeOutCubic, 0.0f, true); // under Reduce motion: a plain fade
      if (!e.t.done()) _enters.push_back(e);
    }
  }
  // _known is rebuilt from the boards that exist now, so keys of undone boards are dropped and a replayed move onto
  // the same key grows in again; same for any in-flight grow-in of a vanished board.
  _enters.erase(std::remove_if(_enters.begin(), _enters.end(), [&](const Enter& e) {
    return !std::binary_search(_scratch.begin(), _scratch.end(), e.key); }), _enters.end());
  _known.swap(_scratch);
  _seeded = true;
}

void MoveAnimator::startFlight(const FlightSpec& spec) {
  if (reduced()) return; // pieces just appear
  Flight f;
  f.spec = spec;
  const Rect src = BoardLayout::squareRect(BoardLayout::boardRect(spec.from.first, spec.from.second), spec.dim, spec.fromX, spec.fromY);
  const Rect dst = BoardLayout::squareRect(BoardLayout::boardRect(spec.to.first, spec.to.second), spec.dim, spec.toX, spec.toY);
  f.from = {src.centerX(), src.centerY()};
  f.to = {dst.centerX(), dst.centerY()};
  f.size = dst.w;
  if (spec.from != spec.to) {
    // Gentle arc across boards / through time
    const float dist = Vector2Distance(f.from, f.to);
    f.arcHeight = std::fmin(140.0f, 24.0f + dist * 0.12f);
    f.move.start(0.0f, 1.0f, slow, easeInOutCubic);
  } else {
    f.move.start(0.0f, 1.0f, base, easeOutCubic);
  }
  if (!spec.victim.empty()) f.victimFade.start(0.0f, 1.0f, fast, easeInCubic);
  _flights.push_back(std::move(f));
}

void MoveAnimator::finish() {
  _flights.clear();
  _enters.clear();
}

void MoveAnimator::select(const std::optional<Chess::Core::Coord>& from, const std::vector<Chess::Core::Coord>& targets, int dim) {
  const bool changed = from && (!_selected || !(*_selected == *from));
  _selected = from;
  if (!from) _lift.snap(0.0f);
  else if (changed) { _lift.snap(0.0f); _lift.setTarget(1.0f); } // pick-up: the spring lifts the piece

  // Targets pop in with a 30 ms stagger per ring of squares away from the picked-up piece (capped at 10 rings)
  _dotDelay.assign(targets.size(), 0.0f);
  _dotClock = 0.0f;
  if (from && !targets.empty()) {
    const Rect fromSq = BoardLayout::squareRect(BoardLayout::boardRect(from->l, from->t), dim, from->x, from->y);
    const Vector2 fc = {fromSq.centerX(), fromSq.centerY()};
    for (size_t i = 0; i < targets.size(); ++i) {
      const Rect sq = BoardLayout::squareRect(BoardLayout::boardRect(targets[i].l, targets[i].t), dim, targets[i].x, targets[i].y);
      const float dist = Vector2Distance(fc, {sq.centerX(), sq.centerY()});
      const float ring = std::fmin(10.0f, std::floor(dist / std::fmax(1.0f, fromSq.w)));
      _dotDelay[i] = ring * staggerDots;
    }
  }
}

float MoveAnimator::enterProgress(BoardKey key) const {
  for (const auto& e : _enters)
    if (e.key == key) return e.t.progress();
  return 1.0f;
}

float MoveAnimator::dotScale(size_t i) const {
  if (reduced()) return 1.0f;
  const float delay = i < _dotDelay.size() ? _dotDelay[i] : 0.0f;
  return easeOutBack(clamp01((_dotClock - delay) / fast));
}

} // namespace play
