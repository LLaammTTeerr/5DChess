#include <doctest/doctest.h>

#include <cmath>
#include <string>
#include "play/Feedback.h"

using namespace play::feedback;

TEST_CASE("Feedback: the shake starts and ends at rest, swings both ways and decays") {
  CHECK(shakeOffset(0.0f) == doctest::Approx(0.0f));
  CHECK(shakeOffset(1.0f) == doctest::Approx(0.0f).epsilon(0.001));
  float maxFirst = 0.0f, minAll = 0.0f, maxAll = 0.0f, lateMax = 0.0f;
  for (int i = 0; i <= 240; ++i) {
    const float t = i / 240.0f, v = shakeOffset(t);
    if (t < 0.34f) maxFirst = std::fmax(maxFirst, std::fabs(v));
    if (t > 0.67f) lateMax = std::fmax(lateMax, std::fabs(v));
    minAll = std::fmin(minAll, v);
    maxAll = std::fmax(maxAll, v);
    CHECK(std::fabs(v) <= kShakePixels + 1e-4f);
  }
  CHECK(maxAll > 0.5f);
  CHECK(minAll < -0.5f);
  CHECK(lateMax < maxFirst); // decays
  CHECK(shakeOffset(0.2f, 6.0f) == doctest::Approx(2.0f * shakeOffset(0.2f, 3.0f)));
}

TEST_CASE("Feedback: legal-dot stagger is 30 ms per ring and never beyond 150 ms") {
  CHECK(dotDelay(0.0f) == doctest::Approx(0.0f));
  CHECK(dotDelay(1.0f) == doctest::Approx(0.03f));
  CHECK(dotDelay(5.0f) == doctest::Approx(0.15f));
  CHECK(dotDelay(10.0f) == doctest::Approx(kMaxDotStagger));
  CHECK(dotDelay(500.0f) == doctest::Approx(kMaxDotStagger));
  CHECK(dotDelay(-3.0f) == doctest::Approx(0.0f));
}

TEST_CASE("Feedback: the check pulse is two peaks that return to rest") {
  CHECK(checkPulse(0.0f) == doctest::Approx(0.0f));
  CHECK(checkPulse(1.0f) == doctest::Approx(0.0f).epsilon(0.001));
  int peaks = 0;
  float prev = 0.0f, prev2 = 0.0f;
  for (int i = 0; i <= 200; ++i) {
    const float v = checkPulse(i / 200.0f);
    if (i >= 2 && prev > prev2 && prev > v && prev > 0.3f) ++peaks;
    prev2 = prev;
    prev = v;
    CHECK(v >= 0.0f);
    CHECK(v <= 1.0f);
  }
  CHECK(peaks == 2);
}

TEST_CASE("Feedback: a lifted board rises, settles and waits for its stagger") {
  CHECK(liftBump(-0.1f) == doctest::Approx(0.0f));
  CHECK(liftBump(0.0f) == doctest::Approx(0.0f));
  CHECK(liftBump(kHandOverSeconds / 2.0f) == doctest::Approx(kLiftPixels));
  CHECK(liftBump(kHandOverSeconds) == doctest::Approx(0.0f).epsilon(0.001));
  CHECK(liftBump(5.0f) == doctest::Approx(0.0f).epsilon(0.001));
}

TEST_CASE("Feedback: a lane unfolds from the side of its parent") {
  const Span fromTop = unfoldedSpan(100.0f, 200.0f, true, 0.25f);
  CHECK(fromTop.y0 == doctest::Approx(100.0f));
  CHECK(fromTop.y1 == doctest::Approx(150.0f));
  const Span fromBottom = unfoldedSpan(100.0f, 200.0f, false, 0.25f);
  CHECK(fromBottom.y0 == doctest::Approx(250.0f));
  CHECK(fromBottom.y1 == doctest::Approx(300.0f));
  const Span full = unfoldedSpan(100.0f, 200.0f, true, 1.0f);
  CHECK(full.y1 - full.y0 == doctest::Approx(200.0f));
  const Span none = unfoldedSpan(100.0f, 200.0f, false, 0.0f);
  CHECK(none.y1 - none.y0 == doctest::Approx(0.0f));
  CHECK(unfoldedSpan(0.0f, 10.0f, true, 7.0f).y1 == doctest::Approx(10.0f)); // clamped
}

TEST_CASE("Feedback: every refusal has a sentence for the HUD") {
  using R = play::Intent::Reason;
  CHECK(std::string(reasonText(R::NotYourPiece)) == "Not your piece");
  CHECK(std::string(reasonText(R::HistoryBoard)) == "This board is history");
  CHECK(std::string(reasonText(R::OtherSidesBoard)) == "Not your move on this board");
  CHECK(std::string(reasonText(R::ComputerThinking)) == "Computer is thinking");
  CHECK(std::string(reasonText(R::None)).empty());
}
