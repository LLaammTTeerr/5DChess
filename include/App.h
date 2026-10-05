#pragma once
#include <functional>

class SceneManager;

namespace App {
// One iteration of the game loop: the single place for per-frame work. Shared by src/main.cpp and the UI
// test harness so both run the same update/render sequence. `beforePresent` (optional) runs after the
// scene has been drawn and before EndDrawing(), e.g. to grab a screenshot of the finished frame.
void frame(SceneManager& scenes, const std::function<void()>& beforePresent = {});
}
