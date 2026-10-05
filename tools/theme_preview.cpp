// Developer tool: renders a real game through ChessModel/ChessView/ChessController
// with a chosen piece theme and saves a screenshot, then exits.
//
//   theme_preview <Classic|Modern|Fantasy|Pixel> <game mode> <output.png> [turns] [options]
//
// Game mode: "Standard", "Battle", "Invasion", "Fragment", or any in-game mode name.
// With turns > 0 (default 2) a few scripted legal moves are played first, preferring
// moves that jump between boards so extra timelines appear in multi-timeline modes.
//
// Options (motion tooling):
//   --demo same|cross   play one animated move through the controller (select, move, submit) after the
//                       scripted turns; "cross" prefers a board-to-board / time-travel move
//   --dump DIR          write the animated frames as DIR/0000.png ... at 30 fps of simulated time
//   --perf [N]          after the scripted turns render N (default 600) frames with the animation running and
//                       print average / p99 / max frame time (CPU render cost, no vsync)
//   --reduce            enable Reduce motion
#include <raylib.h>
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include <exception>
#include "chess.h"
#include "PieceTheme.h"
#include "ResourceManager.h"
#include "Render/View.h"
#include "Render/UITheme.h"
#include "Render/Controller.h"
#include "Render/RenModel.h"
#include "Render/Motion.h"

namespace {

std::shared_ptr<Chess::IGame> makeGame(const std::string& mode) {
  using namespace Chess;
  if (mode == "Standard" || mode == NameOfGame<StandardGame>::value) return createGame<StandardGame>();
  if (mode == "Battle" || mode == NameOfGame<MiscGameTimeLineBattle>::value) return createGame<MiscGameTimeLineBattle>();
  if (mode == "Invasion" || mode == NameOfGame<MiscGameTimeLineInvasion>::value) return createGame<MiscGameTimeLineInvasion>();
  if (mode == "Fragment" || mode == NameOfGame<MiscGameTimeLineFragment>::value) return createGame<MiscGameTimeLineFragment>();
  return nullptr;
}

// Finds a legal move on any moveable board without applying it. Cross-board moves win when preferCross.
bool findScriptedMove(Chess::IGame& game, bool preferCross, Chess::SelectedPosition& bestFrom, Chess::SelectedPosition& bestTo) {
  using namespace Chess;
  bool haveBest = false, cross = false;
  for (const auto& board : game.getMoveableBoards()) {
    for (int y = 0; y < game.dim() && !cross; ++y) {
      for (int x = 0; x < game.dim() && !cross; ++x) {
        SelectedPosition from(board, Position2D(x, y));
        std::vector<SelectedPosition> targets;
        try { targets = game.getMoveablePositions(from); } catch (const std::exception&) { continue; }
        for (const auto& to : targets) {
          if (std::dynamic_pointer_cast<King>(to.board->getPiece(to.position))) continue;  // keep the game running
          if (!haveBest || to.board != board) { bestFrom = from; bestTo = to; haveBest = true; }
          if (preferCross && to.board != board) { cross = true; break; }
        }
      }
    }
    if (haveBest && (cross || !preferCross)) break;
  }
  return haveBest;
}

// Plays one full turn: every moveable board gets one move; cross-board moves win.
bool playScriptedTurn(Chess::IGame& game) {
  using namespace Chess;
  for (int guard = 0; !game.getMoveableBoards().empty() && guard < 16; ++guard) {
    SelectedPosition bestFrom, bestTo;
    if (!findScriptedMove(game, true, bestFrom, bestTo)) break;
    game.makeMove({bestFrom, bestTo});
    if (game.gameEnd()) { game.undo(); break; }  // safety net: never end the game in a preview
  }
  if (game.undoable()) game.submitTurn();
  return !game.gameEnd();
}

struct FrameClock {
  std::vector<double> ms;
  void add(double v) { ms.push_back(v); }
  void report(const char* label) {
    if (ms.empty()) return;
    std::vector<double> sorted = ms;
    std::sort(sorted.begin(), sorted.end());
    double sum = 0; for (double v : ms) sum += v;
    std::cout << label << ": frames=" << ms.size() << " avg=" << sum / ms.size() << " ms  p99="
              << sorted[static_cast<size_t>(sorted.size() * 0.99)] << " ms  max=" << sorted.back() << " ms\n";
  }
};

}  // namespace

