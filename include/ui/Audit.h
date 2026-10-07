#pragma once
#include <raylib.h>
#include <string>

// Layout audit, active only under the UI test harness (TestMode::active): every text that is drawn through a widget, the HUD or a
// panel reports its measured size and the rectangle it must fit in. Anything that does not fit is printed once to stderr as a line
// starting with "UI-OVERFLOW" (tests/ui/run.sh fails when a script log holds one). Shipped builds pay one branch per text.
namespace ui::audit {

bool enabled();
/// A text of `width` x `height` pixels (as MeasureTextEx measures it) must lie inside `box`. `what` names the kind of element.
void fit(const char* what, const std::string& text, float width, float height, Rectangle box);
/// A drawn text occupying `placed` (its position and measured size) must lie inside `bounds`.
void within(const char* what, const std::string& text, Rectangle placed, Rectangle bounds);
/// A label that had to be drawn smaller than its nominal size (still readable, but worth a look).
void shrunk(const char* what, const std::string& text, float size, float nominal);

// ---- Overlap audit ---------------------------------------------------------------------------------------------------------
// Chrome and HUD elements register their screen-space rectangle as they draw; at the end of the frame every pair that intersects
// (or comes closer than `gap`, 1 px unless a rect asks for more) is printed once as "UI-OVERLAP <a> vs <b>", unless a rule below
// allows it. tests/ui/run.sh fails on any such line. Callers build the name only after checking enabled().
enum class Kind {
  Button,   // a button (not the rows of a scrolling list: the list's viewport stands for them)
  Pill,     // the status pill, the controls bar
  Badge,    // a count / marker sitting on something (the Undo count, the Present marker)
  Panel,    // a card or side panel that holds controls (a button inside it is fine)
  Popup,    // a drop-down panel drawn over the scene (the Save panel): covers scene and ruler on purpose, never HUD controls
  List,     // the viewport of a scrolling list
  Text,     // a title or a footer line
  Ruler,    // the turn ruler
  Card,     // a board card, history stack or the inspector of the Play view (the buttons of its tab column lie inside it)
  Band,     // the top edge of a lane band: where the scene begins (only checked against the HUD zone and the ruler)
  PresentBand, // the top edge of the present column: hangs from the ruler, so it may lie inside it (but never in the HUD zone)
  HudZone   // [0, hudBottom): nothing of the scene (Ruler, Band) may enter it
};
/// Registers `r` for this frame's overlap check. `gap`: the clear space this element needs around it.
void rect(const std::string& name, Rectangle r, Kind kind, float gap = 1.0f);
/// Start of a frame's drawing (clears the registered rects) and its end (checks them).
void beginFrame();
void endFrame();
/// Drops what was registered (a snapshot of the outgoing screen is drawn outside the frame's drawing).
void discard();

} // namespace ui::audit
