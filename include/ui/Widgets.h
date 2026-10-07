#pragma once
#include <raylib.h>
#include <string>
#include <vector>
#include "Render/Motion.h"

// Small widget layer: retained state, immediate drawing. A widget's update(dt) reads the pointer (through
// Input::) and advances its animations; draw() only paints that state. Every widget keeps its rect, so a
// screen lays widgets out once (ui::column / ui::row) and then just calls update + draw each frame.
namespace ui {

// ---- Pointer ownership: who has the mouse this frame --------------------------------------------------
// ScreenStack resets the flag at the start of every frame. A widget with the pointer over it consumes it;
// whatever runs later (e.g. board selection) must treat the pointer as taken. The order of update() calls
// is therefore the stacking order: top-most UI first, the game board last.
void beginFrame(bool pointerConsumed = false);
bool pointerConsumed();
void consumePointer();

// ---- Layout ---------------------------------------------------------------------------------------------
enum class Align { Center, Top };
// n items of height itemH stacked with `gap`, spanning area's width; centred vertically in area (or from its top).
std::vector<Rectangle> column(Rectangle area, int n, float itemH, float gap, Align align = Align::Center);
// n items of width itemW side by side with `gap`, centred horizontally in area, each as tall as area.
std::vector<Rectangle> row(Rectangle area, int n, float itemW, float gap);

// ---- Tooltip ----------------------------------------------------------------------------------------------
// Immediate mode: call every frame while the pointer is over `anchor` (Button does it for its `tip`, other callers check the
// pointer themselves with hovered()). The bubble appears under the anchor (above it near the bottom) after the pointer has rested
// on the same anchor for kTooltipDwell seconds, fading in (at once under Reduce motion); `text` may hold '\n'. Draw late in the
// frame: it is painted at once, over what was drawn before it.
inline constexpr double kTooltipDwell = 0.4;
void tooltip(Rectangle anchor, const std::string& text);
/// True when the pointer is inside `r` (screen space).
bool hovered(Rectangle r);

// ---- Skin -----------------------------------------------------------------------------------------------
// The colours of a Button. The default is the cream + terracotta look of the menus; the game screen passes the
// skin of the current board view (Deep space: dark, Atlas: paper, Blueprint: sharp ink on white).
struct Skin {
  Color surface, surfaceHover, border, borderHover, text;
  Color primary, primaryHover, primaryPressed, onPrimary;
  Color pressed, pressedText; // secondary button: background / border and text while pressed
  Color disabledBg, disabledText;
  float roundness = 0.25f;  // DrawRectangleRounded roundness; 0 = square corners
};
const Skin& defaultSkin();

// ---- Button ---------------------------------------------------------------------------------------------
// Secondary (outlined) or primary (filled) button with eased hover, a pressed look that only shows when the
// press began on the button, a disabled look, a click sound and the pointing-hand cursor. A click fires on
// the press (as every menu always did). Disabled buttons still take the pointer so nothing is clickable
// underneath them.
struct Button {
  std::string label;
  Rectangle rect{};
  bool primary = false;
  bool enabled = true;
  bool ellipsize = false;               // a label that does not fit is cut and ends in "..." (at its nominal size: no shrinking first)
  bool leftAligned = false;             // label at the left edge (rows of a list) instead of centred
  std::string detail;                   // second segment, muted and right-aligned ("12 turns . 2026-01-01"); implies a left-aligned label
  std::string tip;                      // tooltip shown after the pointer rests on the button (also while it is disabled)
  const Skin* skin = nullptr;           // nullptr: defaultSkin()
  const Texture2D* icon = nullptr;      // drawn centred instead of the label (e.g. a piece sprite)

  Button() = default;
  Button(std::string text, Rectangle r, bool isPrimary = false) : label(std::move(text)), rect(r), primary(isPrimary) {}

  // Fade and rise in after `delay` seconds (staggered lists); input is never delayed.
  void enterAfter(float delay);
  // `reachable` is false when something else (a list's clip rect) hides the button from the pointer.
  // Returns true on the frame the button is clicked.
  bool update(float dt, bool reachable = true);
  void draw(float alpha = 1.0f) const;

private:
  bool hot_ = false, over_ = false, pressStartedHere_ = false, pressed_ = false, entering_ = false;
  float hover_ = 0.0f; // 0..1, eased toward hot_ over ~150 ms
  UI::Motion::Tween enter_;
};

// A button showing one of two labels that flips a bool when clicked.
struct Toggle : Button {
  Toggle(Rectangle r, bool& value, const char* onLabel, const char* offLabel);
  bool update(float dt);

private:
  bool& value_;
  const char *on_, *off_;
};

// A button showing "<prefix><option>" that moves on to the next option (wrapping) when clicked.
struct Cycle : Button {
  Cycle(Rectangle r, std::string prefix, std::vector<std::string> options, int value);
  bool update(float dt);
  int value() const { return value_; }

private:
  std::string prefix_;
  std::vector<std::string> options_;
  int value_;
};

// ---- ButtonList -----------------------------------------------------------------------------------------
// A list of buttons (each in its own slot) with an optional single selection shown by an accent outline
// that glides between items, a staggered entrance, and, given a viewport, wheel / scrollbar scrolling
// with clipping.
class ButtonList {
public:
  int selected = -1;       // index of the selected item or -1
  bool selectable = true;  // false: plain buttons (a navigation column), a click does not select
  const Skin* skin = nullptr;  // for every item; nullptr: defaultSkin()

  ButtonList() = default;
  // `viewport` (optional) clips and scrolls the slots; `scrollbarW` reserves a scrollbar strip at its right edge.
  ButtonList(const std::vector<std::string>& labels, std::vector<Rectangle> slots, Rectangle viewport = {},
             float scrollbarW = 0.0f);

  // Returns the index clicked this frame, or -1 (it also becomes `selected` when selectable).
  // `interactive` false: the list is hidden or disabled (it still animates but takes no pointer).
  int update(float dt, bool interactive = true);
  void draw(float alpha = 1.0f) const;
  /// A disabled item looks dimmed and ignores clicks (it still takes the pointer).
  void setEnabled(size_t index, bool enabled) { if (index < items_.size()) items_[index].enabled = enabled; }
  /// Rows of a list (save slots): labels left-aligned, long ones end in "..." instead of overflowing the button.
  void setEllipsize(bool on) { for (Button& b : items_) b.ellipsize = b.leftAligned = on; }
  void setLabel(size_t index, std::string label, std::string detail = {}) {
    if (index < items_.size()) { items_[index].label = std::move(label); items_[index].detail = std::move(detail); }
  }

private:
  std::vector<Button> items_;
  std::vector<Rectangle> slots_;
  Rectangle view_{};
  float scrollbarW_ = 0.0f, scroll_ = 0.0f, maxScroll_ = 0.0f;
  bool dragging_ = false;
  UI::Motion::Spring x_, y_, w_, h_, alpha_;  // the sliding selection indicator
  bool indicatorInit_ = false;

  bool clipped() const { return view_.width > 0.0f; }
  Rectangle itemsArea() const { return {view_.x, view_.y, view_.width - scrollbarW_, view_.height}; }
  Rectangle scrollbar() const { return {view_.x + view_.width - scrollbarW_, view_.y, scrollbarW_, view_.height}; }
  Rectangle handle() const;
  void scrollInput();
};

}
