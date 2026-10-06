#include "play/Feedback.h"
#include <algorithm>
#include <cmath>
#include "Render/Motion.h"

namespace play::feedback {

using namespace UI::Motion;

namespace {
constexpr float kPi = 3.14159265f;
}

const char* reasonText(Intent::Reason reason) {
  switch (reason) {
    case Intent::Reason::NotYourPiece: return "Not your piece";
    case Intent::Reason::HistoryBoard: return "This board is history";
    case Intent::Reason::OtherSidesBoard: return "Not your move on this board";
    case Intent::Reason::ComputerThinking: return "Computer is thinking";
    case Intent::Reason::None: break;
  }
  return "";
}

float shakeOffset(float t, float amplitude) {
  t = clamp01(t);
  const float decay = easeOutCubic(1.0f - t); // stays strong for the first swings, then settles
  return amplitude * decay * std::sin(t * 3.0f * kPi);
}

float dotDelay(float ring) { return std::min(kMaxDotStagger, std::max(0.0f, ring) * staggerDots); }

float checkPulse(float t) {
  t = clamp01(t);
  const float s = 0.5f - 0.5f * std::cos(t * 4.0f * kPi); // 0 at both ends, two peaks
  return s * (1.0f - 0.35f * t);
}

float liftBump(float elapsed, float duration) {
  if (elapsed <= 0.0f) return 0.0f;
  const float t = clamp01(elapsed / std::max(0.001f, duration));
  return kLiftPixels * std::sin(t * kPi);
}

Span unfoldedSpan(float y0, float h, bool fromTop, float p) {
  p = clamp01(p);
  return fromTop ? Span{y0, y0 + h * p} : Span{y0 + h * (1.0f - p), y0 + h};
}

} // namespace play::feedback
