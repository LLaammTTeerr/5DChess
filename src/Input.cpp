#include "Input.h"
#include "TestMode.h"
#include <algorithm>

namespace Input {

Scripted& scripted() { static Scripted s; return s; }

namespace {
bool inRange(int b) { return b >= 0 && b < 3; }
}

Vector2 mousePosition() { return TestMode::get().active ? scripted().position : GetMousePosition(); }
Vector2 mouseDelta() { return TestMode::get().active ? scripted().delta : GetMouseDelta(); }
bool mousePressed(int b) {
  if (!TestMode::get().active) return IsMouseButtonPressed(b);
  return inRange(b) && scripted().pressed[b];
}
bool mouseDown(int b) {
  if (!TestMode::get().active) return IsMouseButtonDown(b);
  return inRange(b) && scripted().down[b];
}
float mouseWheel() { return TestMode::get().active ? scripted().wheel : GetMouseWheelMove(); }
bool shiftDown() {
  if (TestMode::get().active) return scripted().shift;
  return IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
}
bool keyPressed(int k) {
  if (!TestMode::get().active) return IsKeyPressed(k);
  const auto& v = scripted().keysPressed;
  return std::find(v.begin(), v.end(), k) != v.end();
}
float frameTime() {
  const TestMode& t = TestMode::get();
  return t.fixedStep ? t.fixedDt : GetFrameTime();
}
double time() {
  const TestMode& t = TestMode::get();
  return t.fixedStep ? t.clock : GetTime();
}

}
