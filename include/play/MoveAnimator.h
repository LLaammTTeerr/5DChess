#pragma once
#include <optional>
#include <string>
#include <utility>
#include <vector>
#include <raylib.h>
#include "Render/Motion.h"
#include "play/BoardLayout.h"
#include "play/Selection.h"

namespace play {

/// Identity of a board: (timeline id, half-turn). Everything that animates is keyed by this, never by object.
using BoardKey = std::pair<int, int>;
inline BoardKey keyOf(const Chess::Core::Coord& c) { return {c.l, c.t}; }

/// A piece travelling between squares. Visual only: the game already holds the finished move, and the destination
/// piece is hidden on its board until the flight lands.
struct FlightSpec {
  std::string piece;      // texture key, e.g. "white_pawn"
  std::string victim;     // captured piece (empty if none)
  BoardKey from{0, 0};    // board holding the source square (== to for same-board moves)
  BoardKey to{0, 0};      // the newly created board holding the moved piece
  int fromX = 0, fromY = 0, toX = 0, toY = 0;
  int dim = 8;
  float delay = 0.0f;     // seconds before the piece leaves (set by startFlight: a move that creates a timeline waits for its lane)
};

/// All motion of the board view that is not the camera: pieces flying to their squares, new boards growing in, the
/// picked-up piece lifting and the legal-target dots popping in, and the hover tint fading; and the feedback motion:
/// the illegal-click shake, the hover preview of a piece's moves, the live time-travel arc, the unfolding lane of a new timeline,
/// the check pulse, the turn hand-over and the halo on the source board of a time-travel move (numbers in play/Feedback.h).
/// State only; the BoardRenderer draws it.
class MoveAnimator {
public:
  MoveAnimator();

  void update(float dt, const std::optional<Chess::Core::Coord>& hover);

  // ---- Feedback motion --------------------------------------------------------------------------------------------
  // The setters below say what the pointer is doing this frame; call them before update().

  /// A click on `square` did nothing: its card shakes, the square flashes red and the HUD gets a red reason for a moment.
  void rejected(const Chess::Core::Coord& square, Intent::Reason reason);
  struct Flash { BoardKey key{0, 0}; int x = -1, y = -1; float alpha = 0.0f; };
  Flash rejectFlash() const;
  struct Alert { std::string text; float alpha = 0.0f; };
  const Alert& hintAlert() const { return _alert; }

  /// Offset in world units of a board's card from where it rests: the shake, the lift of a hovered frame and of the boards of the
  /// side that just got the move. Zero under Reduce motion.
  Vector2 boardOffset(BoardKey key, float zoom) const;

  /// The pointer rests on a piece the player could pick up (nullopt: it does not). After a short dwell previewWantsTargets() turns
  /// true: answer with previewTargets(); the targets then fade in as hollow dots.
  void previewHover(const std::optional<Chess::Core::Coord>& piece);
  bool previewWantsTargets() const;
  void previewTargets(std::vector<Chess::Core::Coord> targets);
  void previewReset();
  struct PreviewLayer { bool valid = false; Chess::Core::Coord from{}; std::vector<Chess::Core::Coord> targets; float alpha = 0.0f; };
  const PreviewLayer& previewNow() const { return _pvNow; }
  const PreviewLayer& previewFading() const { return _pvOld; }

  /// A piece is selected and the pointer is over one of its targets on another board: the arc to it is drawn live (nullopt: none).
  void liveArcTo(const std::optional<std::pair<Chess::Core::Coord, Chess::Core::Coord>>& fromTo);
  struct LiveArc { Chess::Core::Coord from{}, to{}; float grow = 0.0f; };
  const LiveArc* liveArc() const { return _arcActive ? &_arc : nullptr; }
  /// Glow of the board a live arc points at (key of the last target), 0..1.
  struct Glow { BoardKey key{0, 0}; float alpha = 0.0f; };
  const Glow& targetGlow() const { return _targetGlow; }

  /// The pointer is over the frame (not the squares) of a board's card.
  void hoverChrome(const std::optional<BoardKey>& board);

