#pragma once
#include <string>

// Pure text-fitting helpers (no graphics: the caller passes the measuring function, raylib's MeasureTextEx in the game and a fixed
// advance in the unit tests).
namespace ui {

/// `text` unchanged when it fits `maxWidth`, else the longest whole-UTF-8-character prefix that ends in "..." and still fits.
/// `measure(const std::string&)` returns the width in pixels. When not even "..." fits the result is just "..." (never a hang).
template <class Measure>
std::string ellipsized(const std::string& text, float maxWidth, Measure&& measure) {
  if (measure(text) <= maxWidth) return text;
  std::string shown = text;
  while (!shown.empty()) {
    size_t i = shown.size(); // the start of the last character (a lead byte is not 10xxxxxx)
    do --i; while (i > 0 && (static_cast<unsigned char>(shown[i]) & 0xC0) == 0x80);
    shown.resize(i);
    if (measure(shown + "...") <= maxWidth) break;
  }
  return shown + "...";
}

} // namespace ui
