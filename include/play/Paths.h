#pragma once
#include <raylib.h>

namespace play::path {

/// A short polyline on the stack (no allocation per frame): the curves and elbows between boards.
struct Poly {
  static constexpr int kMax = 64;
  Vector2 p[kMax];
  int n = 0;
  void add(Vector2 v) { if (n < kMax) p[n++] = v; }
  float length() const;
  /// The point at `fraction` (0..1) of the length.
  Vector2 at(float fraction) const;
};

/// Cubic bezier through four points.
void bezier(Poly& out, Vector2 p0, Vector2 c0, Vector2 c1, Vector2 p1, int segments = 24);
/// Horizontal - vertical - horizontal with rounded corners (a subway line); the vertical run is at x = xMid.
void elbow(Poly& out, Vector2 a, Vector2 b, float xMid, float radius);

/// Strokes the first `fraction` of the polyline. dash > 0: dashes of `dash` followed by gaps of `gap`.
void stroke(const Poly& poly, float fraction, Color color, float thickness, float dash = 0.0f, float gap = 0.0f);

} // namespace play::path
