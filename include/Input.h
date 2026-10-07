#pragma once
#include <raylib.h>
#include <vector>

// Single entry point for everything the game reads from the outside world: pointer, keyboard and clock.
// Normally these forward to raylib. When TestMode::active is set they read a scripted state that the
// UI test harness fills once per frame (raylib has no input injection).
namespace Input {

struct Scripted {
  Vector2 position{0, 0};
  Vector2 delta{0, 0};
  bool down[3] = {false, false, false};     // left, right, middle held this frame
  bool pressed[3] = {false, false, false};  // went down this frame
  float wheel = 0.0f;
  std::vector<int> keysPressed;             // KEY_* pressed this frame
  bool shift = false;                       // a Shift key is held this frame
};
Scripted& scripted();

Vector2 mousePosition();
Vector2 mouseDelta();
bool mousePressed(int button);
bool mouseDown(int button);
float mouseWheel();
bool keyPressed(int key);
// Seconds the previous frame took (fixed in test mode) and seconds since start.
/// A Shift key is held.
bool shiftDown();
float frameTime();
double time();

}
