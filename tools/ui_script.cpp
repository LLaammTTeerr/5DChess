// ui_script: runs the REAL game (same ScreenStack / screens as src/main.cpp, via App::frame) from a
// script file with deterministic injected input, and saves screenshots. Safety net for UI refactors.
//
//   ui_script <script.ui> <outdir>
//
// Script: one command per line, '#' starts a comment.
//   wait <frames>        advance that many frames with no input
//   move <x> <y>         put the pointer at x,y (one frame)
//   click <x> <y>        move there, press (one frame), release (one frame)
//   wheel <dy>           mouse wheel delta for one frame
//   key <KEYNAME>        press a key for one frame: A..Z, 0..9, ESCAPE, SPACE, ENTER, TAB, BACKSPACE, UP, ...
//   capture <name>       render one more frame and write <outdir>/<name>.png (1400x800)
//
// Determinism: fixed 1/60 s timestep, Reduce motion forced on, audio off, RNG seeded, scripted input
// (see include/TestMode.h and include/Input.h).
#include <raylib.h>
#include <rlgl.h>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <set>
#include <vector>
#include <filesystem>
#include "App.h"
#include "Input.h"
#include "TestMode.h"
#include "PieceTheme.h"
#include "Render/UITheme.h"

namespace {

constexpr int kWidth = 1400, kHeight = 800;

struct FrameSpec {
  bool setPos = false;
  Vector2 pos{0, 0};
  bool press = false;   // left button goes down this frame
  bool release = false; // left button goes up this frame
  float wheel = 0.0f;
  int key = 0;          // KEY_* or 0
  std::string capture;  // non-empty: export the finished frame under this name
  int line = 0;
};

const std::map<std::string, int>& keyTable() {
  static const std::map<std::string, int> t = [] {
    std::map<std::string, int> m = {
        {"ESCAPE", KEY_ESCAPE}, {"ESC", KEY_ESCAPE}, {"SPACE", KEY_SPACE}, {"ENTER", KEY_ENTER},
        {"TAB", KEY_TAB}, {"BACKSPACE", KEY_BACKSPACE}, {"UP", KEY_UP}, {"DOWN", KEY_DOWN},
        {"LEFT", KEY_LEFT}, {"RIGHT", KEY_RIGHT}, {"DELETE", KEY_DELETE},
    };
    for (char c = 'A'; c <= 'Z'; ++c) m[std::string(1, c)] = KEY_A + (c - 'A');
    for (char c = '0'; c <= '9'; ++c) m[std::string(1, c)] = KEY_ZERO + (c - '0');
    for (int i = 1; i <= 12; ++i) m["F" + std::to_string(i)] = KEY_F1 + (i - 1);
    return m;
  }();
  return t;
}

bool parseScript(const std::string& path, std::vector<FrameSpec>& out) {
  std::ifstream in(path);
  if (!in) { std::cerr << "ui_script: cannot open " << path << "\n"; return false; }
  std::string line;
  int n = 0;
  while (std::getline(in, line)) {
    ++n;
    const size_t hash = line.find('#');
    if (hash != std::string::npos) line.erase(hash);
    std::istringstream ss(line);
    std::string cmd;
    if (!(ss >> cmd)) continue;
    auto fail = [&](const char* why) { std::cerr << path << ":" << n << ": " << why << ": " << line << "\n"; return false; };
    FrameSpec f; f.line = n;
    if (cmd == "wait") {
      int k = 0;
      if (!(ss >> k) || k < 0) return fail("wait needs a frame count");
      for (int i = 0; i < k; ++i) out.push_back(f);
    } else if (cmd == "move") {
      float x, y;
      if (!(ss >> x >> y)) return fail("move needs x y");
      f.setPos = true; f.pos = {x, y};
      out.push_back(f);
    } else if (cmd == "click") {
      float x, y;
      if (!(ss >> x >> y)) return fail("click needs x y");
      f.setPos = true; f.pos = {x, y};
      out.push_back(f);       // hover first
      FrameSpec p; p.line = n; p.press = true;
      out.push_back(p);
      FrameSpec r; r.line = n; r.release = true;
      out.push_back(r);
    } else if (cmd == "wheel") {
      float d;
      if (!(ss >> d)) return fail("wheel needs dy");
      f.wheel = d;
      out.push_back(f);
    } else if (cmd == "key") {
      std::string name;
      if (!(ss >> name)) return fail("key needs a name");
      auto it = keyTable().find(name);
      if (it == keyTable().end()) return fail("unknown key name");
      f.key = it->second;
      out.push_back(f);
    } else if (cmd == "capture") {
      if (!(ss >> f.capture)) return fail("capture needs a name");
      out.push_back(f);
    } else {
      return fail("unknown command");
    }
  }
  return true;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc != 3) {
    std::cerr << "usage: ui_script <script.ui> <outdir>\n";
    return 2;
  }
  namespace fs = std::filesystem;
  const fs::path scriptPath = fs::absolute(argv[1]);
  const fs::path outDir = fs::absolute(argv[2]);
  std::vector<FrameSpec> frames;
  if (!parseScript(scriptPath.string(), frames)) return 2;
  {
    std::set<std::string> seen;
    for (const FrameSpec& f : frames)
      if (!f.capture.empty() && !seen.insert(f.capture).second) {
        std::cerr << "duplicate capture name '" << f.capture << "' in " << scriptPath.string() << "\n";
        return 2;
      }
  }
  std::error_code ec;
  fs::create_directories(outDir, ec);