int main(int argc, char** argv) {
  if (argc < 4) {
    std::cerr << "usage: theme_preview <Classic|Modern|Fantasy|Pixel> <Standard|Battle|Invasion|Fragment> <out.png> [turns]"
                 " [--demo same|cross] [--dump DIR] [--perf [N]] [--reduce]\n";
    return 2;
  }
  const std::string theme = argv[1], mode = argv[2];
  std::string out = argv[3];
  int turns = 2;
  std::string demo, dumpDir;
  int perfFrames = 0;
  bool reduce = false;
  for (int i = 4; i < argc; ++i) {
    if (!std::strcmp(argv[i], "--demo") && i + 1 < argc) demo = argv[++i];
    else if (!std::strcmp(argv[i], "--dump") && i + 1 < argc) dumpDir = argv[++i];
    else if (!std::strcmp(argv[i], "--perf")) { perfFrames = (i + 1 < argc && argv[i + 1][0] != '-') ? std::atoi(argv[++i]) : 600; }
    else if (!std::strcmp(argv[i], "--reduce")) reduce = true;
    else if (argv[i][0] != '-') turns = std::atoi(argv[i]);
  }

  // Make the output paths absolute before we change into the asset directory.
  auto absolute = [](std::string p) {
    if (!p.empty() && p[0] != '/' && (p.size() < 2 || p[1] != ':')) p = std::string(GetWorkingDirectory()) + "/" + p;
    return p;
  };
  out = absolute(out);
  if (!dumpDir.empty()) dumpDir = absolute(dumpDir);

  SetConfigFlags(FLAG_MSAA_4X_HINT);
  InitWindow(1400, 800, "theme_preview");
  ChangeDirectory(GetApplicationDirectory());  // assets are copied next to the binary
  UI::Motion::setReduced(reduce);

  int rc = 0;
  {
    ResourceManager& resources = ResourceManager::getInstance();
    ThemeManager& themes = ThemeManager::getInstance();
    if (theme == "Classic") themes.setTheme(std::make_unique<ClassicTheme>());
    else if (theme == "Modern") themes.setTheme(std::make_unique<ModernTheme>());
    else if (theme == "Fantasy") themes.setTheme(std::make_unique<Modern2Theme>());
    else if (theme == "Pixel") themes.setTheme(std::make_unique<PixelTheme>());
    else { std::cerr << "unknown theme: " << theme << "\n"; rc = 2; }

    auto game = rc == 0 ? makeGame(mode) : nullptr;
    if (rc == 0 && !game) { std::cerr << "unknown game mode: " << mode << "\n"; rc = 2; }

    if (rc == 0) {
      for (int i = 0; i < turns; ++i)
        if (!playScriptedTurn(*game)) break;
      std::cout << "timelines: " << game->timeLineCount() << ", half-turn: " << game->presentHalfTurn() << "\n";

      ChessModel model(game);
      ChessView view(Vector3{5000, 5000, 1});
      ChessController controller(model, view);
      controller.update(0.0f);
      const float dt = 1.0f / 60.0f;
      auto frame = [&](bool dump, int index) {
        controller.update(dt);
        BeginDrawing();
        ClearBackground(UI::Color::bg);
        controller.render();
        EndDrawing();
        if (dump) {
          Image shot = LoadImageFromScreen();
          char name[64];
          std::snprintf(name, sizeof name, "/%04d.png", index);
          ExportImage(shot, (dumpDir + name).c_str());
          UnloadImage(shot);
        }
      };
      // Let the camera settle exactly like it would in the running game.
      for (int i = 0; i < 400; ++i) frame(false, 0);

      if (!demo.empty() || perfFrames > 0) {
        using namespace Chess;
        SelectedPosition from, to;
        const bool wantCross = demo != "same";
        if (!findScriptedMove(*game, wantCross, from, to)) { std::cerr << "no legal move for the demo\n"; rc = 1; }
        else {
          std::cout << "demo move: timeline " << from.board->timeLineId() << " -> " << to.board->timeLineId()
                    << (to.board != from.board ? " (cross-board)" : " (same board)") << "\n";
          FrameClock idle, anim;
          int n = 0;
          const bool dumping = !dumpDir.empty();
          auto run = [&](float seconds, FrameClock& clock) {
            const int frames = static_cast<int>(seconds * 60.0f);
            for (int i = 0; i < frames; ++i, ++n) {
              const auto t0 = std::chrono::steady_clock::now();
              frame(dumping && (n % 2 == 0), n / 2);
              clock.add(std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count());
            }
          };
          run(0.8f, idle);                              // idle
          controller.scriptedSelect(from);              // pick up the piece: lift + dots pop
          run(1.3f, anim);
          controller.scriptedSelect(to);                // move: piece travels, new board(s) grow, arrow draws
          run(1.8f, anim);
          // Finish the turn with scripted moves on any other boards, then submit: turn banner
          for (int guard = 0; !game->getMoveableBoards().empty() && guard < 16; ++guard) {
            SelectedPosition f2, t2;
            if (!findScriptedMove(*game, wantCross, f2, t2)) break;
            controller.scriptedSelect(f2);
            run(0.5f, anim);
            controller.scriptedSelect(t2);
            run(0.9f, anim);
          }
          if (game->undoable() && game->getMoveableBoards().empty()) controller.scriptedSubmit();
          run(1.6f, anim);
          if (perfFrames > 0) {
            // A busy stretch: keep re-triggering selection + moves is out of scope, so just measure the steady state
            FrameClock steady;
            for (int i = 0; i < perfFrames; ++i) {
              const auto t0 = std::chrono::steady_clock::now();
              frame(false, 0);
              steady.add(std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count());
            }
            steady.report("steady ");
          }
          idle.report("idle   ");
          anim.report("animated");
        }
      }
      // TakeScreenshot() writes under the (platform dependent) base path, so export explicitly.
      Image shot = LoadImageFromScreen();
      if (!ExportImage(shot, out.c_str())) { std::cerr << "failed to write " << out << "\n"; rc = 1; }
      else std::cout << "wrote " << out << "\n";
      UnloadImage(shot);
    }
    UI::Fonts::unloadAll();
    resources.unloadAll();
  }
  CloseWindow();
  return rc;
}
