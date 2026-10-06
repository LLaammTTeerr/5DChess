#pragma once
#include <string>
#include "chess.h"
#include "play/BoardStyle.h"
#include "ui/ConfirmArm.h"
#include "ui/Widgets.h"

namespace play {

/// The "Save" and "Copy" buttons under Back and the little panel that opens from Save: three slots with what they hold
/// (click one to write the game there), and a warning when the current turn has unsubmitted moves, which a record does not
/// contain. "Copy" puts the record on the clipboard (desktop only: a browser tab cannot read its clipboard back, so the
/// web build has no Copy and no Paste). Screen space, drawn in the skin and panel style of the board view.
class SaveMenu {
public:
  SaveMenu();
  /// Updates the buttons and the panel (they take the pointer first). `reachable`: false while the navigation controls are hidden.
  void update(float dt, bool reachable, const BoardStyle& style, const Chess::IGame& game);
  void draw(const BoardStyle& style, float navAlpha) const;
  bool open() const { return _open; }
  /// A short message in the HUD style (shown for a few seconds), e.g. that the autosave could not be written.
  void notify(std::string text, bool warn = true) { say(std::move(text), warn); }

private:
  ui::Button _save{"Save", {}}, _copy{"Copy", {}};
  ui::ButtonList _slots;
  bool _open = false;
  ui::ConfirmArm _overwrite; // saving over an occupied slot asks twice
  bool _occupied[3] = {false, false, false};
  bool _pending = false; // the current turn has moves a save would leave out
  std::string _message; // result of the last action ("Saved to slot 2"); shown for a few seconds
  bool _warn = false;   // the message is a warning (unsubmitted moves were left out)
  float _messageClock = 0.0f;

  void openPanel();
  Rectangle panelRect() const;
  Rectangle slotRect(int slot) const;
  void say(std::string text, bool warn);
};

} // namespace play
