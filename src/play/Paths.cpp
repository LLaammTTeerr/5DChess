#include "play/Paths.h"
#include <algorithm>
#include <cmath>

namespace play::path {

namespace {
float dist(Vector2 a, Vector2 b) { return std::hypot(b.x - a.x, b.y - a.y); }
Vector2 lerp(Vector2 a, Vector2 b, float t) { return {a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t}; }
} // namespace

float Poly::length() const {
  float total = 0.0f;
  for (int i = 0; i + 1 < n; ++i) total += dist(p[i], p[i + 1]);
  return total;
}

Vector2 Poly::at(float fraction) const {
  if (n == 0) return {0, 0};
  float remaining = std::clamp(fraction, 0.0f, 1.0f) * length();
  for (int i = 0; i + 1 < n; ++i) {
    const float len = dist(p[i], p[i + 1]);
    if (remaining <= len && len > 0.0f) return lerp(p[i], p[i + 1], remaining / len);
    remaining -= len;
  }
  return p[n - 1];
}

void bezier(Poly& out, Vector2 p0, Vector2 c0, Vector2 c1, Vector2 p1, int segments) {
  out.n = 0;
  segments = std::min(segments, Poly::kMax - 1);
  for (int i = 0; i <= segments; ++i) {
    const float t = static_cast<float>(i) / segments, u = 1.0f - t;
    out.add({u * u * u * p0.x + 3 * u * u * t * c0.x + 3 * u * t * t * c1.x + t * t * t * p1.x,
             u * u * u * p0.y + 3 * u * u * t * c0.y + 3 * u * t * t * c1.y + t * t * t * p1.y});
  }
}

void elbow(Poly& out, Vector2 a, Vector2 b, float xMid, float radius) {
  out.n = 0;
  const float dy = b.y - a.y;
  if (std::fabs(dy) < 1.0f) {
    out.add(a);
    out.add(b);
    return;
  }
  const float s = dy > 0 ? 1.0f : -1.0f;
  const float r = std::min({radius, std::fabs(dy) / 2.0f, std::fabs(xMid - a.x), std::fabs(b.x - xMid)});
  auto corner = [&](Vector2 from, Vector2 control, Vector2 to) { // quadratic bezier as the rounded corner
    for (int i = 0; i <= 6; ++i) {
      const float t = i / 6.0f, u = 1.0f - t;
      out.add({u * u * from.x + 2 * u * t * control.x + t * t * to.x, u * u * from.y + 2 * u * t * control.y + t * t * to.y});
    }
  };
  out.add(a);
  corner({xMid - r, a.y}, {xMid, a.y}, {xMid, a.y + s * r});
  corner({xMid, b.y - s * r}, {xMid, b.y}, {xMid + r, b.y});
  out.add(b);
}

void stroke(const Poly& poly, float fraction, Color color, float thickness, float dash, float gap) {
  if (poly.n < 2 || fraction <= 0.0f) return;
  const float limit = std::min(fraction, 1.0f) * poly.length();
  float walked = 0.0f, phase = 0.0f; // phase: distance into the current dash + gap cycle
  const float cycle = dash + gap;
  for (int i = 0; i + 1 < poly.n && walked < limit; ++i) {
    const Vector2 a = poly.p[i], b = poly.p[i + 1];
    const float len = dist(a, b);
    if (len <= 0.0f) continue;
    const float end = std::min(len, limit - walked);
    if (dash <= 0.0f) {
      DrawLineEx(a, lerp(a, b, end / len), thickness, color);
      if (thickness > 2.5f) DrawCircleV(lerp(a, b, end / len), thickness / 2.0f, color); // round joint
    } else {
      float s = 0.0f;
      while (s < end) {
        const float inCycle = std::fmod(phase + s, cycle);
        const float run = inCycle < dash ? dash - inCycle : cycle - inCycle; // to the end of the dash / gap
        const float next = std::min(end, s + std::max(run, cycle * 0.01f)); // always advance (fmod can land a hair below a boundary)
        if (inCycle < dash) DrawLineEx(lerp(a, b, s / len), lerp(a, b, next / len), thickness, color);
        s = next;
      }
      phase = std::fmod(phase + end, cycle);
    }
    walked += len;
  }
}

} // namespace play::path
