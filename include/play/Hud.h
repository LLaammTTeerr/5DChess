#pragma once
#include <string>
#include "Render/Motion.h"
#include "play/BoardStyle.h"
#include "ui/Widgets.h"

namespace play {

/// Everything the heads-up display shows this frame.
struct HudData {
  bool whiteToMove = true;
  int fullTurn = 1;          // 1-based full-turn number shown to the player
  int timelineCount = 1;
  std::string hint;          // e.g. "Select a piece"
  std::string title;         // replaces "White to move" / "Black to move" in the pill when set (a puzzle that is decided)
  float chipWhite = 1.0f;    // 0 = black chip, 1 = white chip (cross-fades on a turn change)
  bool bannerActive = false; // the "<Colour> to move" banner below the action row
  bool bannerWhite = true;
  float bannerClock = 0.0f;
  float thinking = 0.0f;     // 0..1 fade of the computer's "thinking" indicator in the pill (0: not shown)
  float thinkFraction = 0.0f;// 0..1 progress of its search (eased)
  bool thinkBar = false;     // the search is running: show the bar (otherwise only the dots)
  double clock = 0.0;        // seconds, for the indicator's dots
  // Filled by PlayScreen from the camera and the game:
  std::string cameraLabel;    // "Overview" / "Focus" / "Free": drawn at the left of the controls bar
  bool playView = false;      // the Play view is showing (cameraLabel is "Play view"): the toggle reads "Multiverse [M]", the controls line has its keys
  bool playViewAvailable = false; // the "Play view [P]" / "Multiverse [M]" toggle shows (not on an embedded board)
  std::string nextBoardLabel; // non-empty while a Mandatory board is off-screen ("Move on L+1 . T3b"): the "Next board" button shows
  float rightInset = 0.0f;    // a side panel (Puzzle / Guide) of this width at the right: pill, action row and controls bar centre on the rest
  std::string undoLabel;      // "Undo move" (one move of this turn) / "Undo turn" (against the computer: the turn); empty: "Undo"
  int undoCount = 0;          // moves of the unsubmitted turn: a badge on Undo when there are several
  std::string submitTip;      // the tooltip of Submit: why it is disabled, or the moves it will submit
  bool boardsReady = false; // Submit is ready: the count is drawn bright
  int boardsDone = 0, boardsTotal = 0; // "3 / 5 boards" in the pill: the boards of this turn that must be moved on (shown from 2 up; 0: not shown)
  float controlsAlpha = 1.0f; // the controls bar fades out after a few seconds without input (HudMotion)
};

/// The end-of-game card: scrim, centred card, and in the Pixel theme the winner's king hopping on top.
struct EndCard {
  std::string title, reason; // "White wins!" / "Checkmate"
  std::string footer = "Use Back to return to game selection"; // the line under the rule
  bool whiteWon = true, draw = false;
  bool buttons = false;      // the Rematch / Review board / Back to menu row (EndCardButtons) instead of the footer line
  float scrim = 1.0f;        // 0..1 fade of scrim and card
  float pop = 1.0f;          // spring scale of the card (slight overshoot)
  float clock = 0.0f;        // seconds since the game ended (king hop)
};

/// A panel in the style of the board view (the HUD's, and the save menu's); roundness 1 is a pill, smaller values suit tall panels.
void drawPanel(Rectangle r, const BoardStyle& style, float alpha = 1.0f, float roundness = 1.0f);

/// Screen space, unaffected by the camera: the top-centre status pill, the bottom controls bar and the turn banner.
void drawHud(const HudData& hud, const BoardStyle& style);

class EndCardButtons;
/// Screen rectangle of the end card (taller with its buttons).
Rectangle endCardRect(bool buttons);
/// `buttons`: drawn on the card (they scale and fade with it); nullptr: the footer line instead.
void drawEndCard(const EndCard& card, const EndCardButtons* buttons = nullptr);

/// The way out of a finished game, on the end card: Rematch (primary), Review board (dismisses the card, the position stays
/// readable) and Back to menu. A rematch needs a catalog mode: `rematch` false hides that button.
class EndCardButtons {
public:
  enum class Action { None, Rematch, Review, Menu };
  EndCardButtons();
  void setRematch(bool available);
  void setSkin(const ui::Skin* skin) { _rematch.skin = _review.skin = _menu.skin = skin; }
  /// The card swallows the pointer (it is modal); returns the button clicked this frame.
  Action update(float dt, bool reachable);
  void draw(float alpha) const;

private:
  bool _hasRematch = true;
  ui::Button _rematch{"Rematch", {}, true}, _review{"Review board", {}}, _menu{"Back to menu", {}};
  void layout();
};

/// Time-dependent parts of the HUD: the chip cross-fade, the turn banner and the end card's pop-in.
class HudMotion {
public:
  HudMotion();
  void update(float dt);
  /// The side to move (a change starts the banner and the chip cross-fade; the first call only seeds).
  void setTurn(bool whiteToMove);
  /// The computer is searching (`fraction`: Search::progress().fraction); the indicator fades in and out and its bar eases to the
  /// value. Under Reduce motion it follows the value without easing and the dots do not pulse.
  void setThinking(bool thinking, float fraction); // fraction < 0: no search yet, dots only
  /// The game ended (once) / is ongoing again.
  void setEnded(bool ended, bool whiteWon, bool draw);

