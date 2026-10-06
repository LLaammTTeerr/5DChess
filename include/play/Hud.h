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
  float chipWhite = 1.0f;    // 0 = black chip, 1 = white chip (cross-fades on a turn change)
  bool bannerActive = false; // the "<Colour> to move" banner below the action row
  bool bannerWhite = true;
  float bannerClock = 0.0f;
};

/// The end-of-game card: scrim, centred card, and in the Pixel theme the winner's king hopping on top.
struct EndCard {
  std::string title, reason; // "White wins!" / "Checkmate"
  bool whiteWon = true, draw = false;
  float scrim = 1.0f;        // 0..1 fade of scrim and card
  float pop = 1.0f;          // spring scale of the card (slight overshoot)
  float clock = 0.0f;        // seconds since the game ended (king hop)
};

/// Screen space, unaffected by the camera: the top-centre status pill, the bottom controls bar and the turn banner.
void drawHud(const HudData& hud, const BoardStyle& style);
void drawEndCard(const EndCard& card);

/// Time-dependent parts of the HUD: the chip cross-fade, the turn banner and the end card's pop-in.
class HudMotion {
public:
  HudMotion();
  void update(float dt);
  /// The side to move (a change starts the banner and the chip cross-fade; the first call only seeds).
  void setTurn(bool whiteToMove);
  /// The game ended (once) / is ongoing again.
  void setEnded(bool ended, bool whiteWon, bool draw);

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

  bool _endActive = false, _endWhiteWon = true, _endDraw = false;
  float _endClock = 0.0f;
  UI::Motion::Tween _endScrim;
  UI::Motion::Spring _endPop;
};

/// The Undo / Deselect / Submit row under the HUD pill.
class ActionRow {
public:
  enum class Action { None, Undo, Deselect, Submit };

  ActionRow();
  /// Updates all three buttons (they take the pointer first) and returns the one clicked this frame.
  Action update(float dt);
  void draw() const;
  /// The skin of the board view (nullptr: the default cream skin).
  void setSkin(const ui::Skin* skin) {
    _undo.skin = _deselect.skin = _submit.skin = skin;
  }
  void setEnabled(bool undo, bool deselect, bool submit) {
    _undo.enabled = undo;
    _deselect.enabled = deselect;
    _submit.enabled = submit;
  }

private:
  ui::Button _undo{"Undo", {}}, _deselect{"Deselect", {}}, _submit{"Submit", {}, true};
};

} // namespace play
