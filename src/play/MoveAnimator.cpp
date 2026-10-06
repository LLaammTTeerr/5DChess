#include "play/MoveAnimator.h"
#include "play/Feedback.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
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
  updateFeedback(UI::Motion::safeDt(dt));

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
      const bool newLane = std::none_of(_known.begin(), _known.end(), [&](const BoardKey& o) { return o.first == k.first; }) &&
                           std::none_of(_lanes.begin(), _lanes.end(), [&](const Lane& l) { return l.timeline == k.first; });
      if (newLane && !reduced()) _lanes.push_back({k.first, 0.0f}); // the lane of a new timeline unfolds
      Enter e{k, {}};
      e.t.start(0.0f, 1.0f, base, easeOutCubic, 0.0f, true); // under Reduce motion: a plain fade
      if (!e.t.done()) _enters.push_back(e);
    }
  }
  // _known is rebuilt from the boards that exist now, so keys of undone boards are dropped and a replayed move onto
  // the same key grows in again; same for any in-flight grow-in of a vanished board.
  _enters.erase(std::remove_if(_enters.begin(), _enters.end(), [&](const Enter& e) {
    return !std::binary_search(_scratch.begin(), _scratch.end(), e.key); }), _enters.end());
  _lanes.erase(std::remove_if(_lanes.begin(), _lanes.end(), [&](const Lane& l) {
    return std::none_of(_scratch.begin(), _scratch.end(), [&](const BoardKey& k) { return k.first == l.timeline; }); }), _lanes.end());
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
    // Arc through time: higher the more half-turns the piece travels (24 px + 12 px per half-turn)
    const int halfTurns = std::abs(spec.to.second - spec.from.second);
    f.arcHeight = std::fmin(170.0f, 24.0f + 12.0f * static_cast<float>(halfTurns));
    f.move.start(0.0f, 1.0f, slow, easeInOutCubic);
  } else {
    f.move.start(0.0f, 1.0f, base, easeOutCubic);
  }
  if (!spec.victim.empty()) f.victimFade.start(0.0f, 1.0f, fast, easeInCubic);
  // A move that creates a timeline: the piece leaves once its lane has started to unfold
  const bool newLane = std::none_of(_known.begin(), _known.end(), [&](const BoardKey& k) { return k.first == spec.to.first; });
  if (_seeded && newLane) f.move.start(0.0f, 1.0f, f.move.duration, f.move.easing, feedback::kFlightDelayNewTimeline);
  _flights.push_back(std::move(f));
}

void MoveAnimator::finish() {
  _flights.clear();
  _enters.clear();
  _lanes.clear();
  _laneView.clear();
  _kings.clear();
  _checkClock = 99.0f;
  _slide.active = false;
  _lifts.clear();
  _lifting = false;
}

