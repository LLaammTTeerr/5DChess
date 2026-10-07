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

} // namespace ui::audit
