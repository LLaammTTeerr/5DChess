#pragma once
#include "play/BoardLayout.h"

namespace play {

struct Vec2 {
  float x = 0, y = 0;
};

/// The camera over the boards. No graphics dependency, so the whole state machine is unit tested headless: PlayScreen feeds it the
/// window size, the player's drag / wheel / keys and the frame time, and reads the view back.
///
/// Three framings, plus an overlay:
///   Overview  every board, or the present column and its neighbours when the whole field would be too small to read
///   Focus     one board, readable (zoom >= 0.8 unless the caller says otherwise)
///   Free      the player's own framing (after a drag, the wheel or +/-); nothing moves it but the player and Home / Space / a click
///   Locked    a piece is picked up, a promotion is pending or the game is over: the camera makes no *automatic* motion
/// Every framing change is one tween of the position and log(zoom) together, interrupted by the player's next input. Under Reduce
/// motion the tween is an 80 ms linear move (not a teleport: a teleport loses the player).
class BoardCamera {
public:
  enum class State { Overview, Focus, Free };

  static constexpr float kMinZoom = 0.4f, kMaxZoom = 2.5f;     // the player's wheel range
  static constexpr float kWheelStep = 1.12f;                   // zoom *= kWheelStep ^ notches
  static constexpr float kKeyStep = 1.25f;                     // + / - keys
  static constexpr float kReadableZoom = 0.8f;                 // 25 px squares: the smallest zoom the camera chooses on its own
  static constexpr float kMaxOverviewZoom = 1.25f;
  static constexpr float kFarOverviewZoom = 0.3f;              // below this the Overview shows the present column +- 2 instead of everything
  static constexpr float kFocusTime = 0.26f, kOverviewTime = 0.36f, kHopTime = 0.20f, kReducedTime = 0.08f;
  static constexpr float kPadding = 44.0f;                     // inside the safe area: room for a card's label strip and its halo

  /// The nearest of the fixed zoom steps {0.6, 0.8, 1.0, 1.25} (compared on a log scale); a zoom above the last step is kept.
  static float snapZoom(float zoom);
  /// The largest step <= zoom (the smallest step when zoom is below it): what an automatic fit rounds down to.
  static float snapDown(float zoom);

  void setViewport(float width, float height);
  /// Reserved screen margins (px) so that content is placed in the free area between the HUD bars.
  void setInsets(float top, float right, float bottom, float left);
  void setReduceMotion(bool reduced) { _reduced = reduced; }
  /// Where the player may pan to: the boards plus a board of margin. Optional (no bounds until set).
  void setWorldBounds(const Rect& bounds);

  // ---- geometry (pure affine maps: no rotation) ----
  float zoom() const { return _zoom; }
  Vec2 target() const { return _target; } // the world point at the centre of the free area
  Vec2 offset() const;                    // that centre on screen
  Vec2 screenToWorld(Vec2 s) const;
  Vec2 worldToScreen(Vec2 w) const;
  /// The free area (the screen minus the HUD) in world coordinates.
  Rect visibleWorld() const;
  /// At least `fraction` of the card is inside the free area.
  bool isBoardVisible(const Rect& card, float fraction) const;

  // ---- state ----
  State state() const { return _state; }
  const char* stateLabel() const;
  bool locked() const { return _locked; }
  bool moving() const { return _tween.active; }
  bool seeded() const { return _seeded; }
  /// The rectangle the current framing was made for (Overview / Focus), so it can be re-applied after a resize.
  const Rect& frameRect() const { return _frame; }

  /// While locked, requests flagged `automatic` are refused (they return false); the player's own are never.
  void lock(bool locked) { _locked = locked; }
  /// Stop where the view is now. A tween in flight makes the framing the player's own (Free).
  void cancel();

  // ---- framing requests (each starts one tween; the very first one, or `snap`, jumps) ----
  /// Show all of `all` (cards), or `present` (the present column +- 2) when all would be below kFarOverviewZoom. Zoom <= 1.25.
  bool overview(const Rect& all, const Rect& present, bool automatic = false, bool snap = false);
  /// Fit `region` (zoom between minZoom and 1.25) and call it an Overview: the present column after a submit with several mandatory boards.
  bool fitRegion(const Rect& region, float minZoom, bool automatic = false);
  /// Centre a card at max(minZoom, snapZoom(current zoom)), never larger than the card fits.
  bool focusBoard(const Rect& card, float minZoom, bool automatic = false, float duration = kFocusTime);
  /// A move to another board: keep both `from` and `to` in view (zoom down to no less than minZoom, else centre `to` at minZoom).
  bool showBoth(const Rect& from, const Rect& to, float minZoom, bool automatic = false);
  /// Pan, without a zoom change, just far enough that `card` is wholly in the free area. The state label is kept.
  bool reveal(const Rect& card, bool automatic = false);
  /// Slide `fraction` of the way to `card` when it is less than half visible. The state label is kept.
  bool peek(const Rect& card, float fraction, bool automatic = false);
  /// Apply the current framing again (the window or the insets changed); no tween.
  void reframe();

  // ---- the player's own motion (always honoured; they leave Overview / Focus for Free) ----
  void pan(Vec2 screenDelta);
  /// Zoom by `factor` keeping the world point under `screenPoint` where it is. The result stays within kMinZoom..kMaxZoom (a view
  /// already outside the range may only move towards it).
  void zoomAt(Vec2 screenPoint, float factor);
  /// The wheel: `notches` of kWheelStep each, around the pointer.
  void wheel(Vec2 screenPoint, float notches);
  /// The pointer left the drag at this speed (px/s): the view coasts for ~120 ms. Ignored under Reduce motion.
  void fling(Vec2 screenVelocity);

  void update(float dt);
  /// Finish a running tween now.
  void finishTween();

private:
  struct Tween {
    bool active = false;
    Vec2 from, to;
    float fromLog = 0, toLog = 0;
    float t = 0, duration = 0.2f;
    bool linear = false;
    bool easeInOut = false;
  };

  float _viewW = 1400, _viewH = 800;
  float _insetTop = 0, _insetRight = 0, _insetBottom = 0, _insetLeft = 0;
  bool _reduced = false;
  bool _hasBounds = false;
  Rect _bounds;

  Vec2 _target{0, 0};
  float _zoom = 1.0f;
  State _state = State::Overview;
  bool _locked = false;
  bool _seeded = false;
  Rect _frame;
  float _frameMinZoom = 0.0f;
  Tween _tween;
  Vec2 _fling;

  Vec2 safeSize() const;
  float fitZoom(const Rect& r) const;
  void goTo(Vec2 target, float zoom, float duration, bool easeInOut, bool snap = false);
  void clampTarget();
  void panRaw(Vec2 screenDelta);
  static float clampWheel(float current, float wanted);
};

} // namespace play