void MoveAnimator::select(const std::optional<Chess::Core::Coord>& from, const std::vector<Chess::Core::Coord>& targets, int dim) {
  const bool changed = from && (!_selected || !(*_selected == *from));
  _selected = from;
  if (!from) _lift.snap(0.0f);
  else if (changed) { _lift.snap(0.0f); _lift.setTarget(1.0f); } // pick-up: the spring lifts the piece

  // Targets pop in with a 30 ms stagger per ring of squares away from the picked-up piece, all of them within 150 ms
  _dotDelay.assign(targets.size(), 0.0f);
  _dotClock = 0.0f;
  if (from) previewReset(); // the real dots take over from the hover preview
  if (from && !targets.empty()) {
    const Rect fromSq = BoardLayout::squareRect(BoardLayout::boardRect(from->l, from->t), dim, from->x, from->y);
    const Vector2 fc = {fromSq.centerX(), fromSq.centerY()};
    for (size_t i = 0; i < targets.size(); ++i) {
      const Rect sq = BoardLayout::squareRect(BoardLayout::boardRect(targets[i].l, targets[i].t), dim, targets[i].x, targets[i].y);
      const float dist = Vector2Distance(fc, {sq.centerX(), sq.centerY()});
      const float ring = std::fmin(10.0f, std::floor(dist / std::fmax(1.0f, fromSq.w)));
      _dotDelay[i] = feedback::dotDelay(ring);
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

// ---------------------------------------------------------------------------------------------------------------------
// Feedback motion

void MoveAnimator::rejected(const Chess::Core::Coord& square, Intent::Reason reason) {
  _reject = {keyOf(square), square.x, square.y, 0.0f};
  _alert.text = feedback::reasonText(reason);
  _alertClock = 0.0f;
}

MoveAnimator::Flash MoveAnimator::rejectFlash() const {
  if (_reject.clock >= feedback::kRejectFlashSeconds) return {};
  const float a = 1.0f - clamp01(_reject.clock / feedback::kRejectFlashSeconds);
  // Reduce motion: no shake, only the flash (a plain fade of 120 ms)
  const float fadeA = reduced() ? 1.0f - clamp01(_reject.clock / fast) : a;
  return {_reject.key, _reject.x, _reject.y, feedback::kRejectFlash * fadeA};
}

Vector2 MoveAnimator::boardOffset(BoardKey key, float zoom) const {
  if (reduced()) return {0.0f, 0.0f};
  const float inv = 1.0f / std::fmax(zoom, 0.05f);
  float dx = 0.0f, dy = 0.0f;
  if (_reject.key == key && _reject.clock < feedback::kShakeSeconds)
    dx = feedback::shakeOffset(_reject.clock / feedback::kShakeSeconds) * inv;
  if (_chromeNow.key == key) dy -= feedback::kChromeLiftPixels * easeOutCubic(_chromeNow.alpha) * inv;
  if (_chromeOld.key == key && _chromeOld.alpha > 0.0f) dy -= feedback::kChromeLiftPixels * easeOutCubic(_chromeOld.alpha) * inv;
  if (_lifting)
    for (const Lift& l : _lifts)
      if (l.key == key) dy -= feedback::liftBump(_liftClock - l.delay) * inv;
  return {dx, dy};
}

void MoveAnimator::previewHover(const std::optional<Chess::Core::Coord>& piece) { _pvWanted = piece; }

bool MoveAnimator::previewWantsTargets() const { return _pvWanted && _pvDwell >= feedback::kHoverDwell && !_pvNow.valid; }

void MoveAnimator::previewTargets(std::vector<Chess::Core::Coord> targets) {
  if (!_pvWanted) return;
  _pvNow.valid = true;
  _pvNow.from = *_pvWanted;
  _pvNow.targets = std::move(targets);
  _pvNow.alpha = reduced() ? 1.0f : 0.0f;
}

void MoveAnimator::previewReset() {
  _pvNow = {};
  _pvOld = {};
  _pvDwell = 0.0f;
  _pvSeen.reset();
}

void MoveAnimator::liveArcTo(const std::optional<std::pair<Chess::Core::Coord, Chess::Core::Coord>>& fromTo) { _arcWant = fromTo; }

void MoveAnimator::hoverChrome(const std::optional<BoardKey>& board) { _chromeWant = board; }

void MoveAnimator::checkStarted(std::vector<Chess::Core::Coord> kings) {
  _kings = std::move(kings);
  _checkClock = 0.0f;
}

float MoveAnimator::checkPulseNow() const {
  if (reduced() || _checkClock >= feedback::kCheckSeconds) return 0.0f;
  return feedback::checkPulse(_checkClock / feedback::kCheckSeconds);
}

float MoveAnimator::checkDrawOn() const {
  if (reduced() || _checkClock >= feedback::kCheckDrawOnSeconds) return 1.0f;
  return easeOutCubic(_checkClock / feedback::kCheckDrawOnSeconds);
}

void MoveAnimator::presentMoved(int fromHalfTurn, int toHalfTurn) {
  if (reduced() || fromHalfTurn == toHalfTurn) return;
  _slide = {fromHalfTurn, toHalfTurn, 0.0f, true};
  _slideClock = 0.0f;
}

void MoveAnimator::liftBoards(const std::vector<BoardKey>& boards) {
  if (reduced()) return;
  _lifting = true;
  _liftClock = 0.0f;
  _lifts.clear();
  float delay = 0.0f;
  for (const BoardKey& b : boards) {
    _lifts.push_back({b, delay});
    delay += feedback::kLiftStagger;
  }
}

void MoveAnimator::markSource(BoardKey from) {
  if (reduced()) return;
  _sourceKey = from;
  _sourceClock = 0.0f;
}

MoveAnimator::Glow MoveAnimator::sourceHalo() const {
  if (_sourceClock >= feedback::kSourceHaloSeconds) return {};
  return {_sourceKey, 1.0f - easeInCubic(clamp01(_sourceClock / feedback::kSourceHaloSeconds))};
}

void MoveAnimator::updateFeedback(float dt) {
  _reject.clock += dt;
  _alertClock += dt;
  _alert.alpha = _alertClock >= feedback::kHintAlertSeconds ? 0.0f : clamp01((feedback::kHintAlertSeconds - _alertClock) / 0.15f);
  _checkClock += dt;
  _sourceClock += dt;

  // Hover preview: the piece under a resting pointer shows its targets after a short dwell
  if (_pvWanted != _pvSeen) {
    if (_pvNow.valid) _pvOld = _pvNow;
    _pvNow = {};
    _pvDwell = 0.0f;
    _pvSeen = _pvWanted;
  } else if (_pvWanted) {
    _pvDwell += dt;
  }
  if (!_pvWanted && _pvNow.valid) { _pvOld = _pvNow; _pvNow = {}; }
  if (_pvNow.valid) _pvNow.alpha = reduced() ? 1.0f : std::fmin(1.0f, _pvNow.alpha + dt / base * 0.8f); // ~150 ms
  if (_pvOld.valid) {
    _pvOld.alpha = reduced() ? 0.0f : _pvOld.alpha - dt / 0.10f;
    if (_pvOld.alpha <= 0.0f) _pvOld = {};
  }

  // Live arc to the hovered cross-board target
  if (_arcWant) {
    if (!_arcActive || !(_arc.from == _arcWant->first) || !(_arc.to == _arcWant->second)) {
      _arc = {_arcWant->first, _arcWant->second, 0.0f};
      _arcActive = true;
      _arcClock = 0.0f;
      _targetGlow.key = keyOf(_arcWant->second);
    } else {
      _arcClock += dt;
    }
    _arc.grow = reduced() ? 1.0f : easeOutCubic(clamp01(_arcClock / feedback::kArcGrowSeconds));
  } else {
    _arcActive = false;
  }
  const float glowStep = reduced() ? 1.0f : dt / fast;
  _targetGlow.alpha = _arcActive ? std::fmin(1.0f, _targetGlow.alpha + glowStep) : std::fmax(0.0f, _targetGlow.alpha - glowStep);

  // Hovered card frame
  const float chromeStep = dt / fast;
  if (_chromeWant && _chromeNow.key != *_chromeWant) {
    _chromeOld = _chromeNow;
    _chromeNow = {*_chromeWant, 0.0f};
  }
  if (_chromeWant) _chromeNow.alpha = std::fmin(1.0f, _chromeNow.alpha + chromeStep);
  else if (_chromeNow.alpha > 0.0f) { _chromeOld = _chromeNow; _chromeNow = {}; }
  _chromeOld.alpha = std::fmax(0.0f, _chromeOld.alpha - chromeStep);

  // Lanes of new timelines
  _laneView.clear();
  for (Lane& l : _lanes) {
    l.clock += dt;
    if (l.clock < feedback::kLaneUnfoldSeconds) _laneView.push_back({l.timeline, easeOutCubic(l.clock / feedback::kLaneUnfoldSeconds)});
  }
  _lanes.erase(std::remove_if(_lanes.begin(), _lanes.end(), [](const Lane& l) { return l.clock >= feedback::kLaneUnfoldSeconds; }), _lanes.end());

  // The present marker's slide and the lift of the boards that got the move
  if (_slide.active) {
    _slideClock += dt;
    _slide.progress = easeInOutCubic(clamp01(_slideClock / feedback::kHandOverSeconds));
    if (_slideClock >= feedback::kHandOverSeconds) _slide.active = false;
  }
  if (_lifting) {
    _liftClock += dt;
    if (_liftClock >= feedback::kHandOverSeconds + feedback::kLiftStagger * static_cast<float>(_lifts.size())) { _lifting = false; _lifts.clear(); }
  }
}

} // namespace play
