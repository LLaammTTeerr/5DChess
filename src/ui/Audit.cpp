#include "ui/Audit.h"
#include <iostream>
#include <set>
#include "TestMode.h"

namespace ui::audit {

namespace {
// How many texts were checked (printed once at exit, so a log shows the audit really ran)
struct Counter {
  long checked = 0;
  ~Counter() { if (checked > 0) std::cerr << "UI-AUDIT " << checked << " texts checked" << std::endl; }
};
Counter g_counter;

void report(const std::string& line, const char* tag = "UI-OVERFLOW") {
  static std::set<std::string> seen;
  if (seen.insert(std::string(tag) + line).second) std::cerr << tag << ' ' << line << std::endl;
}
} // namespace

bool enabled() { return TestMode::get().active; }

void fit(const char* what, const std::string& text, float width, float height, Rectangle box) {
  if (!enabled() || text.empty()) return;
  ++g_counter.checked;
  constexpr float kSlack = 0.75f; // sub-pixel rounding of the glyph advances
  if (width <= box.width + kSlack && height <= box.height + kSlack) return;
  report(std::string(what) + " '" + text + "' is " + std::to_string(static_cast<int>(width)) + "x" + std::to_string(static_cast<int>(height)) +
         " in a " + std::to_string(static_cast<int>(box.width)) + "x" + std::to_string(static_cast<int>(box.height)) + " box");
}

void within(const char* what, const std::string& text, Rectangle placed, Rectangle bounds) {
  if (!enabled() || text.empty()) return;
  ++g_counter.checked;
  constexpr float kSlack = 0.75f;
  if (placed.x >= bounds.x - kSlack && placed.y >= bounds.y - kSlack && placed.x + placed.width <= bounds.x + bounds.width + kSlack &&
      placed.y + placed.height <= bounds.y + bounds.height + kSlack)
    return;
  report(std::string(what) + " '" + text + "' at " + std::to_string(static_cast<int>(placed.x)) + "," + std::to_string(static_cast<int>(placed.y)) +
         " size " + std::to_string(static_cast<int>(placed.width)) + "x" + std::to_string(static_cast<int>(placed.height)) + " leaves its " +
         std::to_string(static_cast<int>(bounds.width)) + "x" + std::to_string(static_cast<int>(bounds.height)) + " bounds at " +
         std::to_string(static_cast<int>(bounds.x)) + "," + std::to_string(static_cast<int>(bounds.y)));
}

void shrunk(const char* what, const std::string& text, float size, float nominal) {
  if (!enabled() || size >= nominal - 0.01f) return;
  report(std::string(what) + " '" + text + "' drawn at " + std::to_string(static_cast<int>(size)) + "px instead of " +
         std::to_string(static_cast<int>(nominal)) + "px", "UI-SHRUNK");
}

} // namespace ui::audit
