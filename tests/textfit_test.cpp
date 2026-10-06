#include <doctest/doctest.h>

#include <string>
#include "ui/TextFit.h"

namespace {
// A fixed-advance "font": every character (a UTF-8 sequence counts once) is 10 px wide
float measure(const std::string& s) {
  float n = 0;
  for (unsigned char c : s)
    if ((c & 0xC0) != 0x80) n += 10.0f;
  return n;
}
} // namespace

TEST_CASE("ui::ellipsized: text that fits is untouched") {
  CHECK(ui::ellipsized("Slot 1: Empty", 130.0f, measure) == "Slot 1: Empty");
  CHECK(ui::ellipsized("", 0.0f, measure).empty());
}

TEST_CASE("ui::ellipsized: a long text is cut to the longest prefix that fits with its dots") {
  // 12 characters, room for 8: five of the text and the three dots
  const std::string cut = ui::ellipsized("abcdefghijkl", 80.0f, measure);
  CHECK(cut == "abcde...");
  CHECK(measure(cut) <= 80.0f);
}

TEST_CASE("ui::ellipsized: whole UTF-8 characters are cut, never half of one") {
  const std::string text = "ab\xC2\xB7" "cd\xC2\xB7" "ef"; // "ab.cd.ef" with U+00B7 (middle dot)
  for (float room = 0.0f; room < 100.0f; room += 5.0f) {
    const std::string cut = ui::ellipsized(text, room, measure);
    CHECK(!cut.empty());
    CHECK(((static_cast<unsigned char>(cut.front()) & 0xC0) != 0x80)); // never starts inside a character
    for (size_t i = 1; i < cut.size(); ++i) // a continuation byte only directly after its lead (or an earlier continuation)
      if ((static_cast<unsigned char>(cut[i]) & 0xC0) == 0x80)
        CHECK(((static_cast<unsigned char>(cut[i - 1]) & 0x80) != 0));
  }
  CHECK(ui::ellipsized(text, 60.0f, measure) == "ab\xC2\xB7...");
}

TEST_CASE("ui::ellipsized: no room at all still ends (the result is just the dots)") {
  CHECK(ui::ellipsized("anything", 5.0f, measure) == "...");
}
