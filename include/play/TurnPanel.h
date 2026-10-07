#pragma once
#include <optional>
#include <string>
#include <utility>
#include <raylib.h>
#include "chess.h"
#include "play/BoardStyle.h"
#include "play/TurnChecklist.h"
#include "ui/Widgets.h"

namespace play {

/// The docked turn checklist at the right of the game screen (docs: README "Turn checklist"): one row per board of this turn with a
/// thumbnail, its label, a state chip and the move once played; the Submit summary; the opponent's last turn as clickable lines. It
/// holds the data it is given (setChecklist) and reports what was clicked; PlayScreen decides what the camera does. A small button at
/// the top right (and the C key, handled by PlayScreen) folds it away.
class TurnPanel {
public:
  static constexpr float kWidth = 300.0f, kRight = 24.0f, kBottom = 52.0f;
  /// Under the HUD zone (the pill, the action row with Overview / Next board and the HUD button keep the whole window width above it).
  static constexpr float kTop = 124.0f;
  /// What the boards keep free at the right of the window while the panel is open (panel, its margin, a gap).
  static constexpr float kInset = kWidth + kRight + 16.0f;

  /// Off: not on this screen (an embedded board, a finished game); Collapsed: only the HUD button; Open.
  enum class Mode { Off, Collapsed, Open };

  struct Events {
    std::optional<std::pair<int, int>> focusBoard; // a row was clicked: its (timeline, half-turn)
    std::optional<LastMove> focusMove;             // a line of the opponent's last turn was clicked
    bool toggled = false;                          // the HUD button was clicked
  };

  /// Whether the panel is folded away. It is kept for the whole session (a player who closed it does not want it back in the next game).
  static bool wanted();
  static void setWanted(bool open);

  /// New data (the game's state changed).
  void setChecklist(TurnChecklist checklist);
  /// `turnHeading`: "This turn" / "Computer's turn"; `lastHeading`: "Opponent's last turn" / "Computer's last turn" / "Your last turn".
  void setHeadings(std::string turnHeading, std::string lastHeading) {
    _turnHeading = std::move(turnHeading);
    _lastHeading = std::move(lastHeading);
  }
  const TurnChecklist& checklist() const { return _checklist; }

  /// What the screen shows from now on (draw() follows it even before the next update()).
  void setMode(Mode mode) { _mode = mode; }

  /// Takes the pointer where it is over the panel (so the boards below do not see it) and returns what was clicked.
  Events update(float dt, Mode mode, bool buttonReachable, const ui::Skin* skin);
  /// `game`: the thumbnails are its boards. `buttonAlpha`: the HUD button fades with the other navigation buttons.
  void draw(const Chess::IGame& game, const BoardStyle& style, float buttonAlpha) const;

  /// The board a row (or line) under the pointer stands for: the scene rings it.
  std::optional<std::pair<int, int>> hoveredBoard() const { return _hoverBoard; }
  Rectangle rect() const { return _mode == Mode::Open ? _geo.panel : Rectangle{}; }

private:
  struct List {
    Rectangle view{};
    float rowH = 0.0f, scroll = 0.0f;
    int count = 0;
    float content() const { return static_cast<float>(count) * rowH; }
    float maxScroll() const { return content() > view.height ? content() - view.height : 0.0f; }
    int rowAt(Vector2 p) const;
  };
  struct Geometry {
    Rectangle panel{}, header{}, submit{}, submitNote{}, lastHeader{};
    float dividerA = 0.0f, dividerB = 0.0f; // y of the two rules
  };

  Mode _mode = Mode::Off;
  TurnChecklist _checklist;
  std::string _turnHeading = "This turn", _lastHeading = "Opponent's last turn";
  Geometry _geo;
  List _rows, _lines;
  int _hoverRow = -1, _hoverLine = -1;
  bool _draggingRows = false, _draggingLines = false;
  std::optional<std::pair<int, int>> _hoverBoard;
  ui::Button _toggle{"", {}};

  void layout();
  void scrollInput(List& list, bool& dragging, Vector2 mouse, bool inside);
  void drawLast(const BoardStyle& style, Rectangle panel, float inner) const;
};

} // namespace play
