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
//   mode <id>            replace the current screen with the game screen of a catalog mode (e.g. standard)
//   position <file>      same, for a .5dp position file; relative paths are relative to the script's directory
//   record <file>        same, for a game record (.5dr, see docs/NOTATION.md): the game replayed from it
//   slot <n> <file>      put the text of a file (a record, or a corrupted one) into save slot n (1-3; 0 is the autosave) of the in-memory store the
//                        harness uses instead of the config directory; the Load screen then lists it
//   (the first three take effect on the next frame: follow them with `wait`)
//   ainodes <n>          the computer opponent's search runs exactly n nodes a frame (so the frame at which it finishes is reproducible;
//                        400 until changed); 0 freezes a running search where it is, to capture the "thinking" HUD; `ainodes clock` runs it
//                        against the wall-clock budget as shipped (not reproducible frame by frame; for FDCHESS_PERF measurements); see TestMode::aiNodesPerFrame
//   waitai               (also on a puzzle: until its judging, proof and replies are done) run frames until the computer has nothing to do (it has moved, or the game ended; at most 5000 frames, then the
//                        script fails). Does nothing when the game is not against the computer
//   clicksq <l> <t> <sq>  click a square of the game screen (or the Guide's page) by name wherever the camera has put it: timeline id l,
//                        half-turn t of the board (0 = White's first), square like e2 (file a..h, rank 1..8)
//   hoversq <l> <t> <sq>  the same, but only move the pointer onto the square (hover previews, live arcs)
//   (movesq is an alias of hoversq)
//   clickcard <l> <t> clicks the label strip of a board's card
//   dblclick <x> <y>     two clicks on consecutive frames (a double-click: the camera toggles Focus <-> Overview)
//   dblclicksq <l> <t> <sq>  the same on a square, by name
//   drag <x0> <y0> <x1> <y1> [steps]  press at x0,y0, move to x1,y1 in `steps` (6) frames with the button held, release: pans the camera
//   zoom?                prints the camera (state, zoom, target, moving) to the log; takes no frame. The camera of the game screen / Guide / puzzle
//   camlog on|off        print the camera every frame from here on (frame number, state, zoom, target)
//   camstill on|off      from here on every frame must leave the camera exactly where `camstill on` found it (zoom and target): a
//                        change fails the script. Checks "the camera does not move by itself mid-turn"
//   camexpect state <Overview|Focus|Free>   fail the script unless the camera is in that framing (takes no frame)
//   camexpect squareat <l> <t> <sq> <x> <y>|pointer  fail unless that square's centre is within 1.5 px of x,y (or of the pointer: zoom around it)
//   camexpect nextboard <0|1>               fail unless the "a Mandatory board is off-screen" state matches
//   camexpect zoom <min> <max>              fail unless the zoom is within [min, max]
//   camexpect visible <l> <t> [fraction]    fail unless that board's card is at least `fraction` (default 1) inside the free area
//   (key names: A..Z, 0..9, F1..F12, ESCAPE, SPACE, ENTER, TAB, SHIFT_TAB, HOME, PLUS, MINUS, BACKSPACE, UP, DOWN, LEFT, RIGHT, DELETE)
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
#include <optional>
#include <set>
#include <vector>
#include <filesystem>
#include "App.h"
#include "Input.h"
#include "TestMode.h"
#include "engine/GameCatalog.h"
#include "engine/Notation.h"
#include "engine/Position.h"
#include "Screens/GuideScreen.h"
#include "Screens/PlayScreen.h"
#include "play/VsAi.h"
#include "Screens/PuzzleScreen.h"
#include "services/SaveStore.h"
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
  bool shift = false;   // a Shift key is held this frame
  std::string capture;  // non-empty: export the finished frame under this name
  bool clickSquare = false;   // the pointer goes to a square of the game screen (resolved when the frame runs)
  bool cardStrip = false;     // ... or to the label strip of a board's card (sqL, sqT)
  int sqL = 0, sqT = 0, sqX = 0, sqY = 0;
  std::string mode, position; // non-empty: open the game screen of that catalog mode / .5dp file before this frame
  std::string record;         // non-empty: same for a game record file
  bool waitAi = false;        // run frames until the computer opponent is quiet
  int aiNodes = -2;           // >= -1: set TestMode::aiNodesPerFrame before this frame (-1: the wall-clock budget)
  std::string slotFile;       // non-empty: fill save slot `slotNo` with this file's text before this frame
  int slotNo = 0;
  std::string cam;            // non-empty: a camera command that takes no frame (zoom?, camlog, camstill, camexpect), with its arguments
  int line = 0;
};

