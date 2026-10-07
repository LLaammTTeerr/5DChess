#pragma once
#include <optional>
#include <vector>
#include <raylib.h>
#include "chess.h"
#include "play/BoardCamera.h"
#include "play/BoardLayout.h"
#include "play/BoardScene.h"
#include "play/BoardStyle.h"
#include "play/MotionOverlay.h"
#include "play/MoveAnimator.h"
#include "play/MultiverseView.h"
#include "play/Selection.h"

namespace play {

/// The game screen's feedback motion and its cues, kept out of PlayScreen: what the pointer is doing (hover preview, live arc, hovered
/// card frame, clicks that are refused), what a move or a submitted turn does (sounds, the present marker's slide, the lift of the boards
/// that got the move, the halo of the source board, the check pulse) and how all of it is drawn (play/MotionOverlay). The state lives in
/// the MoveAnimator it is given; PlayScreen only forwards a handful of events and draw calls.
class PlayFeedback {
public:
  explicit PlayFeedback(MoveAnimator& animator) : _a(animator) {}

  /// What the screen knows about this frame's pointer.
  struct Pointer {
    const Chess::IGame& game;
    const BoardLayout& layout;
    const BoardCamera& camera;
    const Selection& selection;
    std::optional<Chess::Core::Coord> hover; // the square under the pointer when the boards take input
    bool pointerFree = true;                 // no button or panel has the pointer
    bool blocked = false, aiToMove = false, locked = false, ended = false;
    // The Play view has no camera: it names the card frame and the square under the pointer itself (camera and layout are then unused)
    bool screenSpace = false;
    std::optional<BoardKey> chrome;
    std::optional<Chess::Core::Coord> squareUnder;
  };
  /// Once a frame after the boards took their input.
  void input(const Pointer& in);

  /// A click on `square` did nothing.
  void refused(const Chess::Core::Coord& square, Intent::Reason reason);
  /// A move was made (before the layout is rebuilt). `from`: the board it left.
  void moved(bool newTimeline, bool capture, bool sameBoard, BoardKey from);
  /// The turn was submitted (the next rebuilt() lifts the boards of the side to move).
  void turnSubmitted() { _submitPending = true; }
  /// The layout was rebuilt (`presentBefore`: the present of the previous view): check pulse, present slide, lift, and the one sound
  /// of the change (a check wins over a move, which wins over the submit cue).
  void rebuilt(const Chess::IGame& game, const MultiverseView& view, int presentBefore);

  /// What the draw calls need.
  struct Draw {
    const BoardStyle& style;
    const MultiverseView& view;
    const BoardLayout& layout;
    const Chess::IGame& game;
    const Camera2D& camera;
    BoardScene& scene;
    bool pieceSelected = false;
  };
  /// Before the lanes: the present marker slides.
  void beforeLanes(BoardScene& scene) const;
  /// After the lanes (screen space): a new lane unfolding.
  void afterLanes(const Draw& d) const;
  /// Behind the cards (world space): the glow of the live arc's target and the dashed mark of the source board.
  void behindCards(const Draw& d) const;
  /// Over the boards (world space): refusal flash, hover preview, check pulse, live arc.
  void overBoards(const Draw& d) const;
  /// Over the HUD (screen space): the ruler highlight of the live arc and the reason of a refused click.
  void overHud(const Draw& d) const;

private:
  MoveAnimator& _a;
  bool _submitPending = false;
  bool _checkSeeded = false;                  // the first rebuild only seeds _lastKings
  std::vector<Chess::Core::Coord> _lastKings; // attacked kings of the last rebuild (a change starts the check pulse)
  bool _previewArmed = false;                 // the pointer has moved since the last click: the hover preview may show
  Vector2 _lastPointer{-1.0f, -1.0f};
  std::optional<int> _moveSfx;                // Sfx of a move waiting for rebuilt() to decide the one sound of the frame
  int _moveSfxAge = 0;
};

} // namespace play