  /// A lane that is still unfolding: `progress` 0..1 eased; the layer is drawn over the band that already exists.
  struct LaneUnfold { int timeline = 0; float progress = 0.0f; };
  const std::vector<LaneUnfold>& unfoldingLanes() const { return _laneView; }
  /// A king was newly attacked: its square pulses and the attack lines draw on. `kings` are the attacked squares.
  void checkStarted(std::vector<Chess::Core::Coord> kings);
  /// Squares of the attacked kings and the pulse intensity 0..1 (0 when no pulse runs).
  const std::vector<Chess::Core::Coord>& checkedKings() const { return _kings; }
  float checkPulseNow() const;
  /// 0..1 progress of the attack lines drawing from attacker to king (1 when settled).
  float checkDrawOn() const;

  /// The turn was handed over: the present column slides from half-turn `from` to `to`, and `boards` (the boards the new side
  /// to move may move on, in column order) lift one after the other.
  void handOver(int fromHalfTurn, int toHalfTurn, const std::vector<BoardKey>& boards);
  struct Slide { int from = 0, to = 0; float progress = 1.0f; bool active = false; };
  const Slide& presentSlide() const { return _slide; }

  /// A move left board `from` for another board: it keeps an accent halo for a moment.
  void markSource(BoardKey from);
  Glow sourceHalo() const;

  /// Call when the layout was rebuilt: boards that did not exist before grow in (never on the first call).
  void syncBoards(const BoardLayout& layout);
  void startFlight(const FlightSpec& spec);
  /// New input: running flights and grow-ins jump to their end.
  void finish();

  /// A piece was picked up (lifts) and its targets are shown (dots pop in, rippling outwards); nullopt: nothing selected.
  void select(const std::optional<Chess::Core::Coord>& from, const std::vector<Chess::Core::Coord>& targets, int dim);

  struct Flight {
    FlightSpec spec;
    Vector2 from{}, to{};
    float size = 0.0f;
    float arcHeight = 0.0f;
    UI::Motion::Tween move, victimFade;
  };
  const std::vector<Flight>& flights() const { return _flights; }
  /// 1 for a board that is fully there, 0..1 while it grows in.
  float enterProgress(BoardKey key) const;
  float lift() const { return _lift.value; }
  /// Scale of the legal-target dot `i` (pop-in with a small overshoot).
  float dotScale(size_t i) const;

  struct Hover { BoardKey key{0, 0}; int x = -1, y = -1; float alpha = 0.0f; };
  const Hover& hoverNow() const { return _hoverCur; }
  const Hover& hoverFading() const { return _hoverOld; }

private:
  void updateFeedback(float dt);
  struct Enter { BoardKey key; UI::Motion::Tween t; };
  std::vector<Flight> _flights;
  std::vector<Enter> _enters;
  std::vector<BoardKey> _known, _scratch; // sorted
  bool _seeded = false;

  std::optional<Chess::Core::Coord> _selected;
  UI::Motion::Spring _lift;
  std::vector<float> _dotDelay;
  float _dotClock = 0.0f;

  Hover _hoverCur, _hoverOld;

  // feedback state
  struct Reject { BoardKey key{0, 0}; int x = -1, y = -1; float clock = 99.0f; };
  Reject _reject;
  Alert _alert;
  float _alertClock = 99.0f;

  std::optional<Chess::Core::Coord> _pvWanted, _pvSeen;
  float _pvDwell = 0.0f;
  PreviewLayer _pvNow, _pvOld;

  std::optional<std::pair<Chess::Core::Coord, Chess::Core::Coord>> _arcWant;
  bool _arcActive = false;
  LiveArc _arc;
  float _arcClock = 0.0f;
  Glow _targetGlow;

  std::optional<BoardKey> _chromeWant;
  Glow _chromeNow, _chromeOld;   // hovered frame lift 0..1 (and the one just left)

  struct Lane { int timeline; float clock; };
  std::vector<Lane> _lanes;
  std::vector<LaneUnfold> _laneView;

  std::vector<Chess::Core::Coord> _kings;
  float _checkClock = 99.0f;

  Slide _slide;
  float _slideClock = 99.0f;
  struct Lift { BoardKey key; float delay; };
  std::vector<Lift> _lifts;

  BoardKey _sourceKey{0, 0};
  float _sourceClock = 99.0f;
};

} // namespace play