const std::map<std::string, int>& keyTable() {
  static const std::map<std::string, int> t = [] {
    std::map<std::string, int> m = {
        {"ESCAPE", KEY_ESCAPE}, {"ESC", KEY_ESCAPE}, {"SPACE", KEY_SPACE}, {"ENTER", KEY_ENTER},
        {"TAB", KEY_TAB}, {"BACKSPACE", KEY_BACKSPACE}, {"UP", KEY_UP}, {"DOWN", KEY_DOWN}, {"HOME", KEY_HOME},
        {"PLUS", KEY_EQUAL}, {"MINUS", KEY_MINUS}, {"SHIFT_TAB", KEY_TAB},
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
      f.shift = name == "SHIFT_TAB";
      out.push_back(f);
    } else if (cmd == "clicksq") {
      std::string sq;
      if (!(ss >> f.sqL >> f.sqT >> sq) || sq.size() != 2 || sq[0] < 'a' || sq[0] > 'h' || sq[1] < '1' || sq[1] > '8')
        return fail("clicksq needs <timeline> <half-turn> <square, e.g. e2>");
      f.sqX = sq[0] - 'a';
      f.sqY = sq[1] - '1';
      f.clickSquare = true;
      f.setPos = true;
      out.push_back(f);       // hover first (the position is filled in when the frame runs)
      FrameSpec p; p.line = n; p.press = true;
      out.push_back(p);
      FrameSpec r; r.line = n; r.release = true;
      out.push_back(r);
    } else if (cmd == "hoversq" || cmd == "movesq") {
      std::string sq;
      if (!(ss >> f.sqL >> f.sqT >> sq) || sq.size() != 2 || sq[0] < 'a' || sq[0] > 'h' || sq[1] < '1' || sq[1] > '8')
        return fail("hoversq needs <timeline> <half-turn> <square, e.g. e2>");
      f.sqX = sq[0] - 'a';
      f.sqY = sq[1] - '1';
      f.clickSquare = true;
      f.setPos = true;
      out.push_back(f);
    } else if (cmd == "clickcard") {
      if (!(ss >> f.sqL >> f.sqT)) return fail("clickcard needs <timeline> <half-turn>");
      f.cardStrip = true;
      f.setPos = true;
      out.push_back(f);       // hover first
      FrameSpec p; p.line = n; p.press = true;
      out.push_back(p);
      FrameSpec r; r.line = n; r.release = true;
      out.push_back(r);
    } else if (cmd == "dblclick" || cmd == "dblclicksq") {
      std::string sq;
      if (cmd == "dblclick") {
        float x, y;
        if (!(ss >> x >> y)) return fail("dblclick needs x y");
        f.setPos = true; f.pos = {x, y};
      } else {
        if (!(ss >> f.sqL >> f.sqT >> sq) || sq.size() != 2 || sq[0] < 'a' || sq[0] > 'h' || sq[1] < '1' || sq[1] > '8')
          return fail("dblclicksq needs <timeline> <half-turn> <square, e.g. e2>");
        f.sqX = sq[0] - 'a';
        f.sqY = sq[1] - '1';
        f.clickSquare = true;
        f.setPos = true;
      }
      out.push_back(f);       // hover first
      for (int i = 0; i < 2; ++i) {
        FrameSpec p; p.line = n; p.press = true;
        out.push_back(p);
        FrameSpec r; r.line = n; r.release = true;
        out.push_back(r);
      }
    } else if (cmd == "drag") {
      float x0, y0, x1, y1;
      int steps = 6;
      if (!(ss >> x0 >> y0 >> x1 >> y1)) return fail("drag needs x0 y0 x1 y1 [steps]");
      if (int v; ss >> v) steps = v;
      if (steps < 1) return fail("drag needs at least one step");
      f.setPos = true; f.pos = {x0, y0};
      out.push_back(f);       // hover
      FrameSpec p; p.line = n; p.press = true;
      out.push_back(p);
      for (int i = 1; i <= steps; ++i) {
        FrameSpec m; m.line = n; m.setPos = true;
        m.pos = {x0 + (x1 - x0) * i / steps, y0 + (y1 - y0) * i / steps};
        out.push_back(m);
      }
      FrameSpec r; r.line = n; r.release = true;
      out.push_back(r);
    } else if (cmd == "zoom?") {
      f.cam = "zoom?";
      out.push_back(f);
    } else if (cmd == "camlog" || cmd == "camstill") {
      std::string arg;
      if (!(ss >> arg) || (arg != "on" && arg != "off")) return fail("needs on or off");
      f.cam = cmd + " " + arg;
      out.push_back(f);
    } else if (cmd == "camexpect") {
      std::string rest;
      std::getline(ss, rest);
      if (rest.empty()) return fail("camexpect needs state / zoom / visible");
      f.cam = "camexpect" + rest;
      out.push_back(f);
    } else if (cmd == "mode") {
      if (!(ss >> f.mode)) return fail("mode needs a catalog id");
      out.push_back(f);
    } else if (cmd == "position") {
      if (!(ss >> f.position)) return fail("position needs a file");
      out.push_back(f);
    } else if (cmd == "record") {
      if (!(ss >> f.record)) return fail("record needs a file");
      out.push_back(f);
    } else if (cmd == "slot") {
      if (!(ss >> f.slotNo >> f.slotFile) || f.slotNo < 0 || f.slotNo > savegame::kSlots) return fail("slot needs <0-3> <file>");
      out.push_back(f);
    } else if (cmd == "ainodes") {
      std::string arg;
      if (!(ss >> arg)) return fail("ainodes needs a node count (0 freezes, clock = wall-clock budget as shipped)");
      if (arg == "clock") f.aiNodes = -1;
      else if (arg.find_first_not_of("0123456789") == std::string::npos && arg.size() < 8) f.aiNodes = std::stoi(arg);
      else return fail("ainodes needs a node count (0 freezes, clock = wall-clock budget as shipped)");
      out.push_back(f);
    } else if (cmd == "waitai") {
      f.waitAi = true;
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
  tm.aiNodesPerFrame = 400;

  SetTraceLogLevel(LOG_WARNING);
  SetConfigFlags(FLAG_MSAA_4X_HINT);
  InitWindow(kWidth, kHeight, "5D Chess UI script");
  TestMode::apply();  // after InitWindow: raylib reseeds its RNG from the clock there
  SetExitKey(KEY_NULL);
  ChangeDirectory(GetApplicationDirectory());  // assets are copied next to the binary

  int rc = 0;
  {
    App app;  // same context as src/main.cpp (default Pixel theme, audio stays silent in test mode)

    Input::Scripted& in = Input::scripted();
    bool camLog = false, camStill = false;
    PlayScreen::CameraInfo stillRef{};
    long frameNo = 0;
    auto playScreen = [&]() -> PlayScreen* {
      Screen* top = app.screens.top();
      if (auto* guide = dynamic_cast<GuideScreen*>(top)) return guide->board();
      if (auto* puzzle = dynamic_cast<PuzzleScreen*>(top)) return puzzle->board();
      return dynamic_cast<PlayScreen*>(top);
    };
    auto camText = [](const PlayScreen::CameraInfo& c) {
      std::ostringstream o;
      o << "state=" << c.state << " zoom=" << c.zoom << " target=(" << c.x << "," << c.y << ") moving=" << (c.moving ? 1 : 0);
      return o.str();
    };
    // After every frame: the per-frame camera log, and the camstill check (false: the camera moved, the script fails)
    auto afterFrame = [&](int line) {
      ++frameNo;
      if (!camLog && !camStill) return true;
      PlayScreen* play = playScreen();
      if (!play) return true;
      const PlayScreen::CameraInfo c = play->cameraInfo();
      if (camLog) std::cout << "cam frame " << frameNo << " (line " << line << "): " << camText(c) << "\n";
      if (camStill && (c.zoom != stillRef.zoom || c.x != stillRef.x || c.y != stillRef.y)) {
        std::cerr << scriptPath.string() << ":" << line << ": camstill: the camera moved on its own at frame " << frameNo << ": "
                  << camText(stillRef) << " -> " << camText(c) << "\n";
        rc = 2;
        return false;
      }
      return true;
    };
    Vector2 prev = in.position;
    int captured = 0;
    for (const FrameSpec& f : frames) {
      if (WindowShouldClose() || app.quit) break;
      if (!f.slotFile.empty()) {
        std::ifstream file(scriptPath.parent_path() / f.slotFile, std::ios::binary);
        if (!file) { std::cerr << scriptPath.string() << ":" << f.line << ": cannot open " << f.slotFile << "\n"; rc = 2; break; }
        std::ostringstream text;
        text << file.rdbuf();
        app.saves.storage().write(f.slotNo == 0 ? "autosave" : "slot" + std::to_string(f.slotNo), text.str());
      }
      if (!f.mode.empty() || !f.position.empty() || !f.record.empty()) {
        std::shared_ptr<Chess::IGame> game;
        std::optional<play::VsAi> vs;
        try {
          if (!f.mode.empty()) game = Chess::GameCatalog::create(f.mode);
          else if (!f.record.empty()) {
            std::ifstream file(scriptPath.parent_path() / f.record, std::ios::binary);
            std::ostringstream text;
            text << file.rdbuf();
            game = Chess::loadRecord(text.str());
            vs = play::findMeta(text.str()); // a vs-Computer record restores its mode, like Load and Continue
          }
          else game = Chess::Core::loadPositionFile((scriptPath.parent_path() / f.position).string()).makeGame();
        } catch (const std::exception& e) {
          std::cerr << scriptPath.string() << ":" << f.line << ": " << e.what() << "\n";
          rc = 2;
        }
        if (!game) { if (rc == 0) std::cerr << scriptPath.string() << ":" << f.line << ": unknown mode " << f.mode << "\n"; rc = 2; break; }
        app.screens.replace(std::make_unique<PlayScreen>(game, false, vs));
      }
      if (!f.cam.empty()) { // camera commands: no frame
        PlayScreen* play = playScreen();
        auto bad = [&](const std::string& why) {
          std::cerr << scriptPath.string() << ":" << f.line << ": " << why << "\n";
          rc = 2;
        };
        if (!play) { bad("camera commands need the game screen, the Guide or a puzzle"); break; }
        const PlayScreen::CameraInfo c = play->cameraInfo();
        std::istringstream args(f.cam);
        std::string cmd, a1;
        args >> cmd >> a1;
        if (cmd == "zoom?") std::cout << "zoom? line " << f.line << ": " << camText(c) << "\n";
        else if (cmd == "camlog") camLog = a1 == "on";
        else if (cmd == "camstill") { camStill = a1 == "on"; stillRef = c; }
        else if (cmd == "camexpect") {
          if (a1 == "state") {
            std::string want;
            args >> want;
            if (want != c.state) bad("camexpect state " + want + ": the camera is in " + c.state + " (" + camText(c) + ")");
          } else if (a1 == "zoom") {
            float lo = 0, hi = 0;
            args >> lo >> hi;
            if (c.zoom < lo || c.zoom > hi) bad("camexpect zoom " + std::to_string(lo) + ".." + std::to_string(hi) + ": " + camText(c));
          } else if (a1 == "squareat") {
            int l = 0, t = 0;
            std::string sq, word;
            float x = 0, y = 0;
            args >> l >> t >> sq >> word;
            if (word == "pointer") { x = in.position.x; y = in.position.y; }
            else { x = std::stof(word); args >> y; }
            const Chess::Core::Coord coord{static_cast<int8_t>(sq[0] - 'a'), static_cast<int8_t>(sq[1] - '1'), static_cast<int16_t>(t), static_cast<int16_t>(l)};
            const Vector2 at = play->squareToScreen(coord);
            if (std::abs(at.x - x) > 1.5f || std::abs(at.y - y) > 1.5f)
              bad("camexpect squareat " + std::to_string(l) + " " + std::to_string(t) + " " + sq + ": the square is at (" + std::to_string(at.x) + "," + std::to_string(at.y) + "), not (" + std::to_string(x) + "," + std::to_string(y) + ")");
          } else if (a1 == "nextboard") {
            int want = 0;
            args >> want;
            if ((play->nextMandatoryOffscreen() ? 1 : 0) != want) bad("camexpect nextboard " + std::to_string(want) + ": it is " + std::to_string(want ? 0 : 1));
          } else if (a1 == "visible") {
            int l = 0, t = 0;
            float fraction = 1.0f;
            args >> l >> t;
            if (!(args >> fraction)) fraction = 1.0f;
            if (!play->boardVisible(l, t, fraction)) bad("camexpect visible " + std::to_string(l) + " " + std::to_string(t) + ": the board is not in view (" + camText(c) + ")");
          } else {
            bad("unknown camexpect");
          }
        }
        if (rc != 0) break;
        continue;
      }
      if (f.aiNodes >= -1) tm.aiNodesPerFrame = f.aiNodes;
      if (f.waitAi) {
        in.pressed[0] = false;
        in.wheel = 0.0f;
        in.keysPressed.clear();
        in.delta = {0, 0};
        int frames = 0;
        for (;;) {
          Screen* top = app.screens.top();
          bool busy = false;
          if (auto* play = dynamic_cast<PlayScreen*>(top)) busy = play->aiBusy();
          else if (auto* puzzle = dynamic_cast<PuzzleScreen*>(top)) busy = puzzle->busy(); // a puzzle's judging, proof and replies
          if (!busy) break;
          if (++frames > 5000) { std::cerr << scriptPath.string() << ":" << f.line << ": waitai: the computer is still busy after 5000 frames\n"; rc = 2; break; }
          app.frame();
          if (!afterFrame(f.line)) break;
        }
        if (rc != 0) break;
        continue;
      }
      Vector2 target = f.pos;
      if (f.cardStrip) {
        PlayScreen* play = playScreen();
        if (!play) { std::cerr << scriptPath.string() << ":" << f.line << ": clickcard needs the game screen or the Guide\n"; rc = 2; break; }
        if (!play->game().boardExists({0, 0, static_cast<int16_t>(f.sqT), static_cast<int16_t>(f.sqL)})) {
          std::cerr << scriptPath.string() << ":" << f.line << ": no board at timeline " << f.sqL << ", half-turn " << f.sqT << "\n";
          rc = 2;
          break;
        }
        target = play->cardStripToScreen(f.sqL, f.sqT);
      } else if (f.clickSquare) {
        auto* play = dynamic_cast<PlayScreen*>(app.screens.top());
        if (auto* guide = dynamic_cast<GuideScreen*>(app.screens.top())) play = guide->board(); // the Guide's page
        if (auto* puzzle = dynamic_cast<PuzzleScreen*>(app.screens.top())) play = puzzle->board(); // a puzzle's board
        if (!play) { std::cerr << scriptPath.string() << ":" << f.line << ": clicksq needs the game screen or the Guide\n"; rc = 2; break; }
        const Chess::Core::Coord c{static_cast<int8_t>(f.sqX), static_cast<int8_t>(f.sqY), static_cast<int16_t>(f.sqT), static_cast<int16_t>(f.sqL)};
        if (!play->game().boardExists(c)) { std::cerr << scriptPath.string() << ":" << f.line << ": no board at timeline " << f.sqL << ", half-turn " << f.sqT << "\n"; rc = 2; break; }
        target = play->squareToScreen(c);
      }
      in.pressed[0] = f.press;
      if (f.press) in.down[0] = true;
      if (f.release) in.down[0] = false;
      if (f.setPos) in.position = target;
      in.delta = {in.position.x - prev.x, in.position.y - prev.y};
      prev = in.position;
      in.wheel = f.wheel;
      in.keysPressed.clear();
      if (f.key) in.keysPressed.push_back(f.key);
      in.shift = f.shift;

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
      if (!afterFrame(f.line)) break;
    }
    std::cout << "ui_script: " << frames.size() << " frames, " << captured << " captures -> " << outDir.string() << "\n";
  }
  CloseWindow();
  return rc;
}
