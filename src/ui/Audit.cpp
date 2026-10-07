#include "ui/Audit.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <set>
#include <vector>
#include "TestMode.h"

namespace ui::audit {

namespace {
// How many texts were checked (printed once at exit, so a log shows the audit really ran)
struct Counter {
  long checked = 0;
  long rects = 0, frames = 0;
  ~Counter() {
    if (checked > 0) std::cerr << "UI-AUDIT " << checked << " texts checked" << std::endl;
    if (rects > 0) std::cerr << "UI-AUDIT-OVERLAP " << rects << " rects over " << frames << " frames" << std::endl;
  }
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

// ---- Overlap audit ----------------------------------------------------------------------------------------------------------

namespace {
struct Entry { std::string name; Rectangle r; Kind kind; float gap; };
std::vector<Entry> g_entries;

enum class Rule { Overlap, Contain };  // Overlap: any intersection is fine; Contain: fine only when the inner rect lies wholly inside the outer
struct Allowed { Kind outer, inner; Rule rule; };
// The explicit exceptions. Everything not listed here must stay clear of everything else.
constexpr Allowed kAllowed[] = {
    {Kind::Panel, Kind::Button, Rule::Contain}, {Kind::Panel, Kind::List, Rule::Contain},   {Kind::Panel, Kind::Text, Rule::Contain},
    {Kind::Popup, Kind::Button, Rule::Contain}, {Kind::Popup, Kind::List, Rule::Contain},
    {Kind::HudZone, Kind::Button, Rule::Contain}, {Kind::HudZone, Kind::Pill, Rule::Contain}, {Kind::HudZone, Kind::Badge, Rule::Overlap},
    {Kind::HudZone, Kind::Popup, Rule::Overlap},  // the Save panel drops out of the HUD zone
    {Kind::Ruler, Kind::Badge, Rule::Contain},  // the Present marker sits on the ruler
    {Kind::Ruler, Kind::Panel, Rule::Contain},  // the minimap at its right end
    {Kind::Ruler, Kind::PresentBand, Rule::Contain},  // the present column hangs from the ruler
    {Kind::Ruler, Kind::Popup, Rule::Overlap}, {Kind::Badge, Kind::Popup, Rule::Overlap},  // the Save panel drops over the ruler
    {Kind::Button, Kind::Badge, Rule::Overlap}, // the Undo count on its button
};

bool contains(Rectangle outer, Rectangle inner) {
  constexpr float e = 0.5f;
  return inner.x >= outer.x - e && inner.y >= outer.y - e && inner.x + inner.width <= outer.x + outer.width + e &&
         inner.y + inner.height <= outer.y + outer.height + e;
}

bool scene(Kind k) { return k == Kind::Band || k == Kind::PresentBand; }

bool direct(const Entry& a, const Entry& b) {
  for (const Allowed& rule : kAllowed) {
    for (int swap = 0; swap < 2; ++swap) {
      const Entry& outer = swap ? b : a;
      const Entry& inner = swap ? a : b;
      if (outer.kind != rule.outer || inner.kind != rule.inner) continue;
      if (rule.rule == Rule::Overlap || contains(outer.r, inner.r)) return true;
    }
  }
  return false;
}

// What is drawn inside a popup (the Save panel's slots) is over the scene and the ruler like the popup itself, but must still keep
// clear of the other controls.
bool allowed(const Entry& a, const Entry& b) {
  if (direct(a, b)) return true;
  for (const Entry& p : g_entries) {
    if (p.kind != Kind::Popup) continue;
    if (&p != &a && contains(p.r, a.r) && direct(p, b)) return true;
    if (&p != &b && contains(p.r, b.r) && direct(a, p)) return true;
  }
  return false;
}

bool tooClose(const Entry& a, const Entry& b) {
  const float g = std::max(a.gap, b.gap);
  const float ix = std::min(a.r.x + a.r.width + g, b.r.x + b.r.width) - std::max(a.r.x - g, b.r.x);
  const float iy = std::min(a.r.y + a.r.height + g, b.r.y + b.r.height) - std::max(a.r.y - g, b.r.y);
  return ix > 0.5f && iy > 0.5f;
}
} // namespace

void rect(const std::string& name, Rectangle r, Kind kind, float gap) {
  if (!enabled() || r.width <= 0.0f || r.height <= 0.0f) return;
  g_entries.push_back({name, r, kind, gap});
}

void beginFrame() { g_entries.clear(); }
void discard() { g_entries.clear(); }

void endFrame() {
  if (!enabled()) return;
  ++g_counter.frames;
  g_counter.rects += static_cast<long>(g_entries.size());
  for (size_t i = 0; i < g_entries.size(); ++i)
    for (size_t j = i + 1; j < g_entries.size(); ++j) {
      const Entry& a = g_entries[i];
      const Entry& b = g_entries[j];
      // The scene (lane bands, the present column) is only checked where it meets the HUD zone and the ruler; the rest of the screen
      // legitimately lies over it
      if ((scene(a.kind) && b.kind != Kind::HudZone && b.kind != Kind::Ruler) || (scene(b.kind) && a.kind != Kind::HudZone && a.kind != Kind::Ruler)) continue;
      if (!tooClose(a, b) || allowed(a, b)) continue;
      const bool order = a.name <= b.name;
      const Entry& x = order ? a : b;
      const Entry& y = order ? b : a;
      const auto box = [](const Rectangle& r) {
        return std::to_string(static_cast<int>(r.x)) + "," + std::to_string(static_cast<int>(r.y)) + " " + std::to_string(static_cast<int>(r.width)) + "x" +
               std::to_string(static_cast<int>(r.height));
      };
      static std::set<std::string> seen;  // once per pair of names; the rects shown are those of its first frame
      if (seen.insert(x.name + " vs " + y.name).second)
        std::cerr << "UI-OVERLAP " << x.name << " vs " << y.name << " (" << box(x.r) << " / " << box(y.r) << ")" << std::endl;
    }
  g_entries.clear();
}

} // namespace ui::audit