  // Everything the game reads from the outside world is pinned before the first frame.
  TestMode& tm = TestMode::get();
  tm.active = true;
  tm.fixedStep = true;
  tm.reduceMotion = true;
  tm.audioDisabled = true;
  tm.seed = 5;

  SetTraceLogLevel(LOG_WARNING);
  SetConfigFlags(FLAG_MSAA_4X_HINT);
  InitWindow(kWidth, kHeight, "5D Chess UI script");
  TestMode::apply();  // after InitWindow: raylib reseeds its RNG from the clock there
  SetExitKey(KEY_NULL);
  ChangeDirectory(GetApplicationDirectory());  // assets are copied next to the binary

  int rc = 0;
  {
    App app;  // same context as src/main.cpp (default Modern theme, audio stays silent in test mode)

    Input::Scripted& in = Input::scripted();
    Vector2 prev = in.position;
    int captured = 0;
    for (const FrameSpec& f : frames) {
      if (WindowShouldClose() || app.quit) break;
      in.pressed[0] = f.press;
      if (f.press) in.down[0] = true;
      if (f.release) in.down[0] = false;
      if (f.setPos) in.position = f.pos;
      in.delta = {in.position.x - prev.x, in.position.y - prev.y};
      prev = in.position;
      in.wheel = f.wheel;
      in.keysPressed.clear();
      if (f.key) in.keysPressed.push_back(f.key);

      app.frame([&] {
        if (f.capture.empty()) return;
        rlDrawRenderBatchActive();  // raylib batches draw calls until EndDrawing: flush so the read-back sees them
        Image shot = LoadImageFromScreen();
        if (shot.width != kWidth || shot.height != kHeight)
          std::cerr << "ui_script: warning: screenshot is " << shot.width << "x" << shot.height << ", expected " << kWidth << "x" << kHeight << "\n";
        const std::string file = (outDir / (f.capture + ".png")).string();
        if (!ExportImage(shot, file.c_str())) { std::cerr << "ui_script: failed to write " << file << "\n"; rc = 1; }
        else ++captured;
        UnloadImage(shot);
      });
    }
    std::cout << "ui_script: " << frames.size() << " frames, " << captured << " captures -> " << outDir.string() << "\n";
  }
  CloseWindow();
  return rc;
}