  /// The controls bar fades after kControlsIdle seconds without pointer input (reads Input::) and returns near the window's bottom.
  static constexpr float kControlsIdle = 3.0f;
  /// Fill the animated fields of `hud`.
  void apply(HudData& hud) const;
  /// The end card with its animation state (title and reason are the caller's).
  EndCard endCard() const;

private:
  bool _seeded = false;
  bool _white = true;
  bool _bannerActive = false;
  float _bannerClock = 0.0f;
  UI::Motion::Tween _chip;
  bool _thinking = false;
  bool _thinkBar = false;
  float _thinkTarget = 0.0f, _thinkShown = 0.0f, _thinkVelocity = 0.0f;
  UI::Motion::Tween _thinkFade;
  double _clock = 0.0;
  float _idle = 0.0f, _controls = 1.0f;
  int _turnChanges = 0; // the controls bar stays until the first turn has been handed over

  bool _endActive = false, _endWhiteWon = true, _endDraw = false;
  float _endClock = 0.0f;
  UI::Motion::Tween _endScrim;
  UI::Motion::Spring _endPop;
};

/// The Undo / Deselect / Submit row under the HUD pill, with the camera's Overview and Next board buttons at its right end.
class ActionRow {
public:
  enum class Action { None, Undo, Deselect, Submit, Overview, NextBoard, ToggleView };

  ActionRow();
  /// Centre the row on `availableWidth` (the window minus a side panel); Overview and Next board sit at that width's right end.
  void layout(float availableWidth);
  /// Takes this frame's HUD data: the Undo label and badge, Submit's tooltip, whether Next board shows.
  void sync(const HudData& hud);
  /// Updates all buttons (they take the pointer first) and returns the one clicked this frame.
  Action update(float dt);
  void draw() const;
  /// The skin of the board view (nullptr: the default cream skin).
  void setSkin(const ui::Skin* skin) {
    _undo.skin = _deselect.skin = _submit.skin = _overview.skin = _next.skin = _view.skin = skin;
  }
  /// Reviewing a finished game: the Deselect slot becomes "Result" (enabled), which brings the end card back.
  void setReview(bool on) {
    _deselect.label = on ? "Result" : "Deselect";
    if (on) _deselect.enabled = true;
  }
  void setEnabled(bool undo, bool deselect, bool submit) {
    _undo.enabled = undo;
    _deselect.enabled = deselect;
    _submit.enabled = submit;
  }

  // Layout, exposed for the overflow tests
  const ui::Button& undoButton() const { return _undo; }
  const ui::Button& overviewButton() const { return _overview; }
  const ui::Button& nextButton() const { return _next; }
  const ui::Button& viewButton() const { return _view; }
  bool viewVisible() const { return _viewShown; }
  bool nextVisible() const { return _nextShown; }
  bool overviewVisible() const { return _overviewShown; }

private:
  float _availableW = 0.0f;
  bool _nextShown = false, _overviewShown = false, _overviewLaidOut = false, _viewShown = false, _playView = false;
  int _undoCount = 0;
  ui::Button _undo{"Undo", {}}, _deselect{"Deselect", {}}, _submit{"Submit", {}, true}, _overview{"Overview", {}}, _next{"Next board", {}}, _view{"Play view [P]", {}};
};

} // namespace play
